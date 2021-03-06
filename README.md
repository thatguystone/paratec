# Paratec: Parallel Testing for C/C++ [![Build Status](https://travis-ci.org/thatguystone/paratec.svg?branch=master)](https://travis-ci.org/thatguystone/paratec)

Unit testing is hard enough already, so let's try to make it a bit easier. Paratec is a simple unit testing framework that stays out of your way while making your life easier. Tests are always run in isolation from each other and in parallel, so everything is always fast and safe.

## Quick Start

### Debian-based

1. Add `deb http://deb.stoney.io/ testing/` to your sources.list.
1. Install the archive keys: `sudo apt-get install stoney.io-archive-keyring`
1. `sudo aptitude install libparatec-dev`

### From Source

Grab the source, and a simple `sudo make install` will get everything in place.

## Let's get testing

It's really simple to get testing.

```c
#include "paratec.h"

struct table_test {
	int a;
	int b;
	int c;
};

// Declare a test case
PARATEC(add)
{
	int a = 1;
	pt_gt(a, 0);
}

// Expect this test case to fail
PARATEC(add, PTFAIL())
{
	int a = 0;
	pt_gt(a, 100);
}

// Create a benchmark
PARATEC(bench, PTBENCH())
{
	uint32_t i;

	for (i = 0; i < _N; i++) {
		// Code to time goes here
	}
}

static struct table_test _table[] = {
	{	.a = 0,
		.b = 0,
		.c = 0,
	},
	{	.a = 1,
		.b = 1,
		.c = 1,
	},
	{	.a = 2,
		.b = 2,
		.c = 2,
	},
};

// Run a test case for each member of _table
PARATECV(table_test, _table)
{
	// _t is set automatically to the item to test in _table
	// _i is set automatically to the index of the item in _table

	pt_eq(_t->a, _i);
	pt_eq(_t->b, _i);
	pt_eq(_t->c, _i);
}
```

And that's it. When you link in `-lparatec`, it will figure the rest out.

## Declaring Tests

`PARATEC` is used to declare tests, but you might have noticed `PTFAIL()` also in there. `PTFAIL()` just changes the test's behavior; there are a bunch of other options:

* `PTBENCH()`: declare a benchmark; this is only run when benchmarks are enabled.
* `PTCLEANUP(fn)`: always runs after the test has completed, even in case of failure, outside of the test's environment to cleanup anything it  might have left behind. Making any assertions in this callback will result in undefined behavior.
* `PTDOWN(fn)`: add a teardown function to the test; only runs if the test succeeds; you may run assertions here
* `PTEXIT(status)`: expect this test to exit with the given exit status
* `PTFAIL()`: expect this test to fail
* `PTI(low, high)`: run the test for `(i = low; i < high; i++)`, passing the current value of the iterator as `_i` to the test function
* `PTSIG(num)`: expect this test to raise the given signal
* `PTTIME(sec)`: set a test-specific timeout, in seconds as a double
* `PTUP(fn)`: add a setup function to the test; you may run assertions here

The function given to `PTDOWN` and `PTUP` must be of type `void (*fn)()`.

The function given to `PTCLEANUP` must be of type `void (*fn)()`, and it may use `pt_get_port()` and `pt_get_name()` to find out which test it is cleaning up after.

### Table Tests

Table tests are useful for when you need to test a single thing with a bunch of different inputs. Rather than copy-pasting the same code over and over again, modifying only the arguments, you can create a table (as seen in the example) and have paratec iterate it for you. This has the advantage that each iteration runs in its own environment and that each iteration is a separate test.

`PARATECV(test_name, _table_vector)` is used to create such a test, and it takes all the same arguments as `PARATEC()`.

### Benchmarks

Benchmarks are, by default, skipped. In order to run them, they must be enabled with `-b`/`--bench`/`PTBENCH=1`. Benchmarks are like any other test, except that they must run what they want to time in a loop (as in the example).

A benchmark may be called multiple times as Paratec tries to scale the test to get good timings. Unlike a normal test, however, any cleanup function given is run _after_ all iterations and timings have finished. Also, any cleanup and teardown functions will be called directly before and after every set of iterations; that is, setup and teardown functions may be called multiple times for each benchmark.

## API

`uint16_t pt_get_port(uint8_t i)`

When testing a network server, picking out ports for running tons of tests simultaneously is error-prone. For that reason, you can find the awesome function `pt_get_port(uint8_t i)` at your disposal. This function will give you a unique port for your test, based on the index given. If you only need 1 port, then `i` will be 0; if you need more ports, increment `i` until you have what you need. All ports given are unique amongst all tests.

`void pt_skip()`

Mark the current test as skipped and stop running it immediately.

`const char* pt_get_name()`

Get the name of the currently-running test.

`void pt_set_iter_name(const char *format, ...)`

When running an iteration test, sometimes the name `test-name:1` (iteration with index 1) isn't helpful. This function allows you to change it to something like `test-name:good-description`. Use it like you would any printf() function.

## Assertions

All assertions generate helpful messages on failure, but sometimes you want to extra debugging info. All assertions accept extra format arguments; for example:

```c
pt_eq(1, 2); // No extra message
pt_eq(1, 2, "something isn't right: %d", var); // Some extra output
```

### Basic Assertions

* `pt(cond, ...)`: assert that the condition is true, with an optional message with formatting
* `pt_fail(msg, ...)`: fail this test immediately
* `pt_mark()`: mark the test as having hit this point (makes for nicer debugging output)

