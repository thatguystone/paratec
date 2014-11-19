/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2014 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "paratec.h"

#define LINE_SIZE 2048

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

struct job {
	uint32_t i; // Index of the test in tests.all
	pid_t pid;
	int64_t end_at;
	int timed_out;
	char last_line[LINE_SIZE];
};

struct test {
	char name[LINE_SIZE];
	int passed;
	int timed_out;
	int exit_status;
	int signal_num;
	int64_t i;
	char last_line[LINE_SIZE];
	struct paratec *p;
};

struct tests {
	uint32_t passes;
	uint32_t errors;
	uint32_t failures;

	uint32_t c;
	uint32_t allocated;
	struct test *all;
};

static uint32_t _max_jobs;
static uint32_t _timeout;
static uint32_t _verbose;

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
		.tv_nsec = 100 * 1000,
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

static void _set_opts(int argc, char **argv)
{
	_max_jobs = _get_cpu_count() + 1;
	_timeout = 5;

	if (argc == 1) {
		return;
	}

	// @todo test filtering
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
		.passed = 0,
		.p = p,
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
		}

		if (has_range) {
			snprintf(index, sizeof(index), ":%ld", i);
		} else {
			index[0] = '\0';
		}

		t.i = i;
		snprintf(t.name, sizeof(t.name), "%s:%s%s", p->file, p->name, index);

		ts->all[ts->c++] = t;
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
			if (err < 0) {
				perror("failed to kill child test");
				exit(1);
			}
		}
	}
}

static void _run_test(struct test *t, struct job *j)
{
	int err;
	pid_t pid;

	memset(j, 0, sizeof(*j));

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "failed to fork for test %s: %s\n",
			t->p->name,
			strerror(errno));
		exit(1);
	}

	if (pid == 0) {
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
		j->end_at = _now() + ((t->p->timeout ?: _timeout) * 1000 * 1000);
		j->pid = pid;
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
	uint32_t curr_test = 0;

	if (ts->c == 0) {
		return;
	}

	jobsmm = mmap(
		jobs, sizeof(jobs),
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

		_run_test(ts->all + i, j);
		j->i = i;
		curr_test++;
	}

	while (finished < ts->c) {
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
					t = ts->all + jobsmm[i].i;
					j->pid = -1;
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
				t->passed = t->exit_status == t->p->exit_status;
			} else if (WIFSIGNALED(status)) {
				t->signal_num = WTERMSIG(status);
				t->passed = t->signal_num == t->p->signal_num;
			} else {
				continue;
			}

			strncpy(t->last_line, j->last_line, sizeof(t->last_line));

			if (t->passed) {
				ts->passes++;
				printf(".");
			} else if (t->signal_num > 0) {
				ts->errors++;
				t->timed_out = j->timed_out;
				printf("E");
			} else {
				ts->failures++;
				printf("F");
			}

			fflush(stdout);

			if (curr_test < ts->c) {
				_run_test(ts->all + curr_test, j);
				j->i = curr_test;
				curr_test++;
			}

			finished++;
		}

		_check_timeouts(jobsmm, N_ELEMENTS(jobs));
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	uint32_t i;
	struct tests ts;

	memset(&ts, 0, sizeof(ts));

	_setup_signals();
	_set_opts(argc, argv);

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

	_run_tests(&ts);

	printf("%d%%: %u tests, %u errors, %u failures\n",
		(int)((((double)ts.passes) / ts.c) * 100),
		ts.c,
		ts.errors,
		ts.failures);

	for (i = 0; i < ts.c; i++) {
		struct test *t = ts.all + i;

		if (t->passed) {
			if (_verbose) {
				printf("\t PASS : %s \n",
					t->name);
			}
		} else if (t->exit_status != 0) {
			printf("\t FAIL : %s : exit code=%d\n",
				t->name,
				t->exit_status);
		} else if (t->timed_out) {
			printf("\tERROR : %s : timed out after %s\n",
				t->name,
				t->last_line);
		} else {
			printf("\tERROR : %s : received signal(%d) `%s` after %s\n",
				t->name,
				t->signal_num,
				strsignal(t->signal_num),
				t->last_line);
		}
	}

	free(ts.all);

	return ts.passes != ts.c;
}
