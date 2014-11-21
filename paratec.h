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
#include <inttypes.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PT_SECTION_NAME "paratec"

#define PTXSTR(s) #s
#define PTSTR(s) PTXSTR(s)

#ifdef __linux__
	#define PT_LINUX
	#define PT_SECTION PT_SECTION_NAME
#elif (defined(__APPLE__) && defined(__MACH__))
	#define PT_DARWIN
	#define PT_SECTION "__DATA," PT_SECTION_NAME
#else
	#error Unsupported platform
#endif

/**
 * Run a unit test.
 */
#define PARATEC(test_fn, ...) \
	static void __paratec_test_ ## test_fn(int64_t); \
	static struct paratec* __paratec_ ## test_fn \
		__attribute((used,section(PT_SECTION))) \
		= &(struct paratec){ \
			.fn_name = PTSTR(__paratec_test_ ## test_fn), \
			.name = PTSTR(test_fn), \
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
#define PTUP(fn) .setup = fn

/**
 * Add a teardown function to the test that runs in the isolated environment.
 */
#define PTDOWN(fn) .teardown = fn

/**
 * Cleanup after a test, outside of the testing environment. Even if a test
 * fails, this function is run. No assertions may be run in this callback as
 * it's only a cleanup function an not part of your test; running any assertions
 * will result in undefined behavior.
 */
#define PTCLEANUP(fn) .cleanup = fn

/**
 * Run a test multiple times, over the given range.
 * Does: for (i = a; i < b; i++);
 */
#define PTI(a, b) .range_low = a, .range_high = b

/**
 * Mark that the test hit this line
 */
#define pt_mark() _pt_mark(__FILE__, __func__, __LINE__)

/**
 * Fail right now with the given message.
 */
#define pt_fail(msg, ...) \
	pt_mark(); \
	_pt_fail(msg, ##__VA_ARGS__)

/**
 * A most basic assertion. If false, fail.
 */
#define pt(cond) \
	pt_mark(); \
	if (!(cond)) { \
		_pt_fail("`%s` failed", #cond); }

/**
 * Like pt(), but allows you to give your own message.
 */
#define pt_msg(cond, msg, ...) \
	pt_mark(); \
	if (!(cond)) { \
		_pt_fail(msg, ##__VA_ARGS__); }

/**
 * String assertions with custom messages
 */
#define pt_str_msg(a, op, b, msg, ...) { \
	const char* __pt_a = (a); \
	const char* __pt_b = (b); \
	pt_msg(0 op strcmp(__pt_a, __pt_b), msg, ##__VA_ARGS__); }
#define pt_seq_msg(a, b, msg, ...) pt_str(a, ==, b, msg, ##__VA_ARGS__)
#define pt_sne_msg(a, b, msg, ...) pt_str(a, !=, b, msg, ##__VA_ARGS__)

/**
 * String assertions
 */
#define pt_str(a, op, b) { \
	pt_str_msg(a, op, b, \
		"Expected `%s` %s `%s`", __pt_a, #op, __pt_b); }
#define pt_seq(a, b) pt_str(a, ==, b)
#define pt_sne(a, b) pt_str(a, !=, b)

/**
 * Binary assertions with custom messages
 */
#define pt_mem_msg(a, op, b, len, msg, ...) { \
	const void* __pt_a = (a); \
	const void* __pt_b = (b); \
	const size_t n = (len); \
	pt_msg(0 op memcmp(__pt_a, __pt_b, n), msg, ##__VA_ARGS__); }
#define pt_meq_msg(a, b, len, msg, ...) pt_mem(a, ==, b, len, msg, ##__VA_ARGS__)
#define pt_mne_msg(a, b, len, msg, ...) pt_mem(a, !=, b, len, msg, ##__VA_ARGS__)

/**
 * Binary assertions
 */
#define pt_mem(a, op, b, len) { \
	pt_mem_msg(a, op, b, len, \
		"Expected `%s` %s `%s`", #a, #op, #b); }
#define pt_meq(a, b, len) pt_mem(a, ==, b, len)
#define pt_mne(a, b, len) pt_mem(a, !=, b, len)

/**
 * Pointer assertions with custom messages
 */
#define pt_ptr_msg(a, op, b, msg, ...) { \
	const void *__pt_a = (a); \
	const void *__pt_b = (b); \
	pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__); }
#define pt_peq_msg(a, b, msg, ...) pt_ptr_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_pne_msg(a, b, msg, ...) pt_ptr_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_pgt_msg(a, b, msg, ...) pt_ptr_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_pge_msg(a, b, msg, ...) pt_ptr_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_plt_msg(a, b, msg, ...) pt_ptr_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_ple_msg(a, b, msg, ...) pt_ptr_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Pointer assertions
 */
#define pt_ptr(a, op, b) { \
	pt_ptr_msg(a, op, b, \
		"Expected %p %s %p", __pt_a, #op, __pt_b); }
#define pt_peq(a, b) pt_ptr(a, ==, b)
#define pt_pne(a, b) pt_ptr(a, !=, b)
#define pt_pgt(a, b) pt_ptr(a, >, b)
#define pt_pge(a, b) pt_ptr(a, >=, b)
#define pt_plt(a, b) pt_ptr(a, <, b)
#define pt_ple(a, b) pt_ptr(a, <=, b)

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
 * Integer assertions
 */
#define pt_int(a, op, b) { \
	pt_int_msg(a, op, b, \
		"Expected %" PRId64 " %s %" PRId64, __pt_a, #op, __pt_b); }
#define pt_eq(a, b) pt_int(a, ==, b)
#define pt_ne(a, b) pt_int(a, !=, b)
#define pt_gt(a, b) pt_int(a, >, b)
#define pt_ge(a, b) pt_int(a, >=, b)
#define pt_lt(a, b) pt_int(a, <, b)
#define pt_le(a, b) pt_int(a, <=, b)

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
 * Unsigned integer assertions
 */
#define pt_uint(a, op, b) { \
	pt_uint_msg(a, op, b, \
		"Expected %" PRIu64 " %s %" PRIu64 , __pt_a, #op, __pt_b); }
#define pt_ueq(a, b) pt_uint(a, ==, b)
#define pt_une(a, b) pt_uint(a, !=, b)
#define pt_ugt(a, b) pt_uint(a, >, b)
#define pt_uge(a, b) pt_uint(a, >=, b)
#define pt_ult(a, b) pt_uint(a, <, b)
#define pt_ule(a, b) pt_uint(a, <=, b)

/**
 * When testing network services, it's useful to have a unique port for the
 * service to listen on. This function will give you that port. This function
 * returns the same port number, per test, no matter how many times it's called,
 * for the given port index.
 *
 * When you need multiple ports, increment `i` and call again for another port.
 */
uint16_t pt_get_port(uint8_t i);

/**
 * Get the name of the currently-running test
 */
const char* pt_get_name();

/**
 * Used internally by paratec. Don't mess with any of this.
 */
struct paratec {
	char *fn_name;
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
	void (*cleanup)(const char*);
};

__attribute__((format(printf, 1, 2)))
void _pt_fail(
	const char *format,
	...);

void _pt_mark(
	const char *file,
	const char *func,
	const size_t line);
