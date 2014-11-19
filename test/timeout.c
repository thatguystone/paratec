/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <unistd.h>
#include "../paratec.h"

PARATEC(timeout, PTTO(.01))
{
	while (1) {
		usleep(1000 * 1000 * 1000);
	}
}
