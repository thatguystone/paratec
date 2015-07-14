/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "../paratec.h"

PARATEC(assert_errno)
{
	errno = 98;
	pt_eno(-1, 0);

	pt_noe(0);
	pt_noe(123);
}

PARATEC(assert_noerror, PTFAIL())
{
	errno = 98;
	pt_noe(-1);
}
