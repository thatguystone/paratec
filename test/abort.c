/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include "../paratec.h"

static void _mark(void)
{
	pt_mark();
}

PARATEC(death)
{
	abort();
}

PARATEC(mark)
{
	_mark();
	abort();
}
