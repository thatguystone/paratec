/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include "time.hpp"

namespace pt
{
namespace time
{

point now()
{
	return std::chrono::steady_clock::now();
}

double toSeconds(duration d)
{
	return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

duration toDuration(double secs)
{
	return std::chrono::duration_cast<duration>(
		std::chrono::duration<double, std::ratio<1, 1>>(secs));
}
}
}
