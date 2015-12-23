/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include "opts.hpp"
#include "paratec.h"
#include "results.hpp"
#include "std.hpp"
#include "time.hpp"

namespace pt
{

class Test : _paratec
{
	sp<const Opts> opts_;
	sp<Results> res_;

	bool enabled_ = true;
	bool skipped_ = false;
	std::string stdout_;
	std::string stderr_;

	void clean();
	void flush(int fd, std::string *to);

public:
	Test(const _paratec &p) : _paratec(p)
	{
	}

	/**
	 * Give the test everything it needs to run
	 */
	void prepare(sp<const Opts> opts, sp<Results> res);

	/**
	 * If this test needs to run
	 */
	inline bool enabled() const
	{
		return this->enabled_;
	}

	/**
	 * This test's specific timeout
	 */
	inline double timeout() const
	{
		return _paratec::timeout ?: this->opts_->timeout_.get();
	}

	/**
	 * Run the test. If the test is not enabled, this just adds a skipped
	 * result to Results and returns.
	 */
	void run();

	/**
	 * Capture stdout/stderr
	 */
	void flushPipes(int stdout, int stderr);

	/**
	 * Cleanup after a forked test.
	 *
	 * @param start
	 *     When the test was started
	 * @param status
	 *     Exit status from waiting on the process
	 * @param timedout
	 *     If killed by timeout
	 * @param assertsOK
	 *     If no test assertions failed
	 */
	void
	cleanupFork(time::point start, int status, bool timedout, bool assertsOK);

	/**
	 * Cleanup after a no-fork test.
	 *
	 * @param start
	 *     When the test was started
	 * @param assertsOK
	 *     If no test assertions failed
	 */
	void cleanupNoFork(time::point start, bool assertsOK);
};
}
