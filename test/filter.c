/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

PARATEC(run)
{
	printf("run\n");
}

PARATEC(another)
{
	printf("another\n");
}

PARATEC(fail)
{
	int *i = 0;
	*i = 1;
}
