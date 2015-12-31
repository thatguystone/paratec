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
#include <inttypes.h>
#include <sched.h>
#include <stdarg.h>
#include <string.h>

#define PT_SECTION_NAME "paratec"

#define PT_XSTR(s) #s
#define PT_STR(s) PT_XSTR(s)
#define PT_PRINTF(f, a) __attribute__((format(printf, f, a)))

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
		p->fn_name_ = PT_STR(__PT_TEST(test_fn));                              \
		p->name_ = PT_STR(test_fn);                                            \
		p->fn_ = (void (*)(int64_t, uint32_t, void *))__PT_TEST(test_fn);      \
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
			  p->vec_ = tvec, p->vecisize_ = sizeof((tvec)[0]),                \
			  p->ranged_ = 1, ##__VA_ARGS__)                                   \
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
#define PTI(low, high)                                                         \
	p->range_low_ = low, p->range_high_ = high, p->ranged_ = 1

/**
 * This test is a benchmark
 */
#define PTBENCH() p->bench_ = 1

#ifdef __cplusplus
extern "C" {
#endif

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
PT_PRINTF(1, 2) void pt_set_iter_name(const char *format, ...);

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
	int ranged_;
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

__attribute__((noreturn)) PT_PRINTF(1, 2) void _pt_fail(const char *msg, ...);
void _pt_mark(const char *file, const char *func, const size_t line);

/**
 * Mark that the test hit this line
 */
#define pt_mark() _pt_mark(__FILE__, __func__, __LINE__)

/**
 * Fail right now with the given message.
 */
#define pt_fail(msg, ...)                                                      \
	pt_mark();                                                                 \
	_pt_fail(msg, ##__VA_ARGS__)

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
 * A most basic assertion. If false, fail.
 */
#define pt(cond, ...)                                                          \
	if (!(cond)) {                                                             \
		pt_fail("`%s` failed", PT_STR(cond));                                  \
	}                                                                          \
	pt_mark()

#define __PT_FNS                                                               \
	X(eq, ==, std::equal_to)                                                   \
	X(ne, !=, std::not_equal_to)                                               \
	X(gt, >, std::greater)                                                     \
	X(ge, >=, std::greater_equal)                                              \
	X(lt, <, std::less)                                                        \
	X(le, <=, std::less_equal)

#define __PT_ASSERT(name, type)                                                \
	PT_PRINTF(3, 4) void _pt_##name(type expect, type got, const char *msg, ...)

#define __PT_ASSERTS(name)                                                     \
	__PT_ASSERT(name, int64_t);                                                \
	__PT_ASSERT(u##name, uint64_t);                                            \
	__PT_ASSERT(f##name, double);                                              \
	__PT_ASSERT(s##name, const char *);

#define X(fn, h, c) __PT_ASSERTS(fn)
__PT_FNS
#undef X

__PT_ASSERT(sin, const char *);
__PT_ASSERT(sni, const char *);

#ifndef __cplusplus

// Clang really messes these up...
// clang-format off

#define __PT_GEN_INT(fn)                                                       \
	int8_t: fn,                                                                \
	const int8_t: fn,                                                          \
	int16_t: fn,                                                               \
	const int16_t: fn,                                                         \
	int32_t: fn,                                                               \
	const int32_t: fn,                                                         \
	int64_t: fn,                                                               \
	const int64_t: fn

#define __PT_GEN_UINT(fn)                                                      \
	uint8_t: fn,                                                               \
	const uint8_t: fn,                                                         \
	uint16_t: fn,                                                              \
	const uint16_t: fn,                                                        \
	uint32_t: fn,                                                              \
	const uint32_t: fn,                                                        \
	uint64_t: fn,                                                              \
	const uint64_t: fn

#define __PT_GEN_FLOAT(fn)                                                     \
	float: fn,                                                                 \
	const float: fn,                                                           \
	double: fn,                                                                \
	const double: fn

#define __PT_GEN_STR(fn)                                                       \
	char *: fn,                                                                \
	const char *: fn

#define __PT_GENERIC(name, expect, got, ...) \
	_Generic((expect),                                                         \
		__PT_GEN_INT(_pt_##name),                                              \
		__PT_GEN_UINT(_pt_u##name),                                            \
		__PT_GEN_FLOAT(_pt_f##name),                                           \
		__PT_GEN_STR(_pt_s##name))((expect), (got), " " __VA_ARGS__)

#define __PT_IN(name, expect, got, ...) \
	_Generic((haystack),                                                       \
		__PT_GEN_STR(pt_s##name))((haystack), (needle), " " __VA_ARGS__)

// clang-format on

#else
}

#include <string>

namespace std
{

std::string to_string(const char *t);
std::string to_string(const std::string &t);
std::string to_string(nullptr_t &);
template <typename T> std::string to_string(const T *t)
{
	char buff[128];
	snprintf(buff, sizeof(buff), "%p", t);
	return buff;
}

template <> struct equal_to<void *> {
	inline bool operator()(const void *lhs, const void *rhs) const
	{
		return lhs == rhs;
	}
};

template <> struct not_equal_to<void *> {
	inline bool operator()(const void *lhs, const void *rhs) const
	{
		return lhs != rhs;
	}
};

// clang-format off
template <> struct equal_to<const char *> : equal_to<const std::string> {};
template <> struct equal_to<char *> : equal_to<const std::string> {};
template <> struct not_equal_to<const char *> : not_equal_to<const std::string> {};
template <> struct not_equal_to<char *> : not_equal_to<const std::string> {};
template <> struct greater<const char *> : greater<const std::string> {};
template <> struct greater<char *> : greater<const std::string> {};
template <> struct greater_equal<const char *> : greater_equal<const std::string> {};
template <> struct greater_equal<char *> : greater_equal<const std::string> {};
template <> struct less<const char *> : less<const std::string> {};
template <> struct less<char *> : less<const std::string> {};
template <> struct less_equal<const char *> : less_equal<const std::string> {};
template <> struct less_equal<char *> : less_equal<const std::string> {};
template <> struct equal_to<nullptr_t> : equal_to<void *> {};
template <> struct not_equal_to<nullptr_t> : not_equal_to<void *> {};
// clang-format on
}

namespace pt
{
namespace assert
{

#define __PT_FORWARD(T, U, human, opClass, ...)                                \
	va_list args;                                                              \
	va_start(args, msg);                                                       \
	pt::assert::_check<T, U, opClass<__VA_ARGS__>>(expect, got, PT_STR(human), \
												   msg, args);                 \
	va_end(args);

#define PT_FAIL_BUFF 8192

template <typename T, typename U, class Op>
PT_PRINTF(4, 0) void _check(T expect,
							U got,
							const char *op,
							const char *msg,
							va_list args)
{
	if (!Op()(expect, got)) {
		char buff[PT_FAIL_BUFF];
		buff[0] = '\0';

		/*
		 * msg[1]: used to suppress errors about empty format strings; they're
		 * meant to be empty as a work-around to having something like pt_eq()
		 * and pt_eq_msg().
		 */
		if (msg[1] != '\0') {
			vsnprintf(buff, sizeof(buff), msg, args);
		}

		_pt_fail("Expected `%s` %s `%s`%s%s", std::to_string(expect).c_str(),
				 op, std::to_string(got).c_str(), msg[1] == '\0' ? "" : " ::",
				 buff);
	}
}

template <typename T, typename U> struct In {
	inline bool operator()(const T &haystack, const U &needle) const
	{
		for (const auto &h : haystack) {
			if (h == needle) {
				return true;
			}
		}

		return false;
	}
};

template <typename U> struct In<const char *, U> {
	bool operator()(const char *haystack, const U &needle) const
	{
		return std::string(haystack).find(needle) != std::string::npos;
	}
};

template <typename U> struct In<char *, U> : In<const char *, U> {
};

template <typename U> struct In<const std::string, U> {
	bool operator()(const std::string &haystack, const U &needle) const
	{
		return haystack.find(needle) != std::string::npos;
	}
};

template <typename U> struct In<std::string, U> : In<const std::string, U> {
};

template <typename T, typename U> struct NotIn {
	inline bool operator()(const T &haystack, const U &needle) const
	{
		return !In<T, U>()(haystack, needle);
	}
};

template <typename T, typename U>
PT_PRINTF(3, 4) void _in(T expect, U got, const char *msg, ...)
{
	__PT_FORWARD(T, U, in, In, T, U);
}

template <typename T, typename U>
PT_PRINTF(3, 4) void _nin(T expect, U got, const char *msg, ...)
{
	__PT_FORWARD(T, U, not in, In, T, U);
}

#define X(fn, human, opClass)                                                  \
	template <typename T, typename U>                                          \
	PT_PRINTF(3, 4) void _##fn(T expect, U got, const char *msg, ...)          \
	{                                                                          \
		__PT_FORWARD(T, U, human, opClass, T);                                 \
	}
__PT_FNS
#undef X

#define __PT_GENERIC(name, expect, got, ...)                                   \
	pt::assert::_##name(expect, got, " " __VA_ARGS__)
#define __PT_IN(name, expect, got, ...)                                        \
	__PT_GENERIC(name, expect, got, ##__VA_ARGS__)
}
}
#endif

#define pt_eq(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(eq, expect, got, ##__VA_ARGS__)

#define pt_ne(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(ne, expect, got, ##__VA_ARGS__)

#define pt_gt(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(gt, expect, got, ##__VA_ARGS__)

#define pt_ge(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(ge, expect, got, ##__VA_ARGS__)

#define pt_lt(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(lt, expect, got, ##__VA_ARGS__)

#define pt_le(expect, got, ...)                                                \
	pt_mark();                                                                 \
	__PT_GENERIC(le, expect, got, ##__VA_ARGS__)

#define pt_in(haystack, needle, ...)                                           \
	pt_mark();                                                                 \
	__PT_IN(in, haystack, needle, ##__VA_ARGS__)

#define pt_nin(haystack, needle, ...)                                          \
	pt_mark();                                                                 \
	__PT_IN(nin, haystack, needle, ##__VA_ARGS__)
