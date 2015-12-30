/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <string>
#include <vector>
#include "opts.hpp"
#include "time.hpp"
#include "std.hpp"
#include "test.hpp"
#include "test_env.hpp"

namespace pt
{

/**
 * Result of a single test run
 */
class Result
{
	sp<const Test> test_;
	time::point start_;

	void dumpOuts(std::ostream &os, bool print) const;
	void
	dumpOut(std::ostream &os, const char *which, const std::string &s) const;

public:
	/**
	 * Name of the test that generated this result. Possibly modified by
	 * pt_set_iter_name().
	 */
	std::string name_;

	/**
	 * The test received a signal or exited with a bad status code
	 */
	bool error_ = false;

	/**
	 * A test assertion failed.
	 */
	bool failed_ = false;

	/**
	 * The test was skipped
	 */
	bool skipped_ = false;

	/**
	 * If the test timed out
	 */
	bool timedout_ = false;

	/**
	 * Last line this test executed
	 */
	std::string last_line_;

	/**
	 * Failure message from an assertions
	 */
	std::string fail_msg_;

	/**
	 * Status code of the test
	 */
	int exit_status_ = 0;

	/**
	 * If the test exited because of a signal
	 */
	int signal_num_ = 0;

	/**
	 * How long it took the test to run
	 */
	double duration_ = 0.0;

	/**
	 * Bench results
	 */
	uint64_t bench_iters_ = 0;
	uint64_t bench_ns_op_ = 0;

	/**
	 * Captured stdout
	 */
	std::string stdout_;

	/**
	 * Captured stderr
	 */
	std::string stderr_;

	/**
	 * If the test for this result was enabled
	 */
	inline bool enabled() const
	{
		return this->test_->enabled();
	}

	/**
	 * Reset and get ready to record a new result
	 */
	void reset(sp<const Test> test);

	/**
	 * For sorting
	 */
	bool operator<(const Result &o) const
	{
		return this->name_ < o.name_;
	}

	/**
	 * Do any result cleanup
	 */
	void finalize(const TestEnv &te, sp<const Opts> opts);

	/**
	 * Print a summary of this result
	 */
	void dump(std::ostream &os, sp<const Opts> opts) const;
};

/**
 * Results of all tests
 */
class Results
{
	size_t enabled_ = 0;
	size_t skipped_ = 0;
	size_t passes_ = 0;
	size_t errors_ = 0;
	size_t failures_ = 0;
	size_t finished_ = 0;
	size_t total_ = 0;
	double tests_duration_ = 0.0;
	time::point start_;
	time::point end_;

	sp<Opts> opts_;
	std::ostream &os_;
	std::vector<Result> results_;

public:
	/**
	 * I don't like that this uses a reference, but C++ was fighting me on
	 * this.
	 */
	Results(sp<Opts> opts, std::ostream &os) : opts_(std::move(opts)), os_(os)
	{
	}

	/**
	 * Start the user duration timer
	 */
	void startTimer();

	/**
	 * Incremenent the test counts
	 */
	void inc(bool enabled);

	/**
	 * Record a test result
	 */
	void record(const TestEnv &te, Result r);

	/**
	 * Check if all tests are done running
	 */
	inline bool done() const
	{
		return this->finished_ == this->total_;
	}

	/**
	 * Exit code to use
	 */
	inline int exitCode() const
	{
		return this->passes_ == this->enabled_ ? 0 : 1;
	}

	/**
	 * Get the result of the test with the given name
	 */
	Result get(const std::string &name);

	/**
	 * Dump a summary of all tests
	 */
	void dump();
};
}
