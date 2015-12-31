/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "util_test.hpp"

namespace pt
{

// @todo test errno assertions

PARATEC(asserts)
{
	uint32_t a = 1;
	uint64_t b = 2;

	pt_eq(a, b);
}
}
