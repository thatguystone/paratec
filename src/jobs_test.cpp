/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "paratec.h"
#include "jobs.hpp"
#include "paratec.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(_0)
{
}

TEST(_1)
{
}

TEST(_2)
{
}

TEST(_3)
{
}

TEST(jobsNoFork)
{
	Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
	m.run({ "paratec", "--nofork" });
}

TEST(jobsFork)
{
	Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
	m.run({ "paratec" });
}
}
