/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include "paratec.h"
#include "std.hpp"

namespace pt
{

/**
 * May only contain data elements that are safe to share across processes;
 * aka. only primitive types and constant-sized arrays of primitives.
 */
struct TestEnv {
	static constexpr int kSize = 2048;

	/**
	 * The id of the job that this test is running in
	 */
	uint id_;

	/**
	 * If this test failed
	 */
	bool failed_;

	/**
	 * If this was skipped
	 */
	bool skipped_;

	/**
	 * Bench results
	 */
	uint64_t bench_iters_;
	uint64_t bench_ns_op_;

	/**
	 * Human-readable and print-friendly test name
	 */
	char test_name_[kSize];

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
	char fail_msg_[PT_FAIL_BUFF];

	void reset(uint id, const char *test_name, const char *func_name);
};
}
