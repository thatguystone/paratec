/**
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include <iostream>
#include <random>
#include "jobs.hpp"
#include "main.hpp"
#include "paratec.h"
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

Results Main::main(std::ostream &os, int argc, char **argv)
{
	int i;
	std::vector<const char *> args;

	for (i = 0; i < argc; i++) {
		args.push_back(argv[i]);
	}

	return this->run(os, args);
}

Main::~Main()
{
	sig::reset();
}

Results Main::run(std::ostream &os, const std::vector<const char *> &args)
{
	int err;
	std::vector<sp<const Test>> tests;

	this->opts_->parse(std::move(args));
	auto rslts = mksp<Results>(this->opts_, os);

	auto addTest = [&](sp<const Test> t) {
		rslts->inc(t->enabled());
		tests.push_back(std::move(t));
	};

	for (auto &test : this->tests_) {
		bool ranged;
		int64_t low;
		int64_t high;
		std::tie(ranged, low, high) = test->getRange();

		if (!ranged) {
			addTest(test->bindTo(0, this->opts_));
		} else {
			int64_t i;
			for (i = low; i < high; i++) {
				addTest(test->bindTo(i, this->opts_));
			}
		}
	}

	// Shuffle tests on each run to ensure that tests don't accidentally rely
	// on implied ordering.
	std::shuffle(tests.begin(), tests.end(), std::random_device());

	if (this->opts_->capture_) {
		err = setenv("LIBC_FATAL_STDERR_", "1", 1);
		OSErr(err, {}, "failed to set LIBC_FATAL_STDERR_");
	}

	if (this->opts_->fork_) {
		auto jobs = mksp<Jobs>(this->opts_, rslts, std::move(tests));
		sig::takeover(jobs);
		rslts->startTimer();
		jobs->run();
	} else {
		rslts->startTimer();
		for (auto &test : tests) {
			BasicJob(0, this->opts_, rslts).run(test);
		}
	}

	rslts->dump();

	return *rslts;
}
}

int main(int argc, char **argv)
{
	auto res = pt::Main().main(std::cout, argc, argv);
	return res.exitCode();
}
