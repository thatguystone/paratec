/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "paratec.h"
#include <stdlib.h>

PARATEC(asserts)
{
	pt_eq(0, 0);
	pt_eq('a', 'a');
	pt_eq("test", "test");

	pt_ne(0, 1);
	pt_ne('a', 'A');
	pt_ne("test", "TEST");

	pt_gt(1, 0);
	pt_gt("z", "a");

	pt_ge(1, 1);
	pt_ge("z", "z");

	pt_lt(0, 1);
	pt_lt("a", "z");

	pt_le(1, 1);
	pt_le("z", "z");

	pt_in("cde", "abcdefgh");
	pt_ni("xyz", "abcdefgh");
}

PARATEC(waitFor)
{
	int i = 0;
	pt_wait_for(i++ == 10);
}
