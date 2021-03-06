/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stack>
#include <sys/file.h>
#include <sys/wait.h>
#include "err.hpp"
#include "jobs.hpp"
#include "signal.hpp"
#include "time.hpp"

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

static uint32_t _nearestPow10(uint32_t n)
{
	uint32_t i;
	uint32_t res = 1;
	uint32_t tens = 0;

	while (n >= 10) {
		n = n / 10;
		tens++;
	}

	for (i = 0; i < tens; i++) {
		res *= 10;
	}

	return res;
}

static uint32_t _roundUp(uint32_t n)
{
	static const uint32_t is[] = { 1, 2, 3, 5 };

	uint32_t i;
	uint32_t base = _nearestPow10(n);

	for (i = 0; i < NELS(is); i++) {
		if (n <= (is[i] * base)) {
			return is[i] * base;
		}
	}

	return 10 * base;
}

// This was pretty much lifted from Golang's benchmarking
void Job::runBench()
{
	const auto max_dur = time::toDuration(this->opts_->bench_dur_.get());
	static constexpr uint32_t kMmaxBenchIters = 1000000000;

	uint32_t n = 1;
	uint32_t last_n = 0;

	uint64_t ns_op = 0;
	time::duration dur{ 0 };

	while (n < kMmaxBenchIters && dur < max_dur) {
		last_n = n;

		dur = this->test_->bench(n);
		ns_op = time::toNanoSeconds(dur) / n;

		if (ns_op == 0) {
			n = kMmaxBenchIters;
		} else {
			n = (uint32_t)(time::toNanoSeconds(max_dur) / ns_op);
		}

		n = std::max(std::min(n + n / 5, 100 * last_n), last_n + 1);
		n = _roundUp(n);
	}

	this->sj_->env_->bench_iters_ = last_n;
	this->sj_->env_->bench_ns_op_ = ns_op;
}

bool Job::prep(sp<const Test> test)
{
	this->test_ = std::move(test);
	this->sj_->env_->reset(this->id_, this->test_->name(),
						   this->test_->funcName());
	this->res_.reset(this->test_);

	if (!this->test_->enabled()) {
		this->finish();
		return false;
	}

	this->start_ = time::now();

	return true;
}

void Job::execute()
{
	if (!this->test_->isBenchmark()) {
		this->test_->run();
		return;
	}

	this->runBench();
}

void Job::finish()
{
	if (this->test_->enabled()) {
		this->test_->cleanup();
		this->res_.duration_ = time::toSeconds(time::now() - this->start_);
	}

	this->recordResult();
	this->test_ = nullptr;
}

void BasicSharedJob::_exit(int status)
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
			   this->env_->last_mark_, this->env_->fail_msg_);

		fflush(stdout);
		::exit(status);
	}

	longjmp(this->jmp_, 1);
}

bool BasicJob::run(sp<const Test> test)
{
	std::string underline;

	if (!this->prep(std::move(test))) {
		return false;
	}

	_jobs.push(&this->sj_);

	if (setjmp(this->sj_.jmp_) == 0) {
		std::string head("Running: ");
		head += this->test_->name();
		underline
			= std::move(std::string(std::max(head.size(), (size_t)70), '='));

		printf("%s\n", head.c_str());
		printf("%s\n\n", underline.c_str());
		this->execute();
	}

	this->finish();

	printf("\n%s\n", underline.c_str());

	_jobs.pop();

	return true;
}

void ForkingSharedJob::_exit(int status)
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

bool ForkingJob::run(sp<const Test> test)
{
	if (!this->prep(std::move(test))) {
		return false;
	}

	this->fork_ = mksp<Fork>();

	bool parent = this->fork_->fork(this->opts_->capture_, true);

	if (parent) {
		this->timeout_after_ = this->start_
							   + time::toDuration(this->test_->timeout());
		return true;
	}

	// Don't need to pop() the sj: this is a forked test, so the process exits
	// and it doesn't matter.
	_jobs.push(&this->sj_);

	this->execute();
	this->sj_.exit(0);
}

void ForkingJob::flushPipes()
{
	// This job might not have a running test. So skip flushing.
	if (this->fork_ != nullptr) {
		this->fork_->flushPipes();
	}
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
	if (this->fork_ != nullptr) {
		this->fork_->moveOuts(&this->res_.stdout_, &this->res_.stderr_);
	}

	this->finish();
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
		this->fork_->terminate(nullptr);
	}
}

void Jobs::runNextTest(Job *job)
{
	while (this->testI_ < this->tests_.size()) {
		auto i = this->testI_++;
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
	const auto jobs = this->opts_->jobs_.get();

	uint i;

	if (_bin.size() == 0) {
		_bin = this->opts_->bin_name_;
	}

	this->jobs_.reserve(jobs);
	for (i = 0; i < jobs; i++) {
		this->jobs_.emplace_back(i, this->opts_, this->rslts_);
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
	job->env_->skipped_ = 1;
	job->exit(0);
}

uint16_t pt_get_port(uint8_t i)
{
	auto job = pt::_jobs.top();
	auto &opts = job->opts_;

	return (uint16_t)((opts->port_.get() + job->env_->id_)
					  + (i * opts->jobs_.get()));
}

const char *pt_get_name()
{
	auto job = pt::_jobs.top();
	return job->env_->test_name_;
}

void pt_set_iter_name(const char *format, ...)
{
	va_list args;
	auto job = pt::_jobs.top();

	va_start(args, format);
	vsnprintf(job->env_->iter_name_, sizeof(job->env_->iter_name_), format,
			  args);
	va_end(args);
}

void _pt_fail(const char *format, ...)
{
	va_list args;
	auto job = pt::_jobs.top();

	va_start(args, format);
	vsnprintf(job->env_->fail_msg_, sizeof(job->env_->fail_msg_), format, args);
	va_end(args);

	fflush(stdout);
	fflush(stderr);

	job->env_->failed_ = true;
	job->exit(255);
}

void _pt_mark(const char *file, const char *func, const size_t line)
{
	auto job = pt::_jobs.top();

	if (strcmp(job->env_->func_name_, func) == 0) {
		*job->env_->last_mark_ = '\0';
		snprintf(job->env_->last_test_mark_, sizeof(job->env_->last_test_mark_),
				 "%s:%zu", file, line);
	} else {
		snprintf(job->env_->last_mark_, sizeof(job->env_->last_mark_), "%s:%zu",
				 file, line);
	}
}
}
