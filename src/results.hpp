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

namespace pt
{

/**
 * Result of a single test run
 */
struct Result {
	/**
	 * Name of the test that generated this result
	 */
	std::string name_;

	/**
	 * The test passed with no problems
	 */
	bool passed_ = false;

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
	 * If this test was disabled
	 */
	bool disabled_ = false;

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

	std::string stdout_;
	std::string stderr_;

	Result(std::string name, time::point start = time::point{})
		: name_(std::move(name))
	{
		if (start != time::point{}) {
			this->duration_ = time::toSeconds(time::now() - start);
		}
	}

	/**
	 * For sorting
	 */
	bool operator<(const Result &o)
	{
		return this->name_ < o.name_;
	}

	/**
	 * Print a summary of this result
	 */
	void dump(sp<const Opts> opts) const;
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
	std::vector<Result> results_;

public:
	Results(sp<Opts> opts) : opts_(std::move(opts))
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
	void record(Result r, bool print_status);

	/**
	 * Check if all tests are done running
	 */
	inline bool done()
	{
		return this->finished_ == this->total_;
	}

	/**
	 * Exit code to use
	 */
	inline int exitCode()
	{
		return this->passes_ == this->enabled_ ? 0 : 1;
	}

	/**
	 * Dump a summary of all tests
	 */
	void dump();
};
}
