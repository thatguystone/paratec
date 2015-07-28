/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include "../paratec.h"

PARATEC(port)
{
	pt_ge(pt_get_port(0), 23120);
	pt_ge(pt_get_port(1), pt_get_port(0));
}
