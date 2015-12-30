/**
 * Run parallel unit tests in C/C++.
 *
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
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
#elif(defined(__APPLE__) && defined(__MACH__))
#define PT_DARWIN
#define PT_SECTION "__DATA," PT_SECTION_NAME
#else
#error Unsupported platform
#endif

#define __PT_TEST(test_fn) __paratec_fn_##test_fn
#define __PT_STRUCT(test_fn) __paratec_obj_##test_fn
#define __PT_PTR(test_fn) __paratec_sobj_##test_fn

#define __PARATEC(test_fn, ...)                                                \
	static struct _paratec __PT_STRUCT(test_fn);                               \
	static struct _paratec *__PT_PTR(test_fn)                                  \
		__attribute__((used, section(PT_SECTION))) = &__PT_STRUCT(test_fn);    \
	static __attribute__((constructor)) void __paratec_##test_fn##_ctor(void)  \
	{                                                                          \
		struct _paratec *p = __PT_PTR(test_fn);                                \
		p->fn_name_ = PTSTR(__PT_TEST(test_fn));                                \
		p->name_ = PTSTR(test_fn);                                              \
		p->fn_ = (void (*)(int64_t, uint32_t, void *))__PT_TEST(test_fn);       \
		__VA_ARGS__;                                                           \
	};

/**
 * Run a unit test.
 */
#define PARATEC(test_fn, ...)                                                  \
	static void __PT_TEST(test_fn)(int64_t, uint32_t, void *);                 \
	__PARATEC(test_fn, __VA_ARGS__)                                            \
	static void __PT_TEST(test_fn)(int64_t _i __attribute__((unused)),         \
								   uint32_t _N __attribute__((unused)),        \
								   void *_t __attribute__((unused)))

/**
 * Run a test for each item in `vec`.
 */
