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
#include <unistd.h>
#include <vector>
#include "fork.hpp"
#include "test.hpp"
#include "time.hpp"

namespace pt
{

/**
 * Global info used for the currently-running test.
 */
struct SharedJob {
	/**
	 * May only contain data elements that are safe to share across processes.
	 */
	struct Info {
		static constexpr int kSize = 2048;

		/**
		 * If this test failed
		 */
		bool failed_;

		/**
		 * Name of the test's function (from __func__).
		 */
		char func_name_[kSize];

		/**
		 * Name set by pt_set_iter_name()
		 */
		char iter_name_[kSize];

		/**
		 * Last mark that was hit
		 */
		char last_mark_[kSize];

		/**
		 * Last mark from insize the test function
		 */
		char last_test_mark_[kSize];

		/**
		 * Message to display to user on failure
		 */
		char fail_msg_[kSize * 4];

		void reset();
	};

	/**
	 * Set this in sub-class (so that mmap may be used as necessary).
	 */
	Info *info_;

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

	/**
	 * Current test being given some love
	 */
	sp<Test> test_;

	/**
	 * When the current test started
	 */
	time::point start_;

public:
	Job(sp<const Opts> opts) : opts_(std::move(opts))
	{
	}

	virtual ~Job() = default;

	/**
	 * Run the test.
	 */
	virtual void run(sp<Test> test) = 0;
};

struct BasicSharedJob : public SharedJob {
	jmp_buf jmp_;
	SharedJob::Info sji_;

	BasicSharedJob()
	{
		this->info_ = &sji_;
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
	BasicJob(sp<const Opts> opts) : Job(std::move(opts))
	{
	}

	/**
	 * Run and cleanup the test.
	 */
	void run(sp<Test> test) override;
};

class ForkingSharedJob : public SharedJob
{
	SharedJob::Info *sji_ = nullptr;

public:
	ForkingSharedJob();
	~ForkingSharedJob() override;

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

public:
	ForkingJob(sp<const Opts> opts) : Job(std::move(opts))
	{
	}

	inline pid_t pid()
	{
		if (this->fork_ != nullptr) {
			return this->fork_->pid();
		}

		return -1;
	}

	/**
	 * Run this test.
	 */
	void run(sp<Test> test) override;

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
	void cleanup(int status, bool timedout);

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
	sp<Results> res_;
	size_t testI_ = 0;
	std::vector<sp<Test>> tests_;
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
	Jobs(sp<const Opts> opts, sp<Results> res, std::vector<sp<Test>> tests);

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
