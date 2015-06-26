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
	pt_ss("defg", "ef");
}

PARATEC(memory)
{
	pt_meq("abcd", "abcd", 4);
	pt_mne("defg", "hijk", 4);
}

PARATEC(ptr)
{
	pt_peq((void*)1, (void*)1);
	pt_pne((void*)1, (void*)2);
	pt_pgt((void*)2, (void*)1);
	pt_pge((void*)2, (void*)2);
	pt_pge((void*)3, (void*)2);
	pt_plt((void*)1, (void*)2);
	pt_ple((void*)2, (void*)2);
	pt_ple((void*)2, (void*)3);
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

PARATEC(f_fail, PTFAIL())
{
	pt_feq(1.0, 2.0);
}

PARATEC(f_fail_msg, PTFAIL())
{
	pt_feq_msg(1.0, 2.0, "We're all going to die!");
}
