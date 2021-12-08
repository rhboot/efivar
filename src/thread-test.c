// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * thread-test.c - test (some) efivar thread safety
 * Copyright Jonathan Marler
 */

#include "fix_coverity.h"

#include <alloca.h>
#include <efivar.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#define LOOP_COUNT 100

static int verbosity = 0;

// returns: the number of threads created
static size_t
multi_pthread_create(pthread_t * threads, unsigned count,
		     void *(*start_routine)(void *))
{
	size_t i = 0;
	int result;

	for (; i < count; i++) {
		result = pthread_create(&threads[i], NULL, start_routine, NULL);
		if (result != 0) {
			err(1, "pthread_create failed");
			break;
		}
	}

	return i;
}

#define TEST_SUCCESS NULL
#define TEST_FAIL ((void*)1)
static const char *test_result_str(void *result)
{
	if (result == TEST_SUCCESS)
		return "SUCCESS";
	if (result == PTHREAD_CANCELED)
		return "CANCELED";
	return "FAIL";
}

// returns: 0 on success
static int
multi_pthread_join(pthread_t * threads, unsigned count, void **worst_result)
{
	for (unsigned i = 0; i < count; i++) {
		void *result;
		int join_result;

		join_result = pthread_join(threads[i], &result);
		if (join_result != 0) {
			warnx("pthread_join failed");
			return 1;
		}
		if (verbosity >= 1)
			printf("[MAIN-THREAD] child %d exited with %s\n", i,
			       test_result_str(result));
		if (result != NULL) {
			*worst_result = result;
		}
	}
	return 0;
}

static void *loop_get_variable_size_test(void *_ __attribute__((__unused__)))
{
	if (verbosity >= 2)
		printf("[DEBUG] test running on new thread!\n");
	for (unsigned i = 0; i < LOOP_COUNT; i++) {
		size_t size;
		efi_guid_t guid = { 0 };
		int result;

		result = efi_get_variable_size(guid, "foo2", &size);
		if (result == 0 || errno != ENOENT) {
			err(1, "fail, iteration=%u, result=%d expected ENOENT, got",
			    i, result);
			return TEST_FAIL;
		} else if (verbosity >= 2) {
			printf("[DEBUG] iteration=%u, result=%d errno=%d\n", i, result, errno);
		}
	}
	return TEST_SUCCESS;
}

static int multithreaded_test(size_t count, void *(*test_func)(void *))
{
	pthread_t *threads = alloca(sizeof(pthread_t) * count);
	if (count != multi_pthread_create(threads, count, test_func)) {
		// error already logged
		return 1;
	}
	void *worst_result = TEST_SUCCESS;
	if (multi_pthread_join(threads, count, &worst_result)) {
		// error already logged
		return 1;
	}
	if (verbosity >= 1)
		printf("worst result %s\n", test_result_str(worst_result));
	return (worst_result == TEST_SUCCESS) ? 0 : -1;
}

static void __attribute__((__noreturn__))
usage(int ret)
{
	FILE *out = ret == 0 ? stdout : stderr;
	fprintf(out,
		"Usage: %s [OPTION...]\n"
		"  -v, --verbose                     be more verbose\n"
		"  -t, --thread-count N              use N threads\n"
		"Help options:\n"
		"  -?, --help                        Show this help message\n"
		"      --usage                       Display brief usage message\n",
		program_invocation_short_name);
	exit(ret);
}

int main(int argc, char *argv[])
{
	unsigned long thread_count = 64;
	char *sopts = "vt:?";
	struct option lopts[] = {
		{"help", no_argument, 0, '?'},
		{"quiet", no_argument, 0, 'q'},
		{"thread-count", no_argument, 0, 't'},
		{"usage", no_argument, 0, 0},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0},
	};
	int c;
	int i;
	int rc;

	while ((c = getopt_long(argc, argv, sopts, lopts, &i)) != -1) {
		switch (c) {
		case 'q':
			verbosity -= 1;
			break;
		case 't':
			thread_count = strtoul(optarg, NULL, 0);
			if (errno == ERANGE || errno == EINVAL)
				err(1, "invalid argument for -t: %s", optarg);
			break;
		case 'v':
			verbosity += 1;
			break;
		case '?':
			usage(EXIT_SUCCESS);
			break;
		case 0:
			if (strcmp(lopts[i].name, "usage"))
				usage(EXIT_SUCCESS);
			break;
		}
	}

	if (verbosity >= 1)
		printf("thread count %lu\n", thread_count);
	rc = multithreaded_test(thread_count, loop_get_variable_size_test);
	if (verbosity >= 0)
		printf("thread test %s\n", rc == 0 ? "passed" : "failed");
	return rc;
}

// vim:fenc=utf-8:tw=75:noet
