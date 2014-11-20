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

* `PTCLEANUP(fn)`: always runs after the test has completed, even in case of
  failure, outside of the test's environment to cleanup after the test. Making
  any assertions in this callback will result in undefined behavior (this
  essentially a free function, not part of your test!).
* `PTDOWN(fn)`: add a teardown function to the test; only runs if the test
  succeeds; you may run assertions here
* `PTEXIT(status)`: expect this test to exit with the given exit status
* `PTFAIL()`: expect this test to fail
* `PTI(low, high)`: run the test for `(i = low; i < high; i++)`, passing the
  current value of the iterator as `_i` to the test function
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

### Binary Assertions

* `pt_mem(a, op, b, len)`: assert `op` on two memory segments
* `pt_meq(a, b, len)`: assert that two memory segments are equal
* `pt_mne(a, b, len)`: assert that two memory segments aren't equal

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
  `-f`        |  `--filter`    |  `PTFILTER`    |  Filter which tests are run: if a test has a given filter prefix, it will be run. This option may be given multiple times, and each value may be comma-separated to provide multiple filters. For example: with 4 tests, add, subtract, multiple, divide, and the filters "add", "subtract,divide", only multiply would not be run. On the command line, this would be `./paratec -f add --filter=subtract,divide`. As an environment variable, this would be `PTFILTER=add,subtract,divide`.
  `-j`        |  `--jobs`      |  `PTJOBS`      |  Set the number of parallel tests to run. By default, this uses the number of CPUs on the machine + 1. Any positive integer > 0 is fine.
  `-n`        |  `--nocapture` |  `PTNOCAPTURE` |  Don't capture test output on stdout/stderr.
  `-p`        |  `--port`      |  `PTPORT`      |  Specify where pt_get_port() should start handing out ports.
  `-s`        |  `--nofork`    |  `PTNOFORK`    |  Throw caution to the wind and don't isolate test cases. This is useful for running tests in `gdb`.
  `-t`        |  `--timeout`   |  `PTTIMEOUT`   |  Change the global timeout from 5 seconds to the given value.
  `-v`        |  `--verbose`   |  `PTVERBOSE`   |  Print information about tests that succeed.

## Supported Platforms

Tested on Debian/testing, OSX 10.10, and travis-ci's environment-du-jour.
