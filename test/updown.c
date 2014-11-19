/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

static void _up()
{
	printf("up-");
}

static void _down()
{
	printf("down\n");
}

PARATEC(basic, PTUP(_up), PTDOWN(_down))
{

}
