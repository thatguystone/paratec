/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <iostream>
#include "results.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(resultsGetFailure)
{
	auto opts = mksp<Opts>();

	try {
		Results(opts, std::cout).get("i dont exist");
		pt_fail("should have failed");
	} catch (Err) {
	}
}
}
