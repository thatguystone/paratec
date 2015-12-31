/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
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
	setenv("PTFILTER", "0", 1);

	Opts opts;
	opts.parse({ "paratec", "--filter=1,-2", "-f", "3,4", "-f", "5" });

	// pt::eq(opts.filter_.filts_, std::vector<FilterOpt::F>({}));
}

TEST(optsPort)
{
	Opts opts;
	opts.parse({ "paratec", "--port", "3333" });
	pt_eq(opts.port_.get(), 3333);
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
	pt_eq(opts.timeout_.get(), 3.3);
}

TEST(optsTimeoutError, PTEXIT(1))
{
	Opts opts;
	opts.parse({ "paratec", "-t", "-1" });
}
}
