/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <signal.h>
#include "err.hpp"
#include "signal.hpp"

namespace pt
{
namespace sig
{

static sp<Jobs> _jobs;

#ifdef PT_LINUX
static sigset_t _sigset;
#endif

static void _handler(int sig)
{
	_jobs->terminate();

	signal(sig, SIG_DFL);
	raise(sig);
}

void takeover(sp<Jobs> jobs)
{
	if (_jobs != nullptr) {
		Err(-1, "jobs already managed for this process");
	}

	_jobs = std::move(jobs);
	signal(SIGINT, _handler);
	signal(SIGTERM, _handler);

#ifdef PT_LINUX

	{
		int err;

		sigemptyset(&_sigset);
		sigaddset(&_sigset, SIGCHLD);

		err = sigprocmask(SIG_BLOCK, &_sigset, NULL);
		OSErr(err, {}, "failed to change signal mask");
	}

#endif
}

void reset()
{
	if (_jobs == nullptr) {
		return;
	}

	_jobs = nullptr;
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

#ifdef PT_LINUX
	sigdelset(&_sigset, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &_sigset, NULL);
#endif
}

void childWait()
{
	const int kSleepUsec = 10 * 1000;

#ifdef PT_LINUX

	int err;
	struct timespec timeout = {
		.tv_sec = 0, .tv_nsec = kSleepUsec * 1000,
	};

	err = sigtimedwait(&_sigset, NULL, &timeout);
	OSErr(err, { EAGAIN }, "sigtimedwait failed");

#else

	usleep(kSleepUsec);

#endif
}
}
}
