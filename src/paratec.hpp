/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <string>
#include <vector>
#include "opts.hpp"
#include "std.hpp"
#include "test.hpp"

namespace pt
{

class Main
{
	sp<Opts> opts_ = mksp<Opts>();
	std::vector<sp<Test>> tests_;

public:
	/**
	 * Use the tests in the binary
	 */
	Main();

	/**
	 * Use these tests instead
	 */
	Main(std::initializer_list<sp<Test>> tests) : tests_(std::move(tests))
	{
	}

	~Main();

	/**
	 * Run from the command line, taking over the entire environment
	 */
	Results main(int argc, char **argv);

	/**
	 * Run the tests with the given arguments.
	 */
	Results run(const std::vector<const char *> &args);
};
}
