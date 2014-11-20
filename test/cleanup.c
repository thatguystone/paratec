/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

static void _cleanup(const char *name)
{
	printf("%s, everybody clean up!\n", name);
}

PARATEC(cleanup_test, PTCLEANUP(_cleanup))
{}
