/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <chrono>

namespace pt
{
namespace time
{

typedef std::chrono::time_point<std::chrono::steady_clock> point;
typedef std::chrono::steady_clock::duration duration;

/**
 * Get monotonic now
 */
point now();

/**
 * Conver the duration to nanoseconds
 */
uint64_t toNanoSeconds(duration d);

/**
 * Convert the duration to seconds
 */
double toSeconds(duration d);

/**
 * Convert seconds to a duration
 */
duration toDuration(double secs);
}
}
