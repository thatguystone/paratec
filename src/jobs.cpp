/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <fcntl.h>
#include <stack>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "err.hpp"
#include "jobs.hpp"
#include "signal.hpp"

namespace pt
{

/**
 * Name of the test binary. Used for flock in forking tests only.
 */
static std::string _bin;

/**
 * Stack of active jobs. It's possible for non-forking tests to run other non-
 * forking tests.
 */
static std::stack<SharedJob *> _jobs;

bool Job::prepRun(sp<const Test> test)
{
	this->test_ = std::move(test);
	this->sj_->info_->reset(this->test_->name(), this->test_->funcName());
	this->res_.reset(this->test_);

	if (!this->test_->enabled()) {
		this->recordResult();
		return false;
	}

	this->start_ = time::now();

	return true;
}

void Job::finishRun()
{
	this->test_->cleanup();
	this->res_.duration_ = time::toSeconds(time::now() - this->start_);
	this->recordResult();

	this->test_ = nullptr;
}

void BasicSharedJob::exit(int)
{
	if (std::this_thread::get_id() != this->thid_) {
		printf("************************************************************\n"
			   "*                          ERROR                           *\n"
			   "*                                                          *\n"
			   "*  Whoa there! You can't make assertions from any thread   *\n"
			   "*  but the testing thread when running in single-process   *\n"
			   "*  mode. The faulty assertion follows.                     *\n"
			   "*                                                          *\n"
			   "************************************************************\n"
			   "\n"
			   "%s : %s\n",
			   this->info_->last_mark_, this->info_->fail_msg_);

		fflush(stdout);
		abort();
	}

	longjmp(this->jmp_, 1);
}

bool BasicJob::run(sp<const Test> test)
{
	if (!this->prepRun(std::move(test))) {
		return false;
	}

	_jobs.push(&this->sj_);

	if (setjmp(this->sj_.jmp_) == 0) {
		this->test_->run();
	}

	this->finishRun();

	_jobs.pop();

	return true;
}

ForkingSharedJob::ForkingSharedJob()
{
	this->ti_
		= (TestInfo *)mmap(NULL, sizeof(*this->ti_), PROT_READ | PROT_WRITE,
						   MAP_ANON | MAP_SHARED, -1, 0);
	OSErr(this->ti_ == nullptr ? -1 : 0, {}, "failed to mmap TestInfo");

	this->info_ = this->ti_;
}

ForkingSharedJob::~ForkingSharedJob()
{
	if (this->ti_ != nullptr) {
		munmap(this->ti_, sizeof(*this->ti_));
	}
}

void ForkingSharedJob::exit(int status)
{
	/*
	 * LLVM's cov is not fork-safe. So if forking and running with llvm cov,
	 * then need to make sure that only 1 process is writing coverage data at
	 * a time: flock is magic for this. With flock, the lock is released when
	 * the process exits (awesome), and it serves as a lock (awesome).
	 *
	 * @see https://llvm.org/bugs/show_bug.cgi?id=20986
	 *
	 * Don't worry about checking if running llvm's cov (that's platform
	 * specific and painful); just flock everything. It won't make too much of
	 * a difference since this is just preventing a very rare race condition.
	 */
	int fd = open(_bin.c_str(), O_RDONLY);
	OSErr(fd, {}, "failed to open paratec binary for reading");
	flock(fd, LOCK_EX);

	::exit(status);
}

void ForkingJob::flush(int fd, std::string *to)
{
	ssize_t err;
	char buff[4096];

	if (fd == -1) {
		return;
	}

	do {
		err = read(fd, buff, sizeof(buff));
		OSErr(err, { EAGAIN, EWOULDBLOCK }, "failed to read from subprocess");

		if (err > 0) {
			to->append(buff, err);
		}
	} while (err == sizeof(buff));
}

bool ForkingJob::run(sp<const Test> test)
{
	if (!this->prepRun(std::move(test))) {
		return false;
	}

	this->fork_ = mksp<Fork>();

	bool parent = this->fork_->fork(this->opts_->capture_);

	if (parent) {
		this->timeout_after_ = this->start_
							   + time::toDuration(this->test_->timeout());
		return true;
	}

	// Don't need to pop() the sj: this is a forked test, so the process exits
	// and it doesn't matter.
	_jobs.push(&this->sj_);

	this->test_->run();
	this->sj_.exit(0);
}

void ForkingJob::flushPipes()
{
	// This job might not have a running test. So skip flushing.
	if (this->fork_ == nullptr) {
		return;
	}

	this->flush(this->fork_->stdout(), &this->stdout_);
	this->flush(this->fork_->stderr(), &this->stderr_);
}

bool ForkingJob::checkTimeout(time::point now)
{
	if (this->test_ == nullptr) {
		return true;
	}

	if (now > this->timeout_after_) {
		this->terminate();
		this->res_.timedout_ = true;
		this->cleanup();
		return true;
	}

	return false;
}

void ForkingJob::cleanup()
{
	this->flushPipes();
	this->res_.stdout_ = std::move(this->stdout_);
	this->res_.stderr_ = std::move(this->stderr_);

	this->finishRun();
	this->fork_ = nullptr;
}

void ForkingJob::cleanupStatus(int status)
{
	if (WIFEXITED(status)) {
		this->res_.exit_status_ = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		this->res_.signal_num_ = WTERMSIG(status);
	}

	this->cleanup();
}

void ForkingJob::terminate()
{
	if (this->fork_ != nullptr) {
		this->fork_->terminate();
	}
}

void Jobs::runNextTest(Job *job)
{
	while (this->testI_ < this->tests_.size()) {
		int i = this->testI_++;
		auto test = this->tests_[i];

		if (job->run(std::move(test))) {
			return;
		}
	}
}

void Jobs::checkTimeouts()
{
	auto now = time::now();

	for (auto &job : this->jobs_) {
		if (job.checkTimeout(now)) {
			this->runNextTest(&job);
		}
	}
}

void Jobs::flushPipes()
{
	if (this->opts_->capture_) {
		for (auto &job : this->jobs_) {
			job.flushPipes();
		}
	}
}

Jobs::Jobs(sp<const Opts> opts,
		   sp<Results> rslts,
		   std::vector<sp<const Test>> tests)
	: opts_(std::move(opts)), rslts_(std::move(rslts)), tests_(std::move(tests))
{
	const int jobs = this->opts_->jobs_.get();

	int i;

	_bin = this->opts_->bin_name_;

	this->jobs_.reserve(jobs);
	for (i = 0; i < jobs; i++) {
		this->jobs_.emplace_back(this->opts_, this->rslts_);
	}
}

void Jobs::terminate()
{
	for (auto &job : this->jobs_) {
		job.terminate();
	}
}

void Jobs::run()
{
	for (auto &job : this->jobs_) {
		this->runNextTest(&job);
	}

	while (!this->rslts_->done()) {
		sig::childWait();
		this->flushPipes();

		while (!this->rslts_->done()) {
			pid_t pid;
			int status;

			pid = waitpid(-1, &status, WNOHANG);
			OSErr(pid, {}, "waitpid() failed");
			if (pid == 0 || (!WIFEXITED(status) && !WIFSIGNALED(status))) {
				break;
			}

			for (auto &job : this->jobs_) {
				if (job.pid() == pid) {
					job.cleanupStatus(status);
					this->runNextTest(&job);
					break;
				}
			}
		}

		this->checkTimeouts();
	}
}
}

