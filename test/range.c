/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

PARATEC(range, PTI(-3, 3))
{
	fprintf(stderr, "%" PRIi64 ":", _i);
}

PARATEC(range_name, PTI(0, 2))
{
	pt_set_iter_name("set_iter_name-%" PRIi64, _i);
}

PARATEC(not_range_with_set_name)
{
	pt_set_iter_name("i have a really pretty name");
}
