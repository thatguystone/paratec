/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include <unistd.h>
#include "../paratec.h"

PARATEC(basic)
{
}

PARATEC(exit, PTEXIT(1))
{
	exit(1);
}

PARATEC(signal, PTSIG(SIGABRT))
{
	abort();
}

PARATEC(sleep, PTSIG(SIGKILL), PTTIME(.01))
{
	while (1) {
		usleep(1000 * 1000 * 1000);
	}
}
