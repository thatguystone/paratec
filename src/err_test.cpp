/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "test.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(errBasic)
{
	va_list args;

	Err(0, "this is fine");
	Err(0, "this is fine", args);

	OSErr(0, {}, "this is fine");
	OSErr(0, {}, "this is fine", args);

	errno = EAGAIN;
	OSErr(-1, { EAGAIN }, "this is fine");

	errno = EAGAIN;
	OSErr(-1, { EAGAIN }, "this is fine", args);
}

TEST(errFailures)
{
	va_list args;

	try {
		Err(-1, "this is not fine");
		pt_fail("should not be fine");
	} catch (Err e) {
		pt_seq("this is not fine", e.what());
	}

	try {
		Err(-1, "this is not fine", args);
		pt_fail("should not be fine");
	} catch (Err) {
	}

	try {
		OSErr(-1, {}, "this is not fine");
		pt_fail("should not be fine");
	} catch (Err) {
	}

	try {
		OSErr(-1, {}, "this is not fine", args);
		pt_fail("should not be fine");
	} catch (Err) {
	}

	try {
		errno = EAGAIN;
		OSErr(-1, {}, "this is fine");
		pt_fail("should not be fine");
	} catch (Err) {
	}

	try {
		errno = EAGAIN;
		OSErr(-1, {}, "this is fine", args);
		pt_fail("should not be fine");
	} catch (Err) {
	}
}
}
