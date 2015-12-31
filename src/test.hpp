/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <tuple>
#include "opts.hpp"
#include "paratec.h"
#include "std.hpp"
#include "test_env.hpp"

namespace pt
{

/**
 * A description of a test. You'll typically just want to use this as a const
 * guy everywhere.
 */
class Test : public _paratec
{
	std::string name_;
	sp<const Opts> opts_;

	/**
	 * For vector tests
	 */
	int64_t i_ = 0;
	void *vitem_ = nullptr;

	bool enabled_ = true;

public:
	Test(const _paratec &p) : _paratec(p), name_(p.name_)
	{
	}

	Test(const _paratec &p, int64_t i, void *vitem) : Test(p)
	{
		this->i_ = i;
		this->vitem_ = vitem;

		if (this->isRanged()) {
			this->name_ += ':';
			this->name_ += std::to_string(i);
		}
	}

	/**
	 * Create a new test that runs at the given index, bound to the given
	 * options.
	 */
	sp<const Test> bindTo(int64_t i, sp<const Opts> opts) const;

	/**
	 * Human-friendly test name, with index if an iterated test.
	 */
	inline const char *name() const
	{
		return this->name_.c_str();
	}

	/**
	 * Test function name, as reported by __func__
	 */
	inline const char *funcName() const
	{
		return this->fn_name_;
	}

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
		return this->timeout_ > 0 ? this->timeout_
								  : this->opts_->timeout_.get();
	}

	/**
	 * Check if this test operates on a range
	 */
	inline bool isRanged() const
	{
		return this->ranged_;
	}

	/**
	 * Range of the test.
	 */
	inline std::tuple<bool, int64_t, int64_t> getRange() const
	{
		return std::tuple<bool, int64_t, int64_t>(
			this->isRanged(), this->range_low_, this->range_high_);
	}

	/**
	 * Run the test. If the test is not enabled, this just adds a skipped
	 * result to Results and returns.
	 */
	void run() const;

	/**
	 * Run the test's cleanup function
	 */
	void cleanup() const;
};
}
