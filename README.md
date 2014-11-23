# Paratec: Parallel Testing in C [![Build Status](https://travis-ci.org/thatguystone/paratec.svg?branch=master)](https://travis-ci.org/thatguystone/paratec)

Unit testing is hard enough already, so let's try to make it a bit easier. Paratec is a simple unit testing framework that stays out of your way while making your life easier. Tests are always run in isolation from each other and in parallel, so everything is always fast and safe.

## Quick Start

Drop `paratec.{c,h}` in your project, add them to your build, and you should be off running.

## Let's get testing

It's really simple to get testing. Just write your test cases, link in `paratec.c` (which includes a main() function), and execute the binary.

```c
#include "paratec.h"

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
```

And that's it. When you link in `paratec.c`, it will figure the rest out.

## Declaring Tests

`PARATEC` is used to declare tests, but you might have noticed `PTFAIL()` also in there. `PTFAIL()` just changes the test's behavior; there are a bunch of other options:

* `PTCLEANUP(fn)`: always runs after the test has completed, even in case of failure, outside of the test's environment to cleanup anything it  might have left behind. Making any assertions in this callback will result in undefined behavior.
* `PTDOWN(fn)`: add a teardown function to the test; only runs if the test succeeds; you may run assertions here
* `PTEXIT(status)`: expect this test to exit with the given exit status
* `PTFAIL()`: expect this test to fail
* `PTI(low, high)`: run the test for `(i = low; i < high; i++)`, passing the current value of the iterator as `_i` to the test function
* `PTSIG(num)`: expect this test to raise the given signal
* `PTTIME(sec)`: set a test-specific timeout, in seconds as a double
* `PTUP(fn)`: add a setup function to the test; you may run assertions here

The function given to `PTDOWN` and `PTUP` must be of type `void (*fn)()`.

The function given to `PTCLEANUP` must be of type `void (*fn)(const char *test_name)`, since it has no way of finding out the name of the test it's cleaning up after.

## API

`uint16_t pt_get_port(uint8_t i)`

When testing a network server, picking out ports for running tons of tests simultaneously is error-prone. For that reason, you can find the awesome function `pt_get_port(uint8_t i)` at your disposal. This function will give you a unique port for your test, based on the index given. If you only need 1 port, then `i` will be 0; if you need more ports, increment `i` until you have what you need. All ports given are unique amongst all tests.

`const char* pt_get_name()`

Get the name of the currently-running test.

`void pt_set_iter_name(const char *format, ...)`

When running an iteration test, sometimes the name `test-name:1` (iteration with index 1) isn't helpful. This function allows you to change it to something like `test-name:good-description`. Use it like you would any printf() function.

## Assertions

Note: Aside from the basic assertions, each assertion has a `*_msg()` counterpart that allows for a custom message with optional formatting.

### Basic Assertions

* `pt(cond)`: assert that the condition is true
* `pt_msg(cond, msg, ...)`: assert with a message with optional formatting
* `pt_fail(msg, ...)`: fail this test immediately
* `pt_mark()`: mark the test as having hit this point (makes for nicer debugging output)

### String Assertions

* `pt_str(a, op, b)`: assert `op` on two strings
* `pt_seq(a, b)`: assert that two strings are equal
* `pt_sne(a, b)`: assert that two strings aren't equal
* `pt_ss(a, b)`: assert that b is in a

### Binary Assertions

* `pt_mem(a, op, b, len)`: assert `op` on two memory segments
* `pt_meq(a, b, len)`: assert that two memory segments are equal
* `pt_mne(a, b, len)`: assert that two memory segments aren't equal

### Pointer Assertions

* `pt_peq(a, b)`: assert a == b
* `pt_pne(a, b)`: assert a != b
* `pt_pgt(a, b)`: assert a > b
* `pt_pge(a, b)`: assert a >= b
* `pt_plt(a, b)`: assert a < b
* `pt_ple(a, b)`: assert a <= b

### Signed Integer Assertions

* `pt_eq(a, b)`: assert a == b
* `pt_ne(a, b)`: assert a != b
* `pt_gt(a, b)`: assert a > b
* `pt_ge(a, b)`: assert a >= b
* `pt_lt(a, b)`: assert a < b
* `pt_le(a, b)`: assert a <= b

### Unsigned Integer Assertions

* `pt_ueq(a, b)`: assert a == b
* `pt_une(a, b)`: assert a != b
* `pt_ugt(a, b)`: assert a > b
* `pt_uge(a, b)`: assert a >= b
* `pt_ult(a, b)`: assert a < b
* `pt_ule(a, b)`: assert a <= b

## Running Tests

I'm sure you're already familiar with how to build things in C, but just in case:

```bash
$ gcc -g math_test.c paratec.c -o paratec && ./paratec
```

The paratec binary comes equipped with the following options:

 Short Option | Long Option    | Env Variable   | Description
 ------------ | -------------- | -------------- | -----------
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
1. `-vv`: print skipped tests
1. `-vvv`: print stdout/stderr of passed tests

`PTVERBOSE=` is the equivalent of `-v`; `PTVERBOSE=vv` is the equivalent of `-vvv`, and so forth.

## Test Output

A lot of information is output on failure. Let's take the output of the `nofork` test as an example:

```
FFFF..
33%: 6 tests, 0 errors, 4 failures, 0 skipped. Ran in 0.015742s
     FAIL : nofork_fail_outside (0.000229s) : test start (test/nofork.c:18) : here
     FAIL : nofork_fail_outside_marked (0.000247s) : test/nofork.c:53 (test/nofork.c:18) : here
     FAIL : nofork_mark (0.000216s) : test/nofork.c:43 : Expected `a` == `b`
     FAIL : nofork_failure (0.000218s) : test/nofork.c:37 : Expected `a` == `b`
```

The first line is a summary that prints while the test is running. It shows that 4 tests failed (`F`) and 2 passed (`.`).

The next line gives a complete summary once all tests have finished, and it includes the total runtime.

The next 4 lines highlight the failed tests, showing the test name, how long the test took to run, and where the test failed. `nofork_fail_outside` is a test where the assertion fails outside of the testing function; no assertions were ever hit inside the testing function. To note this, it outputs "test start" as the failure position in the test, and the in parenthesis gives the location of the failed assertion.

Like the first failure, the second one failed outside the test function, but in this case, the test function executed an assertion before calling the failing function. This is noted by the location of the last assertion in the test function and the location of the failed assertion in parenthesis.

The final two tests fail inside their testing functions, and they very simply output where the test failed.

## Supported Platforms

Tested on Debian/testing, OSX 10.10, and travis-ci's environment-du-jour.
