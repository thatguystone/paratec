/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

static void _cleanup(void)
{
	printf("%s, everybody clean up!\n", pt_get_name());
}

PARATEC(cleanup_test, PTCLEANUP(_cleanup))
{}

static void _cleanupi(void)
{
	printf("i %d cleanup!\n", pt_get_port(0));
}

PARATEC(cleanup_testi, PTCLEANUP(_cleanupi), PTI(0, 16))
{}
