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
#include "util.hpp"
#include "util_test.hpp"

namespace pt
{

TEST(_0)
{
	printf("%s", pt_get_name());
}

TEST(_1)
{
	printf("%s", pt_get_name());
}

TEST(_2)
{
	printf("%s", pt_get_name());
}

TEST(_3)
{
	printf("%s", pt_get_name());
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

TEST(_threadedAssertion)
{
	std::thread th([]() { pt_fail("from another thread"); });

	while (true) {
		usleep(1000000);
	}
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

	pt_in(e.stdout_, "100%: of 1");
}

TEST(jobsForkNoCapture)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
		m.run(std::cout, { "paratec", "--nocapture" });
	});

	pt_in(e.stdout_, "_0");
	pt_in(e.stdout_, "_1");
	pt_in(e.stdout_, "_2");
	pt_in(e.stdout_, "_3");
}

TEST(jobsVeryVerbose)
{
	std::stringstream out;

	Main m({ MKTEST(_0), MKTEST(_1), MKTEST(_2), MKTEST(_3) });
	m.run(out, { "paratec", "-vvvv" });

	auto s = out.str();
	pt_in(s, "stdout\n");
	pt_in(s, "_0");
	pt_in(s, "_1");
	pt_in(s, "_2");
	pt_in(s, "_3");
}

TEST(jobsBench)
{
	// @todo
}

TEST(jobsAbortSignal, PTSIG(SIGABRT))
{
	abort();
}

TEST(_signalMismatch, PTSIG(5))
{
}

TEST(jobsSignal)
{
	std::stringstream out;

	Main m({ MKTEST(_signalMismatch) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, "received signal");
}

TEST(_exitMismatch, PTEXIT(1))
{
}

TEST(jobsExit)
{
	std::stringstream out;

	Main m({ MKTEST(_exitMismatch) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, "got exit code");
}

TEST(_timeout, PTTIME(.001))
{
	std::this_thread::sleep_for(std::chrono::seconds(10));
}

TEST(jobsTimeout)
{
	std::stringstream out;

	Main m({ MKTEST(_timeout) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, "TIME OUT : _timeout");
}

static SharedMem<std::atomic_bool> _sleeping;
TEST(_sleep)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	_sleeping->store(true);
	std::this_thread::sleep_for(std::chrono::seconds(10));
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
	pt_eq(WTERMSIG(status), SIGKILL);
}

TEST(_fail)
{
	pt_fail("failure");
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

TEST(_error)
{
	abort();
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

TEST(jobsDisabled)
{
	std::stringstream out;

	Main m({ MKTEST(_0) });
	m.run(out, { "paratec", "-vvv", "-f=-_0" });

	auto s = out.str();
	pt_in(s, "DISABLED : _0");
	pt_in(s, "100%: of 0");
}

TEST(_skip)
{
	pt_skip();
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

TEST(_port)
{
	auto p = pt_get_port(0);

	pt_gt(p, (uint16_t)0);
	pt_gt(pt_get_port(1), p);
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

TEST(_iterName, PTI(-5, 5))
{
	pt_set_iter_name("fun:%" PRId64, _i);
}

TEST(_notIterSetName)
{
	pt_set_iter_name("what am i doing?");
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

TEST(jobsEmptyIterTest)
{
	std::stringstream out;

	Main m({ MKTEST(_noIters0), MKTEST(_noIters1), MKTEST(_emptyVector) });
	m.run(out, { "paratec", "--nofork" });

	auto s = out.str();
	pt_in(s, "100%: of 0 tests");
}

TEST(_null)
{
	std::cout << std::string("null: \0", 7);
	pt_fail("nulls?");
}

TEST(jobsCaptureNullByteInOutput)
{
	std::stringstream out;

	Main m({ MKTEST(_null) });
	m.run(out, { "paratec" });

	auto s = out.str();
	pt_in(s, std::string("null: \0", 7));
}

static void _fixtureUp()
{
	printf("up\n");
}
static void _fixtureDown()
{
	printf("down\n");
}
static void _fixtureCleanup()
{
	printf("cleanup\n");
}

PARATEC(_fixture,
		PTUP(_fixtureUp),
		PTDOWN(_fixtureDown),
		PTCLEANUP(_fixtureCleanup))
{
}

TEST(jobsFixtures)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_fixture) });
		m.run(std::cout, { "paratec", "-vvvv" });
	});

	pt_in(e.stdout_, "cleanup\n");
	pt_in(e.stdout_, "| up\n");
	pt_in(e.stdout_, "| down\n");
}

TEST(jobsFixturesOrdering)
{
	auto e = Fork().run([]() {
		Main m({ MKTEST(_fixture) });
		m.run(std::cout, { "paratec", "--nocapture" });
	});

	pt_in(e.stdout_, "up\ndown\ncleanup\n");
}

TEST(_assertInTest)
{
	pt_eq(0, 1);
}

static void _assertOutTest()
{
	pt_eq(0, 1);
}

TEST(_assertOutTest)
{
	_assertOutTest();
}

TEST(_assertMarkBeforeOut)
{
	pt_mark();
	_assertOutTest();
}

TEST(jobsAssertionLines)
{
	Main m({ MKTEST(_assertInTest),
			 MKTEST(_assertOutTest),
			 MKTEST(_assertMarkBeforeOut) });
	auto rslts = m.run(std::cout, { "paratec" });

	auto res = rslts.get("_assertInTest");
	pt_in(res.last_line_, "jobs_test.cpp");
	pt_ni(res.last_line_, "last test assert");

	res = rslts.get("_assertOutTest");
	pt_in(res.last_line_, "last test assert: test start");

	res = rslts.get("_assertMarkBeforeOut");
	pt_in(res.last_line_, "last test assert: /");
}
}
