/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "paratec.h"

#define INDENT "    "
#define STDPREFIX INDENT INDENT " | "
#define LINE_SIZE 2048

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

struct job {
	pid_t pid;
	int stdout;
	int stderr;

	uint32_t i; // Index of the test in tests.all
	int64_t end_at;
	int timed_out;
	char last_line[LINE_SIZE];
};

struct buff {
	char *str;
	size_t len;
	size_t alloc;
};

struct test {
	char name[LINE_SIZE];
	int exit_status;
	int signal_num;
	int64_t i; // For ranged tests, the i in the range
	struct buff stdout;
	struct buff stderr;
	char last_line[LINE_SIZE];
	struct paratec *p;
	struct {
		int run:1;
		int filtered_run:1;
		int passed:1;
		int timed_out:1;
	} flags;
};

struct tests {
	uint32_t passes;
	uint32_t errors;
	uint32_t failures;

	uint32_t c;
	uint32_t allocated;
	uint32_t enabled;
	uint32_t next_test;
	struct test *all;
};

static uint32_t _max_jobs;
static uint32_t _timeout;
static int _verbose;

// Only for use in testing processes
static struct job *_tjob;

static int64_t _now()
{
	int err;
	struct timespec ts;

	err = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (err != 0) {
		perror("failed to get time");
		exit(1);
	}

	return (((int64_t)ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

static uint32_t _get_cpu_count()
{
	int count;

	count = sysconf(_SC_NPROCESSORS_ONLN);
	if (count > 0) {
		return count;
	}

	return 1;
}

static void _signal_wait()
{
	int err;
	sigset_t mask;
	siginfo_t info;
	struct timespec timeout = {
		.tv_sec = 0,
		.tv_nsec = 10 * 1000,
	};

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	err = sigtimedwait(&mask, &info, &timeout);
	if (err < 0 && errno != EAGAIN) {
		perror("sigtimedwait failed");
		exit(1);
	}
}

static void _setup_signals()
{
	int err;
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	err = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (err < 0) {
		perror("failed to change signal mask");
		exit(1);
	}
}


static void _filter_tests(struct tests *ts, const char *filter)
{
	uint32_t i;

	for (i = 0; i < ts->c; i++) {
		struct test *t = ts->all + i;
		if (strstr(t->name, filter) == t->name) {
			t->flags.filtered_run = 1;
		} else {
			t->flags.run = 0;
		}
	}
}

static void _print_opt(char *s, char *arg, char *desc)
{
	printf(INDENT "-%s, --%s\n", s, arg);
	printf(INDENT INDENT "%s\n", desc);
}

static void _print_usage(char **argv)
{
	printf("Usage: %s [OPTION]...\n", argv[0]);
	printf("\n");
	_print_opt("f FILTER", "filter=FILTER", "only run tests prefixed with FILTER");
	_print_opt("h", "help", "print this messave");
	_print_opt("v", "verbose", "print information about succeeding tests");

	exit(2);
}

static void _set_opts(struct tests *ts, int argc, char **argv)
{
	struct option lopts[] = {
		{ "filter", required_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "verbose", no_argument, &_verbose, 'v' },
		{ NULL, 0, NULL, 0 },
	};

	_max_jobs = _get_cpu_count() + 1;
	_timeout = 5;

	while (1) {
		char c = getopt_long(argc, argv, "f:hv", lopts, NULL);
		if (c == -1) {
			break;
		}

		switch (c) {
			case 0:
				break;

			case 'f':
				_filter_tests(ts, optarg);
				break;

			case 'v':
				_verbose = 1;
				break;

			case 'h':
			default:
				_print_usage(argv);
		}
	}

	// @todo single-process testing
	// @todo recording last-executed line of test
	// @todo global setup/teardown in parent process
	// @todo get_port()

	(void)argv;
}

static void _add_test(struct tests *ts, struct paratec *p)
{
	int64_t i;
	char index[34];
	int has_range = 0;

	struct test t = {
		.p = p,
		.flags = {
			.run = 1,
			.passed = 0,
		},
	};

	if (p->range_low == 0 && p->range_high == 0) {
		p->range_high = 1;
	} else {
		has_range = 1;
	}

	for (i = p->range_low; i < p->range_high; i++) {
		if (ts->c == ts->allocated) {
			ts->allocated = MAX(ts->allocated, 1) * 2;
			ts->all = realloc(ts->all, sizeof(*ts->all) * ts->allocated);
			if (ts->all == NULL) {
				fprintf(stderr, "failed to allocate array for tests\n");
				exit(1);
			}
		}

		if (has_range) {
			snprintf(index, sizeof(index), ":%ld", i);
		} else {
			index[0] = '\0';
		}

		t.i = i;
		snprintf(t.name, sizeof(t.name), "%s%s", p->name, index);

		ts->all[ts->c++] = t;
	}
}

static void _flush_pipe(int fd, struct buff *b)
{
	ssize_t err;
	ssize_t cap;

	do {
		if (b->alloc == b->len) {
			b->alloc = sizeof(*b->str) * (MAX(b->alloc, 16) * 2);
			b->str = realloc(b->str, b->alloc);
			if (b->str == NULL) {
				fprintf(stderr, "failed to allocate buffer for test output\n");
				exit(1);
			}
		}

		cap = (b->alloc - b->len) - 1;
		err = read(fd, b->str + b->len, cap);
		if (err < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("failed to read test output");
				exit(1);
			}
		} else {
			b->len += err;
			b->str[b->len] = '\0';
		}
	} while (err == cap);
}

static void _flush_pipes(struct tests *ts, struct job *jobs, uint32_t jobsc)
{
	uint32_t i;

	for (i = 0; i < jobsc; i++) {
		struct job *j = jobs + i;

		if (j->pid == -1) {
			continue;
		}

		_flush_pipe(j->stdout, &ts->all[j->i].stdout);
		_flush_pipe(j->stderr, &ts->all[j->i].stdout);
	}
}

static void _check_timeouts(struct job *jobs, uint32_t jobsc)
{
	int err;
	uint32_t i;
	int64_t now = _now();

	for (i = 0; i < jobsc; i++) {
		struct job *j = jobs + i;
		if (j->pid != -1 && j->end_at < now) {
			j->timed_out = 1;
			err = killpg(j->pid, SIGKILL);
			if (err < 0 && errno != ESRCH) {
				perror("failed to kill child test");
				exit(1);
			}
		}
	}
}

static void _pipe(int p[2])
{
	int err = pipe(p);
	if (err < 0) {
		perror("failed to create pipes");
		exit(1);
	}

	err = fcntl(p[0], F_SETFL, O_NONBLOCK);
	if (err < 0) {
		perror("failed to make test pipe nonblocking");
		exit(1);
	}
}

static void _dup2(int std, int fd)
{
	int err = dup2(fd, std);
	if (err < 0) {
		perror("failed to dup2");
		exit(1);
	}
}

static void _run_test(struct test *t, struct job *j)
{
	int err;
	pid_t pid;
	int pstdin[2];
	int pstdout[2];
	int pstderr[2];

	memset(j, 0, sizeof(*j));

	_pipe(pstdin);
	_pipe(pstdout);
	_pipe(pstderr);

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "failed to fork for test %s: %s\n",
			t->p->name,
			strerror(errno));
		exit(1);
	}

	if (pid == 0) {
		_dup2(STDIN_FILENO, pstdin[1]);
		_dup2(STDOUT_FILENO, pstdout[1]);
		_dup2(STDERR_FILENO, pstderr[1]);

		err = setpgid(0, 0);
		if (err < 0) {
			perror("could not setpgid");
			exit(1);
		}

		_tjob = j;
		strncat(_tjob->last_line, "test start", sizeof(_tjob->last_line));

		if (t->p->setup != NULL) {
			t->p->setup();
		}

		t->p->fn(t->i);

		if (t->p->teardown != NULL) {
			t->p->teardown();
		}

		exit(0);
	} else {
		close(pstdin[0]);

		j->stdout = pstdout[0];
		j->stderr = pstderr[0];
		j->pid = pid;
		j->end_at = _now() + ((t->p->timeout ?: _timeout) * 1000 * 1000);
	}
}

