/**
 * @author Andrew Stone <a@stoney.io>
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

PARATEC(two)
{
	printf("two\n");
}

PARATEC(others)
{
	printf("others\n");
}

PARATEC(fail)
{
	int *i = 0;
	*i = 1;
}