extern "C" {
void pt_skip(void)
{
	auto job = pt::_jobs.top();
	job->info_->skipped_ = 1;
	job->exit(0);
}

uint16_t pt_get_port(uint8_t)
{
	return 0;
	// auto job = pt::_jobs.top();
	// return _port + job->id_ + (i * _max_jobs);
}

const char *pt_get_name()
{
	auto job = pt::_jobs.top();
	return job->info_->test_name_;
}

void pt_fail_(const char *format, ...)
{
	va_list args;
	auto job = pt::_jobs.top();

	va_start(args, format);
	vsnprintf(job->info_->fail_msg_, sizeof(job->info_->fail_msg_), format,
			  args);
	va_end(args);

	fflush(stdout);
	fflush(stderr);

	job->info_->failed_ = true;
	job->exit(255);
}

void pt_mark_(const char *file, const char *func, const size_t line)
{
	auto job = pt::_jobs.top();

	if (strcmp(job->info_->func_name_, func) == 0) {
		*job->info_->last_mark_ = '\0';
		snprintf(job->info_->last_test_mark_,
				 sizeof(job->info_->last_test_mark_), "%s:%zu", file, line);
	} else {
		snprintf(job->info_->last_mark_, sizeof(job->info_->last_mark_),
				 "%s:%zu", file, line);
	}
}
}