static void _run_next_test(struct tests *ts, struct job *j)
{
	while (ts->next_test < ts->c) {
		uint32_t i = ts->next_test++;
		struct test *t = ts->all + i;

		if (t->flags.run) {
			_run_test(t, j);
			j->i = i;
			break;
		}
	}
}

static void _run_tests(struct tests *ts)
{

	pid_t pid;
	uint32_t i;
	int status;
	struct job *j;
	struct job *jobsmm;
	struct job jobs[_max_jobs];
	uint32_t finished = 0;

	if (ts->c == 0) {
		return;
	}

	jobsmm = mmap(
		NULL, sizeof(jobs),
		PROT_READ | PROT_WRITE,
		MAP_ANON | MAP_SHARED, -1, 0);
	if (jobsmm == MAP_FAILED) {
		perror("could not map shared testing memory");
		exit(1);
	}

	for (i = 0; i < N_ELEMENTS(jobs); i++) {
		jobsmm[i].pid = -1;
	}

	for (i = 0; i < N_ELEMENTS(jobs) && i < ts->c; i++) {
		j = jobsmm + i;
		_run_next_test(ts, j);
	}

	while (finished < ts->enabled) {
		struct test *t = NULL;

		_signal_wait();

		pid = waitpid(-1, &status, WNOHANG);
		if (pid == -1) {
			perror("wait() failed");
			exit(1);
		}

		if (pid > 0) {
			for (i = 0; i < N_ELEMENTS(jobs); i++) {
				j = jobsmm + i;
				if (j->pid == pid) {
					t = ts->all + j->i;
					break;
				}
			}

			if (t == NULL) {
				fprintf(stderr,
					"child exited (pid=%d), but test case can't be found?\n",
					pid);
				exit(1);
			}

			if (WIFEXITED(status)) {
				t->exit_status = WEXITSTATUS(status);
				t->flags.passed = t->exit_status == t->p->exit_status;
			} else if (WIFSIGNALED(status)) {
				t->signal_num = WTERMSIG(status);
				t->flags.passed = t->signal_num == t->p->signal_num;
			} else {
				continue;
			}

			_flush_pipes(ts, jobsmm, N_ELEMENTS(jobs));

			close(j->stdout);
			close(j->stderr);
			j->pid = -1;
			j->stdout = -1;
			j->stderr = -1;

			strncpy(t->last_line, j->last_line, sizeof(t->last_line));

			if (t->flags.passed) {
				ts->passes++;
				printf(".");
			} else if (t->signal_num > 0) {
				ts->errors++;
				t->flags.timed_out = j->timed_out;
				printf("E");
			} else {
				ts->failures++;
				printf("F");
			}

			fflush(stdout);

			_run_next_test(ts, j);
			finished++;
		}

		_flush_pipes(ts, jobsmm, N_ELEMENTS(jobs));
		_check_timeouts(jobsmm, N_ELEMENTS(jobs));
	}

	printf("\n");
}

