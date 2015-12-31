/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "signal.hpp"
#include "util_test.hpp"

namespace pt
{
namespace sig
{

TEST(signalTakeover)
{
	auto opts = mksp<Opts>();
	auto jobs = mksp<Jobs>(opts, mksp<Results>(opts, std::cout),
						   std::vector<sp<const Test>>());

	takeover(jobs);

	try {
		takeover(jobs);
		pt_fail("should have failed");
	} catch (Err) {
	}
}
}
}
