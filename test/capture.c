/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

PARATEC(capture)
{
	int i;

	for (i = 0; i < 1024; i++) {
		printf("capture this!\n");
	}
}
