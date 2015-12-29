/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include "paratec.h"
#include "opts.hpp"
#include "util_test.hpp"

namespace pt
{

static struct {
	std::vector<const char *> args_;
	int cnt;
} _verbose[] = {
	{
	 .args_ = { "paratec" },
	 .cnt = 0,
	},
	{
	 .args_ = { "paratec", "-vvv" },
	 .cnt = 2,
	},
	{
	 .args_ = { "paratec", "-v", "-v", "-v" },
	 .cnt = 3,
	},
};

TESTV(optsVerbose, _verbose)
{
	Opts opts;

	opts.parse(_t->args_);
	pt_eq(opts.verbose_.get(), _t->cnt);
}

TEST(optsFilter)
{
}

TEST(optsPort)
{
	Opts opts;
	opts.parse({ "paratec", "--port", "3333" });
	pt_feq(opts.port_.get(), 3333);
}

TEST(optsPortError, PTEXIT(1))
{
	Opts opts;
	opts.parse({ "paratec", "--port", "-1" });
}

TEST(optsTimeout)
{
	Opts opts;
	opts.parse({ "paratec", "-t", "3.3" });
	pt_feq(opts.timeout_.get(), 3.3);
}

TEST(optsTimeoutError, PTEXIT(1))
{
	Opts opts;
	opts.parse({ "paratec", "-t", "-1" });
}

// @todo test negative int/dbl values
}
