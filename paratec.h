/**
 * Run parallel unit tests in C.
 *
 * @file
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Run a unit test.
 */
#define PARATEC(test_fn, ...) \
	static void __paratec_test_ ## test_fn(int64_t); \
	static struct paratec* __paratec_ ## test_fn \
		__attribute((__section__("paratec"))) \
		__attribute((__used__)) = &(struct paratec){ \
			.file = __FILE__, \
			.name = #test_fn, \
			.fn = __paratec_test_ ## test_fn, \
			__VA_ARGS__ \
		}; \
	static void __paratec_test_ ## test_fn(int64_t _i __attribute((__unused__)))

/**
 * Run a unit test, expecting the given exit status.
 */
#define PTEXIT(s) .exit_status = s

/**
 * Run a unit test, expecting the given signal.
 */
#define PTSIG(s) .signal_num = s

/**
 * Run a unit test with a specific timeout (in seconds as a double).
 */
#define PTTO(s) .timeout = s

/**
 * Add a setup function to the test that runs in the isolated environment.
 */
#define PTUP(s) .setup = s

/**
 * Add a teardown function to the test that runs in the isolated environment.
 */
#define PTDOWN(t) .teardown = t

/**
 * Used internally by paratec. Don't mess with any of this.
 */
struct paratec {
	char *file;
	char *name;
	int exit_status;
	int signal_num;
	double timeout;
	void (*fn)(int64_t);
	void (*setup)();
	void (*teardown)();
};
