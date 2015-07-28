/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

static void _up(void)
{
	printf("up-");
}

static void _down(void)
{
	printf("down\n");
}

PARATEC(basic, PTUP(_up), PTDOWN(_down))
{

}
