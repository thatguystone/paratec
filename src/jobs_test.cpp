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
#include "main.hpp"
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

TEST(_fail)
{
	pt_fail("failure");
}

TEST(_error)
{
	abort();
}

TEST(_skip)
{
	pt_skip();
}

TEST(_port)
{
	auto p = pt_get_port(0);

	pt_gt(p, 0);
	pt_gt(pt_get_port(1), p);
}

TEST(_iterName, PTI(-5, 5))
{
	pt_set_iter_name("fun:%" PRId64, _i);
}

TEST(_notIterSetName)
{
	pt_set_iter_name("what am i doing?");
}

TEST(_noIters0, PTI(0, 0))
{
}

TEST(_noIters1, PTI(1, 1))
{
}

static int _empty[] = {};

TESTV(_emptyVector, _empty)
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

	pt_in(e.stdout_.c_str(), "Running: _0\n=========");
}

TEST(jobsNoForkFiltered)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "--nofork", "-f", "_2" });
	});

	pt_in(e.stdout_.c_str(), "Running: _2");
	pt_in(e.stdout_.c_str(), "100%: of 1");
}

TEST(jobsNoForkThreadedAssertion)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_threadedAssertion) });
		m.run(std::cout, { "paratec", "--nofork" });
	});

	pt_in(e.stdout_.c_str(), "Whoa there!");
}

TEST(jobsFork)
{
	std::stringstream out;

	Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, "100%");
}

TEST(jobsForkFiltered)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "-f", "_2" });
	});

	pt_in(e.stdout_.c_str(), "100%: of 1");
}

TEST(jobsForkNoCapture)
{
	// @todo
}

TEST(jobsBench)
{
	// @todo
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
	pt_in(s, "received signal");
}

TEST(jobsTimeout)
{
	std::stringstream out;

	Main m({ MKTEST(_timeout) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, "TIME OUT : _timeout");
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

static void _fail(bool fork)
{
	std::stringstream out;
	std::vector<const char *> args({ "paratec", "-vvv" });

	if (!fork) {
		args.push_back("--nofork");
	}

	Main m({ MKTEST(_fail) });
	m.run(out, args);

	auto s = out.str();
	pt_in(s, "FAIL : _fail");
	pt_in(s, "0%: of 1");
	pt_in(s, "1 failures");
}

TEST(jobsForkFail)
{
	_fail(true);
}

TEST(jobsNoForkFail)
{
	_fail(false);
}

// This can only run in a forked env since it calls exit()
TEST(jobsError)
{
	std::stringstream out;

	Main m({ MKTEST(_error) });
	m.run(out, { "paratec", "-vvv" });

	auto s = out.str();
	pt_in(s, "ERROR : _error");
	pt_in(s, "0%: of 1");
	pt_in(s, "1 errors");
}

static void _skip(bool fork)
{
	std::stringstream out;
	std::vector<const char *> args({ "paratec", "-vvv" });

	if (!fork) {
		args.push_back("--nofork");
	}

	Main m({ MKTEST(_0), MKTEST(_skip) });
	m.run(out, args);

	auto s = out.str();
	pt_in(s, "SKIP : _skip");
	pt_in(s, "100%: of 1");
	pt_in(s, "1 skipped");
}

TEST(jobsForkSkip)
{
	_skip(true);
}

TEST(jobsNoForkSkip)
{
	_skip(false);
}

static void _getPort(bool fork)
{
	std::vector<const char *> args({ "paratec" });
	if (!fork) {
		args.push_back("--nofork");
	}

	Main m({ MKTEST(_port), MKTEST(_port), MKTEST(_port) });
	auto res = m.run(std::cout, args);

	pt_eq(res.exitCode(), 0);
}

TEST(jobsForkGetPort)
{
	_getPort(true);
}

TEST(jobsNoForkGetPort)
{
	_getPort(false);
}

TEST(jobsGetCppName)
{
	pt_eq("jobsGetCppName", pt_get_name());
}

extern "C" {
TEST(jobsGetCName)
{
	pt_eq("jobsGetCName", pt_get_name());
}
}

static void _setIterName(bool fork)
{
	std::stringstream out;

	std::vector<const char *> args({ "paratec", "-vvv" });
	if (!fork) {
		args.push_back("--nofork");
	}

	Main m({ MKTEST(_iterName), MKTEST(_notIterSetName) });
	m.run(out, args);

	auto s = out.str();
	pt_in(s, "PASS : _iterName:-1:fun:-1");
	pt_in(s, "PASS : _iterName:0:fun:0");
	pt_in(s, "PASS : _iterName:4:fun:4");
	pt_in(s, "PASS : _notIterSetName (");
}

TEST(jobsForkSetIterName)
{
	_setIterName(true);
}

TEST(jobsNoForkSetIterName)
{
	_setIterName(false);
}

TEST(jobsEmptyIterTest)
{
	std::stringstream out;

	Main m({ MKTEST(_noIters0), MKTEST(_noIters1), MKTEST(_emptyVector) });
	m.run(out, { "paratec", "--nofork" });

	auto s = out.str();
	pt_in(s, "100%: of 0 tests");
}

TEST(jobsCaptureNullByteInOutput)
{
// @todo
}

TEST(jobsFixtures)
{
	// @todo test setup, teardown, cleanup
}
}
