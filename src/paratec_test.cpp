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
// @todo check error messages, last-line marks, etc

#include "paratec_c_test.c"

PARATEC(assertsFailure, PTFAIL())
{
	pt_eq(0, 1);
}
}
