/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "../paratec.h"

PARATEC(strings)
{
	pt_seq("abcd", "abcd");
	pt_sne("defg", "hijk");
}

PARATEC(memory)
{
	pt_meq("abcd", "abcd", 4);
	pt_mne("defg", "hijk", 4);
}

PARATEC(int)
{
	pt_eq(1, 1);
	pt_ne(1, 2);
	pt_gt(2, -1);
	pt_ge(2, 2);
	pt_ge(3, 2);
	pt_lt(-1, 2);
	pt_le(2, 2);
	pt_le(2, 3);
}

PARATEC(uint)
{
	pt_ueq(1, 1);
	pt_une(1, 2);
	pt_ugt(2, 1);
	pt_uge(2, 2);
	pt_uge(3, 2);
	pt_ult(1, 2);
	pt_ule(2, 2);
	pt_ule(2, 3);
}

PARATEC(int_fail, PTFAIL())
{
	pt_eq(-1, 1);
}

PARATEC(int_fail_msg, PTFAIL())
{
	pt_eq_msg(-1, 1, "We're all going to die!");
}

PARATEC(uint_fail, PTFAIL())
{
	pt_ueq(1, 2);
}

PARATEC(uint_fail_msg, PTFAIL())
{
	pt_ueq_msg(1, 2, "We're all going to die!");
}