#define PARATECV(test_fn, tvec, ...)                                           \
	static void __PT_TEST(test_fn)(int64_t, uint32_t, typeof(tvec[0]) *);      \
	__PARATEC(test_fn, PTI(0, (sizeof(tvec) / sizeof((tvec)[0]))),             \
			  p->vec_ = tvec, p->vecisize_ = sizeof((tvec)[0]), ##__VA_ARGS__)   \
	static void __PT_TEST(test_fn)(int64_t _i __attribute__((unused)),         \
								   uint32_t _N __attribute__((unused)),        \
								   typeof(tvec[0]) *_t                         \
								   __attribute__((unused)))

/**
 * Run a unit test, expecting the given exit status.
 */
#define PTEXIT(s) p->exit_status_ = s

/**
 * Run a unit test, expecting the given signal.
 */
#define PTSIG(s) p->signal_num_ = s

/**
 * Expect this test to have failed assertions. Kinda weird to use, but no
 * judgment.
 */
#define PTFAIL() p->expect_fail_ = 1

/**
 * Run a unit test with a specific timeout (in seconds as a double).
 */
#define PTTIME(s) p->timeout_ = s

/**
 * Add a setup function to the test that runs in the isolated environment.
 */
#define PTUP(fn) p->setup_ = fn

/**
 * Add a teardown function to the test that runs in the isolated environment.
 */
#define PTDOWN(fn) p->teardown_ = fn

/**
 * Cleanup after a test, outside of the testing environment. Even if a test
 * fails, this function is run. No assertions may be run in this callback as
 * it's only a cleanup function an not part of your test; running any assertions
 * will result in undefined behavior.
 */
#define PTCLEANUP(fn) p->cleanup_ = fn

/**
 * Run a test multiple times, over the given range.
 * Does: for (i = a; i < b; i++);
 */
#define PTI(a, b) p->range_low_ = a, p->range_high_ = b

/**
 * This test is a benchmark
 */
#define PTBENCH() p->bench_ = 1

/**
 * Mark that the test hit this line
 */
#define pt_mark() pt_mark_(__FILE__, __func__, __LINE__)

/**
 * Fail right now with the given message.
 */
#define pt_fail(msg, ...)                                                      \
	pt_mark();                                                                 \
	pt_fail_(msg, ##__VA_ARGS__)

/**
 * A most basic assertion. If false, fail.
 */
#define pt(cond)                                                               \
	if (!(cond)) {                                                             \
		pt_fail("`%s` failed", #cond);                                         \
	}                                                                          \
	pt_mark()

/**
 * Like pt(), but allows you to give your own message.
 */
#define pt_msg(cond, msg, ...)                                                 \
	if (!(cond)) {                                                             \
		pt_fail(msg, ##__VA_ARGS__);                                           \
	}                                                                          \
	pt_mark()

/**
 * String assertions with custom messages
 */
#define pt_str_msg(a, op, b, msg, ...)                                         \
	({                                                                         \
		const char *__pt_a = (a);                                              \
		const char *__pt_b = (b);                                              \
		pt_msg(0 op strcmp(__pt_a, __pt_b), msg, ##__VA_ARGS__);               \
	})
#define pt_seq_msg(a, b, msg, ...) pt_str(a, ==, b, msg, ##__VA_ARGS__)
#define pt_sne_msg(a, b, msg, ...) pt_str(a, !=, b, msg, ##__VA_ARGS__)
#define pt_ss_msg(a, b, msg, ...)                                              \
	{                                                                          \
		const char *__pt_a = (a);                                              \
		const char *__pt_b = (b);                                              \
		pt_msg(strstr(a, b) != NULL, msg, ##__VA_ARGS__);                      \
	}

/**
 * String assertions
 */
#define pt_str(a, op, b)                                                       \
	({ pt_str_msg(a, op, b, "Expected `%s` %s `%s`", __pt_a, #op, __pt_b); })
#define pt_seq(a, b) pt_str(a, ==, b)
#define pt_sne(a, b) pt_str(a, !=, b)
#define pt_ss(a, b)                                                            \
	pt_ss_msg(a, b, "`%s` does not contain `%s`", __pt_a, __pt_b)

/**
 * Binary assertions with custom messages
 */
#define pt_mem_msg(a, op, b, len, msg, ...)                                    \
	({                                                                         \
		const void *__pt_a = (a);                                              \
		const void *__pt_b = (b);                                              \
		const size_t n = (len);                                                \
		pt_msg(0 op memcmp(__pt_a, __pt_b, n), msg, ##__VA_ARGS__);            \
	})
#define pt_meq_msg(a, b, len, msg, ...)                                        \
	pt_mem_msg(a, ==, b, len, msg, ##__VA_ARGS__)
#define pt_mne_msg(a, b, len, msg, ...)                                        \
	pt_mem_msg(a, !=, b, len, msg, ##__VA_ARGS__)

/**
 * Binary assertions
 */
#define pt_mem(a, op, b, len)                                                  \
	({ pt_mem_msg(a, op, b, len, "Expected `%s` %s `%s`", #a, #op, #b); })
#define pt_meq(a, b, len) pt_mem(a, ==, b, len)
#define pt_mne(a, b, len) pt_mem(a, !=, b, len)

/**
 * Pointer assertions with custom messages
 */
#define pt_ptr_msg(a, op, b, msg, ...)                                         \
	({                                                                         \
		const void *__pt_a = (a);                                              \
		const void *__pt_b = (b);                                              \
		pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__);                          \
	})
#define pt_peq_msg(a, b, msg, ...) pt_ptr_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_pne_msg(a, b, msg, ...) pt_ptr_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_pgt_msg(a, b, msg, ...) pt_ptr_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_pge_msg(a, b, msg, ...) pt_ptr_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_plt_msg(a, b, msg, ...) pt_ptr_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_ple_msg(a, b, msg, ...) pt_ptr_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Pointer assertions
 */
#define pt_ptr(a, op, b)                                                       \
	({ pt_ptr_msg(a, op, b, "Expected %p %s %p", __pt_a, #op, __pt_b); })
#define pt_peq(a, b) pt_ptr(a, ==, b)
#define pt_pne(a, b) pt_ptr(a, !=, b)
#define pt_pgt(a, b) pt_ptr(a, >, b)
#define pt_pge(a, b) pt_ptr(a, >=, b)
#define pt_plt(a, b) pt_ptr(a, <, b)
#define pt_ple(a, b) pt_ptr(a, <=, b)

/**
 * Integer assertions with custom messages
 */
#define pt_int_msg(a, op, b, msg, ...)                                         \
	({                                                                         \
		const int64_t __pt_a = (a);                                            \
		const int64_t __pt_b = (b);                                            \
		pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__);                          \
	})
#define pt_eq_msg(a, b, msg, ...) pt_int_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_ne_msg(a, b, msg, ...) pt_int_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_gt_msg(a, b, msg, ...) pt_int_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_ge_msg(a, b, msg, ...) pt_int_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_lt_msg(a, b, msg, ...) pt_int_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_le_msg(a, b, msg, ...) pt_int_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Integer assertions
 */
#define pt_int(a, op, b)                                                       \
	({                                                                         \
		pt_int_msg(a, op, b, "Expected %'" PRId64 " %s %'" PRId64              \
							 " (off by %'" PRId64 ")",                         \
				   __pt_a, #op, __pt_b, __pt_b - __pt_a);                      \
	})
#define pt_eq(a, b) pt_int(a, ==, b)
#define pt_ne(a, b) pt_int(a, !=, b)
#define pt_gt(a, b) pt_int(a, >, b)
#define pt_ge(a, b) pt_int(a, >=, b)
#define pt_lt(a, b) pt_int(a, <, b)
#define pt_le(a, b) pt_int(a, <=, b)

/**
 * Unsigned integer assertions with custom messages
 */
#define pt_uint_msg(a, op, b, msg, ...)                                        \
	({                                                                         \
		const uint64_t __pt_a = (a);                                           \
		const uint64_t __pt_b = (b);                                           \
		pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__);                          \
	})
#define pt_ueq_msg(a, b, msg, ...) pt_uint_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_une_msg(a, b, msg, ...) pt_uint_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_ugt_msg(a, b, msg, ...) pt_uint_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_uge_msg(a, b, msg, ...) pt_uint_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_ult_msg(a, b, msg, ...) pt_uint_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_ule_msg(a, b, msg, ...) pt_uint_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Unsigned integer assertions
 */
#define pt_uint(a, op, b)                                                      \
	({                                                                         \
		pt_uint_msg(a, op, b, "Expected %'" PRIu64 " %s %'" PRIu64             \
							  " (off by %'" PRId64 ")",                        \
					__pt_a, #op, __pt_b, __pt_b - __pt_a);                     \
	})
#define pt_ueq(a, b) pt_uint(a, ==, b)
#define pt_une(a, b) pt_uint(a, !=, b)
#define pt_ugt(a, b) pt_uint(a, >, b)
#define pt_uge(a, b) pt_uint(a, >=, b)
#define pt_ult(a, b) pt_uint(a, <, b)
#define pt_ule(a, b) pt_uint(a, <=, b)

/**
 * Floating point assertions with custom messages
 */
#define pt_f_msg(a, op, b, msg, ...)                                           \
	({                                                                         \
		const double __pt_a = (a);                                             \
		const double __pt_b = (b);                                             \
		pt_msg(__pt_a op __pt_b, msg, ##__VA_ARGS__);                          \
	})
#define pt_feq_msg(a, b, msg, ...) pt_f_msg(a, ==, b, msg, ##__VA_ARGS__)
#define pt_fne_msg(a, b, msg, ...) pt_f_msg(a, !=, b, msg, ##__VA_ARGS__)
#define pt_fgt_msg(a, b, msg, ...) pt_f_msg(a, >, b, msg, ##__VA_ARGS__)
#define pt_fge_msg(a, b, msg, ...) pt_f_msg(a, >=, b, msg, ##__VA_ARGS__)
#define pt_flt_msg(a, b, msg, ...) pt_f_msg(a, <, b, msg, ##__VA_ARGS__)
#define pt_fle_msg(a, b, msg, ...) pt_f_msg(a, <=, b, msg, ##__VA_ARGS__)

/**
 * Floating point assertions
 */
#define pt_f(a, op, b)                                                         \
	({                                                                         \
		pt_f_msg(a, op, b, "Expected %'f %s %'f (off by %'f)", __pt_a, #op,    \
				 __pt_b, __pt_b - __pt_a);                                     \
	})
#define pt_feq(a, b) pt_f(a, ==, b)
#define pt_fne(a, b) pt_f(a, !=, b)
#define pt_fgt(a, b) pt_f(a, >, b)
#define pt_fge(a, b) pt_f(a, >=, b)
#define pt_flt(a, b) pt_f(a, <, b)
#define pt_fle(a, b) pt_f(a, <=, b)

/**
 * For asserting return values that also set errno.
 */
#define pt_eno(a, b)                                                           \
	({                                                                         \
		pt_eq_msg(a, b, "Expected %'" PRId64 " == %'" PRId64 ". Error %d: %s", \
				  __pt_a, __pt_b, errno, strerror(errno));                     \
	})
#define pt_noe(a)                                                              \
	pt_ge_msg(a, 0, "Expected no error. Error %d: %s", errno, strerror(errno));

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wait for the given condition to become true
 */
#define pt_wait_for(cond)                                                      \
	({                                                                         \
		unsigned __pt_spins = 0;                                               \
		while (!(cond)) {                                                      \
			if (++__pt_spins > 1024) {                                         \
				sched_yield();                                                 \
			}                                                                  \
		}                                                                      \
	})

/**
 * Mark the current test as skipped and stop running it immediately.
 */
void pt_skip(void);

/**
 * When testing network services, it's useful to have a unique port for the
 * service to listen on. This function will give you that port. This function
 * returns the same port number, per test, no matter how many times it's
 * called, for the given port index.
 *
 * When you need multiple ports, increment `i` and call again for another port.
 */
uint16_t pt_get_port(uint8_t i);

/**
 * Get the name of the currently-running test
 */
const char *pt_get_name(void);

/**
 * Set the name of the current iteration test to something besides a number.
 */
__attribute__((format(printf, 1, 2))) void pt_set_iter_name(const char *format,
															...);

/**
 * Used internally by paratec. Don't mess with any of this.
 */
struct _paratec {
	const char *fn_name_;
	const char *name_;
	int exit_status_;
	int signal_num_;
	double timeout_;
	int expect_fail_;
	int64_t range_low_;
	int64_t range_high_;
	void *vec_;
	size_t vecisize_;
	int bench_;
	void (*fn_)(int64_t, uint32_t, void *);
	void (*setup_)(void);
	void (*teardown_)(void);
	void (*cleanup_)(void);
};

__attribute__((format(printf, 1, 2),
			   noreturn)) void pt_fail_(const char *format, ...);

void pt_mark_(const char *file, const char *func, const size_t line);

#ifdef __cplusplus
}
#endif