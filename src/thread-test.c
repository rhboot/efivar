// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * thread-test.c - test (some) efivar thread safety
 * Copyright Jonathan Marler
 */

#include <alloca.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

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
			perror("pthread_create");
			break;
		}
	}

	return i;
}

#define TEST_SUCCESS NULL
#define TEST_FAIL ((void*)1)
static const char *test_result_str(void *result)
{
	return (result == TEST_SUCCESS) ? "SUCCESS" : "FAIL";
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
			perror("pthread_join");
			return 1;
		}
		printf("[MAIN-THREAD] child %d exited with %s\n", i,
		       test_result_str(result));
		if (result != NULL) {
			*worst_result = result;
		}
	}
	return 0;
}

#include <efivar/efivar.h>
#define LOOP_COUNT 100

static void *loop_get_variable_size_test(void *_ __attribute__((__unused__)))
{
	//printf("[DEBUG] test running on new thread!\n");
	for (unsigned i = 0; i < LOOP_COUNT; i++) {
		size_t size;
		efi_guid_t guid = { 0 };
		int result;

		result = efi_get_variable_size(guid, "foo2", &size);
		if (result == 0 || errno != ENOENT) {
			printf("fail, iteration=%u, result=%d errno=%d, expected error\n",
			       i, result, errno);
			return TEST_FAIL;
		} else {
			//printf("[DEBUG] iteration=%u, result=%d errno=%d\n", i, result, errno);
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
	printf("worst result %s\n", test_result_str(worst_result));
	return (worst_result == TEST_SUCCESS) ? 0 : -1;
}

int main(int argc, char *argv[])
{
	int thread_count;

	if (argc != 2) {
		printf("Usage: %s THREAD_COUNT\n", argv[0]);
		return 1;
	}

	thread_count = atoi(argv[1]);
	printf("thread count %d\n", thread_count);
	return multithreaded_test(thread_count, loop_get_variable_size_test);
}

// vim:fenc=utf-8:tw=75:noet