### Value Assertions

* `pt_eq(expect, got)`: assert that expect == got
* `pt_ne(expect, got)`: assert that expect != got
* `pt_gt(a, b)`: assert that a > b
* `pt_ge(a, b)`: assert that a >= b
* `pt_lt(a, b)`: assert that a < b
* `pt_le(a, b)`: assert that a <= b
* `pt_in(needle, haystack)`: assert that needle exists in haystack
* `pt_ni(needle, haystack)`: assert that needle does not exist in haystack

### Binary Assertions

* `pt_meq(expect, got, len)`: assert that two memory segments are equal
* `pt_mne(expect, got, len)`: assert that two memory segments aren't equal

### Errno Assertions

* `pt_ner(err)`: assert err != -1, where a is some error return value. The error message includes strerror(errno).

### C++ Assertions

For C++, you may extend the value assertions using C++'s operator overrides. For this to work properly, for each type you intend to compare, you need to overload `pt::assert::toString(Type t)` so that your type can be printed into failure messages.

## Running Tests

The paratec binary comes equipped with the following options:

 Short Option | Long Option    | Env Variable   | Description
 ------------ | -------------- | -------------- | -----------
  `-b`        |  `--bench`     |  `PTBENCH`     |  Run benchmarks
  `-d`        |  `--bench-dur` |  `PTBENCHDUR`  |  Run each benchmark for the given number of seconds. By default, each has 1 second.
  `-e`        |  `--exit-fast` |  `PTEXITFAST`  |  After a test has finished, exit without calling any atexit() or on_exit() functions. When running tons of tests, this can speed things up if you don't care about cleanup or coverage.
  `-f`        |  `--filter`    |  `PTFILTER`    |  See [test filtering](#test-filtering). May be given multiple times.
  `-j`        |  `--jobs`      |  `PTJOBS`      |  Set the number of parallel tests to run. By default, this uses the number of CPUs on the machine + 1. Any positive integer > 0 is fine.
  `-n`        |  `--nocapture` |  `PTNOCAPTURE` |  Don't capture test output on stdout/stderr.
  `-p`        |  `--port`      |  `PTPORT`      |  Specify where pt_get_port() should start handing out ports.
  `-s`        |  `--nofork`    |  `PTNOFORK`    |  Throw caution to the wind and don't isolate test cases. This is useful for running tests in `gdb`.
  `-t`        |  `--timeout`   |  `PTTIMEOUT`   |  Change the global timeout from 5 seconds to the given value.
  `-v`        |  `--verbose`   |  `PTVERBOSE`   |  Be more verbose with the test summary. See [verbosity](#verbosity).

### Test Filtering

Test cases can be filtered for selective runs. Filters match on test name prefix; that is, a filter of `test` will match both `test_one`and `test_two`, whereas a filter of `test_two` will _only_ match `test_two`.

Tests may also be negatively filtered, such that a filter of `-test_two` would, in the previous example, only match `test_one`.

Filters may be comma-separated, given multiple times, or any combination thereof.

The following are all valid filters:

1. `-f test_`: only run tests starting with "test_"
1. `--filter=test_`: same as above
1. `--filter=test_ -f -test_two`: only run tests starting with "test_", except "test_two"
1. `--filter=test_two -f -test_two`: run no tests (first filters for test_two, then removes test_two).
1. `--filter=test_,-test_two`: only run tests starting with "test_", except "test_two"
1. `PTFILTER=test_,-test_two`: only run tests starting with "test_", except "test_two"

### Verbosity

There are 3 levels of verbosity:

1. `-v`: print tests that succeeded
1. `-vv`: print skipped/disabled tests
1. `-vvv`: print stdout/stderr of passed tests

`PTVERBOSE=` is the equivalent of `-v`; `PTVERBOSE=vv` is the equivalent of `-vvv`, and so forth.

## Test Output

A lot of information is output on failure. Let's take the output of the `nofork` test as an example:

```
FFF..
0%: of 5 tests run, 2 OK, 0 errors, 3 failures, 0 skipped. Ran 0 benches. Took 0.337784s (tests used 0.610332s)
    FAIL : a (0.337354s) : src/file.cpp:517 (last test assert: test start) : Expected `0` == `1`
    FAIL : b (0.150189s) : src/file.cpp:517 (last test assert: src/file.cpp:527) : Expected `0` == `1`
    FAIL : c (0.122789s) : src/file.c:512 : Expected `0` == `1`
```

The first line is a summary that prints while the test is running. It shows that 3 tests failed (`F`) and 2 passed (`.`).

The next line gives a complete summary once all tests have finished, and it includes the total runtime.

The next 3 lines highlight the failed tests, showing the test name, how long the test took to run, and where the test failed. `a` is a test where the assertion fails outside of the testing function; no assertions were ever hit inside the testing function. To note this, it outputs the location of the failure, and the last run assertion in the test case in parenthesis (in this case, "test start").

Like `a`, `b` failed outside the test function, but in this case, the test function executed an assertion before calling the failing function. This is noted by the location of the failed assertion followed by the last assertion in the test function in parenthesis.

The final test (`c`) failed inside its testing functions, and it very simply output where it failed.

## Supported Platforms

Tested on Debian/testing, OSX 10.11, and travis-ci's environment-du-jour.
