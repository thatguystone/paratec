/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <iostream>
#include <thread>
#include "jobs.hpp"
#include "paratec.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(_0)
{
}

TEST(_1)
{
}

TEST(_2)
{
}

TEST(_3)
{
}

TEST(_threadedAssertion)
{
	std::thread th([]() { pt_fail("from another thread"); });

	while (true) {
		usleep(1000000);
	}
}

TEST(_signalMismatch, PTSIG(5))
{
}

TEST(_timeout, PTTIME(.001))
{
}

TEST(jobsNoFork)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "--nofork" });
	});

	pt_ss(e.stdout_.c_str(), "Running: _0\n=========");
}

TEST(jobsAbortSignal, PTSIG(6))
{
	abort();
}

TEST(jobsNoForkThreadedAssertion)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_threadedAssertion) });
		m.run(std::cout, { "paratec", "--nofork" });
	});

	pt_ss(e.stdout_.c_str(), "Whoa there!");
}

TEST(jobsFork)
{
	std::stringstream out;

	Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_ss(s.c_str(), "100%");
}

TEST(jobsSignal)
{
	std::stringstream out;

	Main m({ MKTEST(_signalMismatch) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_ss(s.c_str(), "received signal");
}

TEST(jobsTimeout)
{
	std::stringstream out;

	Main m({ MKTEST(_timeout) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_ss(s.c_str(), "TIME OUT : _timeout");
}

TEST(jobsTerminate)
{
}
}
