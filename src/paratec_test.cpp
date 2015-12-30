/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "paratec.hpp"
#include "util_test.hpp"

namespace pt
{

// @todo implement C++-style asserts for everything
template <typename T> void eq(T a, T b)
{
	if (a != b) {
		pt_fail("nope");
	}
}

// @todo test vector test with 0 items (should not execute at all)
// @todo test calling run on same pt::Main() multiple times.
}
