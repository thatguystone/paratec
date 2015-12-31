/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <iostream>
#include "main.hpp"
#include "util_test.hpp"

namespace pt
{

#include "paratec_c_test.c"

TEST(_custom0)
{
	pt_eq(1, -1, "i failed %d", 1234);
}

TEST(_custom1)
{
	pt_eq("yes", "no", "what? %s", "no");
}

TEST(_custom2)
{
	pt_eq(nullptr, (void *)123, "mah pointers");
}

TEST(customMessages)
{
	std::stringstream out;

	Main m({ MKTEST(_custom0), MKTEST(_custom1), MKTEST(_custom2) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in("Expected `1` == `-1` :: i failed 1234", s);
	pt_in("Expected `yes` == `no` :: what? no", s);
	pt_in("Expected `(nil)` == `0x7b` :: mah pointers", s);
}
}
