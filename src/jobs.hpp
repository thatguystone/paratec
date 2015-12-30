/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <setjmp.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "fork.hpp"
#include "results.hpp"
#include "test.hpp"
#include "time.hpp"
#include "util.hpp"

namespace pt
{

/**
 * Global info used for the currently-running test.
 */
struct SharedJob {
	/**
	 * Set this in sub-class (so that mmap may be used as necessary).
	 */
	TestEnv *env_;

	virtual ~SharedJob() = default;

	/**
	 * Environment-dependent test exiting.
	 */
	[[noreturn]] virtual void exit(int status) = 0;
};

class Job
{
protected:
	sp<const Opts> opts_;
	sp<Results> rslts_;
	SharedJob *sj_;

	/**
	 * Current test being given some love
	 */
	sp<const Test> test_;

	/**
	 * Results for this test
	 */
	Result res_;

	/**
	 * When the current test started
	 */
	time::point start_;

	/**
	 * Reset the job for the given test. Returns false if the test is disabled
	 * and should be skipped.
	 */
	bool prep(sp<const Test> test);

	/**
	 * Execute the job
	 */
	void execute();

	/**
	 * Finish the test run
	 */
	void finish();

	/**
	 * Record the test result
	 */
	inline void recordResult()
	{
		this->rslts_->record(*this->sj_->env_, std::move(this->res_));
	}

public:
	Job(sp<const Opts> opts, sp<Results> rslts, SharedJob *sj)
		: opts_(std::move(opts)), rslts_(std::move(rslts)), sj_(sj)
	{
	}

	virtual ~Job() = default;

	/**
	 * Run the test.
	 */
	virtual bool run(sp<const Test> test) = 0;
};

struct BasicSharedJob : public SharedJob {
	jmp_buf jmp_;
	TestEnv te_;
	std::thread::id thid_;

	BasicSharedJob() : thid_(std::this_thread::get_id())
	{
		this->env_ = &this->te_;
	}

	[[noreturn]] void exit(int status) override;
};

/**
 * For non-forking jobs
 */
class BasicJob : public Job
{
	BasicSharedJob sj_;

public:
	BasicJob(sp<const Opts> opts, sp<Results> rslts)
		: Job(std::move(opts), std::move(rslts), &sj_)
	{
	}

	/**
	 * Run and cleanup the test.
	 */
	bool run(sp<const Test> test) override;
};

class ForkingSharedJob : public SharedJob
{
	SharedMem<TestEnv> shm_;

public:
	ForkingSharedJob()
	{
		this->env_ = this->shm_.get();
	}

	[[noreturn]] void exit(int status) override;
};

/**
 * For isolated jobs
 */
class ForkingJob : public Job
{
	ForkingSharedJob sj_;

	/**
	 * Forked subprocess
	 */
	sp<Fork> fork_;

	/**
	 * When the test should time out at
	 */
	time::point timeout_after_;

	void flush(int fd, std::string *to);

public:
	ForkingJob(sp<const Opts> opts, sp<Results> rslts)
		: Job(std::move(opts), std::move(rslts), &sj_)
	{
	}

	inline pid_t pid() const
	{
		if (this->fork_ != nullptr) {
			return this->fork_->pid();
		}

		return -1;
	}

	/**
	 * Run this test.
	 */
	bool run(sp<const Test> test) override;

	/**
	 * Flush the test's pipes
	 */
	void flushPipes();

	/**
	 * Check if this job has timed out. If it has, the job cleans itself up.
	 */
	bool checkTimeout(time::point now);

	/**
	 * The test finished. Clean him up.
	 */
	void cleanup();

	/**
	 * The test finished with a status (from waitpid)
	 */
	void cleanupStatus(int status);

	/**
	 * Terminate this test
	 */
	void terminate();
};

/**
 * Forking job runner
 */
class Jobs
{
	sp<const Opts> opts_;
	sp<Results> rslts_;
	size_t testI_ = 0;
	std::vector<sp<const Test>> tests_;
	std::vector<ForkingJob> jobs_;

	/**
	 * Run the next test in the given job
	 */
	void runNextTest(Job *job);

	/**
	 * Kill any timed-out tests
	 */
	void checkTimeouts();

	/**
	 * Flush all test output
	 */
	void flushPipes();

public:
	/**
	 * Run this many jobs in parallel
	 */
	Jobs(sp<const Opts> opts,
		 sp<Results> rslts,
		 std::vector<sp<const Test>> tests);

	/**
	 * Prematurely terminate all jobs. Only used from a signal handler to
	 * clean up children before exiting.
	 */
	void terminate();

	/**
	 * Run all tests
	 */
	void run();
};
}
