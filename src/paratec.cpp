/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include <random>
#include "jobs.hpp"
#include "paratec.h"
#include "paratec.hpp"
#include "signal.hpp"

extern "C" {
#ifdef PT_LINUX

/**
 * There's something weird going on with GCC/LD on Linux: when not using a
 * pointer, anything larger than a few words causes the section to be
 * misaligned, and you can't access anything.
 */
extern struct _paratec *__start_paratec;
extern struct _paratec *__stop_paratec;

#elif defined(PT_DARWIN)

extern struct _paratec *
	__start_paratec __asm("section$start$__DATA$" PT_SECTION_NAME);
extern struct _paratec *
	__stop_paratec __asm("section$end$__DATA$" PT_SECTION_NAME);

#endif
}

namespace pt
{

Main::Main()
{
	_paratec **sp;

	sp = &__start_paratec;
	while (sp < &__stop_paratec) {
		this->tests_.emplace_back(mksp<Test>(**sp));
		sp++;
	}
}

Results Main::main(int argc, char **argv)
{
	int i;
	std::vector<const char *> args;

	for (i = 0; i < argc; i++) {
		args.push_back(argv[i]);
	}

	return this->run(args);
}

Main::~Main()
{
	sig::reset();
}

Results Main::run(const std::vector<const char *> &args)
{
	int err;

	this->opts_->parse(std::move(args));
	auto res = mksp<Results>(this->opts_);
	for (auto &test : this->tests_) {
		test->prepare(this->opts_, res);
	}

	// Shuffle tests on each run to ensure that tests don't accidentally rely
	// on implied ordering.
	std::shuffle(this->tests_.begin(), this->tests_.end(),
				 std::random_device());

	if (this->opts_->capture_) {
		err = setenv("LIBC_FATAL_STDERR_", "1", 1);
		OSErr(err, {}, "failed to set LIBC_FATAL_STDERR_");
	}

	if (this->opts_->fork_) {
		auto jobs = mksp<Jobs>(this->opts_, res, std::move(this->tests_));
		sig::takeover(jobs);
		res->startTimer();
		jobs->run();
	} else {
		res->startTimer();
		for (auto &test : this->tests_) {
			BasicJob(this->opts_).run(test);
		}
	}

	return *res.get();
}
}
