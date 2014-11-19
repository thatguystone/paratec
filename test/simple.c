/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include <unistd.h>
#include "../paratec.h"

PARATEC(a)
{

}

PARATEC(exit, PTEXIT(1))
{
	exit(1);
}

PARATEC(signal, PTSIG(SIGSEGV))
{
	int *i = 0;
	*i = 1;
}

PARATEC(sleep, PTSIG(SIGKILL), PTTO(.01))
{
	while (1) {
		usleep(1000 * 1000 * 1000);
	}
}
