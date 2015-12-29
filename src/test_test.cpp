/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "paratec.h"
#include "paratec.hpp"
#include "test.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(_one)
{
	pt_fail("this test should never run");
}

static struct {
	std::vector<const char *> args_;
	bool enabled_;

} _testFilter[] = {
	{
	 .args_ = { "paratec", "--filter=-_one" },
	 .enabled_ = false,
	},
	{
	 .args_ = { "paratec", "--filter=-_" },
	 .enabled_ = false,
	},
	{
	 .args_ = { "paratec", "--filter=tests" },
	 .enabled_ = false,
	},
	{
	 .args_ = { "paratec", "--filter=_one" },
	 .enabled_ = true,
	},
	{
	 .args_ = { "paratec", "--filter=_o" },
	 .enabled_ = true,
	},
	{
	 .args_ = { "paratec", "--filter=_ones" },
	 .enabled_ = false,
	},
	{
	 .args_ = { "paratec", "--filter=_one,-_one,_one" },
	 .enabled_ = true,
	},
};

TESTV(testFilter, _testFilter)
{
	auto test = MKTEST(_one);
	auto opts = mksp<Opts>();

	opts->parse(_t->args_);
	pt_msg(test->bindTo(0, opts)->enabled() == _t->enabled_,
		   "expected test to be %s", _t->enabled_ ? "enabled" : "disabled");
}
}
