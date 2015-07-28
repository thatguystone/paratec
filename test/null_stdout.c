/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <unistd.h>
#include "../paratec.h"

PARATEC(null)
{
	fwrite("byte `\x00` test", 13, 1, stdout);
}
