/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <sstream>
#include "paratec.h"

#define TEST(name, ...) PARATEC(name, PTUP(util_setup), ##__VA_ARGS__)

#define TESTV(name, vec, ...)                                                  \
	PARATECV(name, vec, PTUP(util_setup), ##__VA_ARGS__)

#define MKTEST(name) mksp<const Test>(__PT_STRUCT(name))

namespace pt
{

/**
 * Setup the test for running
 */
void util_setup();
}
