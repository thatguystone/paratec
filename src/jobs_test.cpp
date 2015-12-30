/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <atomic>
#include <iostream>
#include <sys/wait.h>
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
	std::this_thread::sleep_for(std::chrono::seconds(10));
}

static SharedMem<std::atomic_bool> _sleeping;
TEST(_sleep)
{
	_sleeping->store(true);
	std::this_thread::sleep_for(std::chrono::seconds(10));
}

TEST(jobsNoFork)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "--nofork" });
	});

	pt_ss(e.stdout_.c_str(), "Running: _0\n=========");
}

TEST(jobsNoForkFiltered)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "--nofork", "-f", "_2" });
	});

	pt_ss(e.stdout_.c_str(), "Running: _2");
	pt_ss(e.stdout_.c_str(), "100%: of 1");
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

TEST(jobsForkFiltered)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "-f", "_2" });
	});

	pt_ss(e.stdout_.c_str(), "100%: of 1");
}

TEST(jobsAbortSignal, PTSIG(6))
{
	abort();
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

TEST(jobsTerminateFromSignal)
{
	Fork f;
	int status;

	bool parent = f.fork(false, true);
	if (!parent) {
		Main m({ MKTEST(_sleep) });
		m.run(std::cout, { "paratec" });
		exit(0);
	}

	// Wait until the test is firmly asleep before trying to kill.
	pt_wait_for(_sleeping->load());

	auto err = f.terminate(&status);
	OSErr(err, {}, "failed to reap child");

	pt_eq(f.pid(), err);
	pt(WIFSIGNALED(status));
	pt_eq(WTERMSIG(status), SIGTERM);
}
}