static void _dump_line(const char *line, const size_t len)
{
	fwrite(STDPREFIX, strlen(STDPREFIX), 1, stdout);
	fwrite(line, len, 1, stdout);
	fwrite("\n", 1, 1, stdout);
}

static void _clear_buff(int dump, const char *which, struct buff *b)
{
	if (dump) {
		printf(INDENT INDENT "%s\n", which);

		if (b->len == 0) {
			printf(INDENT INDENT " | (nothing)\n");
		} else {
			int first = 1;
			char *start = b->str;
			size_t len = b->len;

			while (1) {
				char *nl = memmem(start, len, "\n", 1);
				if (nl == NULL) {
					if (first) {
						_dump_line(start, len);
					}
					break;
				}

				first = 0;
				*nl = '\0';
				_dump_line(start, nl - start);
				len -= nl - start;
				start = nl + 1;
			}
		}
	}

	free(b->str);
	b->str = NULL;
}

int main(int argc, char **argv)
{
	uint32_t i;
	struct tests ts;

	memset(&ts, 0, sizeof(ts));

	_setup_signals();

	/**
	 * There's something weird going on with GCC/LD: when not using a pointer,
	 * anything larger than a few words causes the section to be misaligned, and
	 * you can't access anything.
	 */
	extern struct paratec *__start_paratec;
	extern struct paratec *__stop_paratec;
	struct paratec **sp = &__start_paratec;

	while (sp < &__stop_paratec) {
		struct paratec *p = *sp;
		_add_test(&ts, p);
		sp++;
	}

	_set_opts(&ts, argc, argv);

	for (i = 0; i < ts.c; i++) {
		struct test *t = ts.all + i;
		if (t->flags.run || t->flags.filtered_run) {
			t->flags.run = 1;
			ts.enabled++;
		}
	}

	_run_tests(&ts);

	printf("%d%%: %u tests, %u errors, %u failures, %u skipped\n",
		ts.enabled == 0 ?
			100 :
			(int)((((double)ts.passes) / ts.enabled) * 100),
		ts.c,
		ts.errors,
		ts.failures,
		ts.c - ts.enabled);

	for (i = 0; i < ts.c; i++) {
		int dump = 1;
		struct test *t = ts.all + i;

		if (!t->flags.run) {
			dump = 0;
			if (_verbose) {
				printf(INDENT " SKIP : %s \n",
					t->name);
			}
		} else if (t->flags.passed) {
			dump = _verbose;
			if (dump) {
				printf(INDENT " PASS : %s \n",
					t->name);
			}
		} else if (t->exit_status != 0) {
			printf(INDENT " FAIL : %s : exit code=%d\n",
				t->name,
				t->exit_status);
		} else if (t->flags.timed_out) {
			printf(INDENT "ERROR : %s : timed out after %s\n",
				t->name,
				t->last_line);
		} else {
			printf(INDENT "ERROR : %s : received signal(%d) `%s` after %s\n",
				t->name,
				t->signal_num,
				strsignal(t->signal_num),
				t->last_line);
		}

		_clear_buff(dump, "stdout", &t->stdout);
		_clear_buff(dump, "stderr", &t->stderr);
	}

	free(ts.all);
	ts.all = NULL;

	return ts.passes != ts.enabled;
}
