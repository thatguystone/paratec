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
#include <string.h>

/**
 * Run a unit test.
 */

// @todo Darwin: DATA,paratec is section name

#define PARATEC(test_fn, ...) \
	static void __paratec_test_ ## test_fn(int64_t); \
	static struct paratec* __paratec_ ## test_fn \
		__attribute((__section__("paratec"))) \
		__attribute((__used__)) = &(struct paratec){ \
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
 * Expect this test to fail. Kinda weird to use, but no judgment.
 */
#define PTFAIL() .expect_fail = 1

/**
 * Run a unit test with a specific timeout (in seconds as a double).
 */
#define PTTIME(s) .timeout = s

/**
 * Add a setup function to the test that runs in the isolated environment.
 */
#define PTUP(s) .setup = s

/**
 * Add a teardown function to the test that runs in the isolated environment.
 */
#define PTDOWN(t) .teardown = t

/**
 * Run a test multiple times, over the given range.
 * Does: for (i = a; i < b; i++);
 */
#define PTI(a, b) .range_low = a, .range_high = b

/**
 * Mark that the test hit this line
 */
#define pt_mark() _pt_mark(__FILE__, __LINE__)

/**
 * Fail right now with the given message.
 */
#define pt_fail(msg, ...) \
	_pt_fail(msg, ##__VA_ARGS__)

/**
 * A most basic assertion. If false, fail.
 */
#define pt(cond) \
	pt_mark(); \
	if (!(cond)) { \
		pt_fail("`%s` failed", #cond); }

/**
 * Like pt(), but allows you to give your own message.
 */
#define pt_msg(cond, msg, ...) \
	pt_mark(); \
	if (!(cond)) { \
		pt_fail(msg, ##__VA_ARGS__); }

/**
 * String assertions
 */
#define pt_str(a, op, b) { \
	const char* __pt_a = (a); \
	const char* __pt_b = (b); \
	pt_msg(0 op strcmp(__pt_a, __pt_b), \
		"Expected `%s` %s `%s`", __pt_a, #op, __pt_b); }
#define pt_seq(a, b) pt_str(a, ==, b)
#define pt_sne(a, b) pt_str(a, !=, b)

/**
 * Binary assertions
 */
#define pt_mem(a, op, b, len) { \
	const void* __pt_a = (a); \
	const void* __pt_b = (b); \
	const size_t n = (len); \
	pt_msg(0 op memcmp(__pt_a, __pt_b, n), \
		"Expected `%s` %s `%s`", #a, #op, #b); }
#define pt_meq(a, b, len) pt_mem(a, ==, b, len)
#define pt_mne(a, b, len) pt_mem(a, !=, b, len)

/**
 * Integer assertions
 */
#define pt_int(a, op, b) { \
	const int64_t __pt_a = (a); \
	const int64_t __pt_b = (b); \
	pt_msg(__pt_a op __pt_b, \
		"Expected %ld %s %ld", __pt_a, #op, __pt_b); }
#define pt_eq(a, b) pt_int(a, ==, b)
#define pt_ne(a, b) pt_int(a, !=, b)
#define pt_gt(a, b) pt_int(a, >, b)
#define pt_ge(a, b) pt_int(a, >=, b)
#define pt_lt(a, b) pt_int(a, <, b)
#define pt_le(a, b) pt_int(a, <=, b)

/**
 * Integer assertions with custom messages
 */
#define pt_int_msg(a, op, b, msg, ...) { \
	const int64_t __pt_a = (a); \
	const int64_t __pt_b = (b); \
	pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__); }
#define pt_eq_msg(a, b, msg, ...) pt_int_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_ne_msg(a, b, msg, ...) pt_int_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_gt_msg(a, b, msg, ...) pt_int_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_ge_msg(a, b, msg, ...) pt_int_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_lt_msg(a, b, msg, ...) pt_int_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_le_msg(a, b, msg, ...) pt_int_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Unsigned integer assertions
 */
#define pt_uint(a, op, b) { \
	const uint64_t __pt_a = (a); \
	const uint64_t __pt_b = (b); \
	pt_msg(__pt_a op __pt_b, \
		"Expected %ld %s %ld", __pt_a, #op, __pt_b); }
#define pt_ueq(a, b) pt_uint(a, ==, b)
#define pt_une(a, b) pt_uint(a, !=, b)
#define pt_ugt(a, b) pt_uint(a, >, b)
#define pt_uge(a, b) pt_uint(a, >=, b)
#define pt_ult(a, b) pt_uint(a, <, b)
#define pt_ule(a, b) pt_uint(a, <=, b)

/**
 * Unsigned integer assertions with custom messages
 */
#define pt_uint_msg(a, op, b, msg, ...) { \
	const uint64_t __pt_a = (a); \
	const uint64_t __pt_b = (b); \
	pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__); }
#define pt_ueq_msg(a, b, msg, ...) pt_uint_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_une_msg(a, b, msg, ...) pt_uint_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_ugt_msg(a, b, msg, ...) pt_uint_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_uge_msg(a, b, msg, ...) pt_uint_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_ult_msg(a, b, msg, ...) pt_uint_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_ule_msg(a, b, msg, ...) pt_uint_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Used internally by paratec. Don't mess with any of this.
 */
struct paratec {
	char *name;
	int exit_status;
	int signal_num;
	double timeout;
	int expect_fail;
	int64_t range_low;
	int64_t range_high;
	void (*fn)(int64_t);
	void (*setup)();
	void (*teardown)();
};

/**
 * Use the macros above
 */

__attribute__((format(printf, 1, 2)))
void _pt_fail(
	const char *format,
	...);

void _pt_mark(
	const char *file,
	const size_t line);
