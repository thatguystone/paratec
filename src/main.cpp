/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <iostream>
#include "paratec.hpp"

int main(int argc, char **argv)
{
	auto res = pt::Main().main(std::cout, argc, argv);
	return res.exitCode();
}
