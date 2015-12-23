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

void SharedJob::Info::reset()
{
	this->failed_ = false;
	this->iter_name_[0] = '\0';
	this->func_name_[0] = '\0';
	this->last_mark_[0] = '\0';
	this->last_test_mark_[0] = '\0';
	this->fail_msg_[0] = '\0';
}

void BasicSharedJob::exit(int)
{
	longjmp(this->jmp_, 1);
}

void BasicJob::run(sp<Test>)
{
	_jobs.push(&this->sj_);

	if (setjmp(this->sj_.jmp_) == 0) {
		this->start_ = time::now();
		this->test_->run();
	}

	this->test_->cleanupNoFork(this->start_, !this->sj_.info_->failed_);

	_jobs.pop();
}

ForkingSharedJob::ForkingSharedJob()
{
	this->sji_ = (SharedJob::Info *)mmap(NULL, sizeof(*this->sji_),
										 PROT_READ | PROT_WRITE,
										 MAP_ANON | MAP_SHARED, -1, 0);
	OSErr(this->sji_ == nullptr ? -1 : 0, {}, "failed to mmap SharedJob::Info");

	this->info_ = this->sji_;
}

ForkingSharedJob::~ForkingSharedJob()
{
	munmap(this->sji_, sizeof(*this->sji_));
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

void ForkingJob::run(sp<Test> test)
{
	this->test_ = std::move(test);
	this->fork_ = mksp<Fork>();

	bool parent = this->fork_->fork(this->opts_->capture_);

	if (parent) {
		this->start_ = time::now();
		this->timeout_after_ = this->start_
							   + time::toDuration(this->test_->timeout());
		return;
	}

	// Don't need to pop() the sj: this is a forked test, so the process exits
	// and it doesn't matter.
	this->sj_.info_->reset();
	_jobs.push(&this->sj_);

	this->test_->run();
	this->sj_.exit(0);
}

void ForkingJob::flushPipes()
{
	if (this->fork_ == nullptr) {
		return;
	}

	this->test_->flushPipes(this->fork_->stdout(), this->fork_->stderr());
}

bool ForkingJob::checkTimeout(time::point now)
{
	if (this->test_ == nullptr) {
		return true;
	}

	if (now > this->timeout_after_) {
		this->terminate();
		this->cleanup(0, true);
		return true;
	}

	return false;
}

void ForkingJob::cleanup(int status, bool timedout)
{
	this->test_->cleanupFork(this->start_, status, timedout,
							 !this->sj_.info_->failed_);
	this->fork_ = nullptr;
	this->test_ = nullptr;
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

		if (!test->enabled()) {
			test->run();
			continue;
		}

		job->run(std::move(test));
		return;
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

Jobs::Jobs(sp<const Opts> opts, sp<Results> res, std::vector<sp<Test>> tests)
	: opts_(std::move(opts)), res_(std::move(res)), tests_(std::move(tests))
{
	const int jobs = this->opts_->jobs_.get();

	int i;

	_bin = this->opts_->bin_name_;

	for (i = 0; i < jobs; i++) {
		this->jobs_.emplace_back(this->opts_);
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

	while (!this->res_->done()) {
		sig::childWait();

		while (!this->res_->done()) {
			pid_t pid;
			int status;

			pid = waitpid(-1, &status, WNOHANG);
			OSErr(pid, {}, "waitpid() failed");

			// Just in case: only flush after process exits so there's no race
			// condition with getting test output
			this->flushPipes();

			if (pid == 0 || (!WIFEXITED(status) && !WIFSIGNALED(status))) {
				break;
			}

			for (auto &job : this->jobs_) {
				if (job.pid() == pid) {
					job.cleanup(status, false);
					this->runNextTest(&job);
					break;
				}
			}
		}

		this->checkTimeouts();
	}
}
}
