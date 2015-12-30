/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <atomic>
#include "fork.hpp"
#include "opts.hpp"
#include "util.hpp"
#include "util_test.hpp"

namespace pt
{
void util_setup()
{
	Opts().clearEnv();
}

TEST(utilFormat)
{
	std::stringstream out;
	char buff[8192];

	memset(buff, 'a', sizeof(buff));
	buff[sizeof(buff) - 1] = '\0';

	format(out, "%s", buff);

	pt_eq(strlen(buff), out.str().size());
}

TEST(sharedMem)
{
	SharedMem<std::atomic_bool> m0;
	SharedMem<std::atomic_bool> m1;

	pt(!m0->load());
	pt(!m1->load());

	Fork().run([&]() {
		m0->store(true);
		Fork().run([&]() { m1->store(true); });
	});

	pt(m0->load());
	pt(m1->load());
}
}
