/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include "jobs.hpp"
#include "std.hpp"

namespace pt
{
namespace sig
{

/**
 * Takeover signal management. Only 1 thing may manage signals at a time
 */
void takeover(sp<Jobs> jobs);

/**
 * Release signal management
 */
void reset();

/**
 * Wait for a child process to exit, if the platform supports it, or just
 * pause for a while.
 */
void childWait();
}
}
