/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @addtogroup t_mutex_api
 * @{
 * @defgroup t_mutex_lock test_mutex_lock
 * @brief TestPurpose: verify zephyr mutex lock/unlock apis under different
 *                     condition
 * - API coverage
 *   -# k_mutex_init K_MUTEX_DEFINE
 *   -# k_mutex_lock [FOREVER NO_WAIT TIMEOUT]
 *   -# k_mutex_unlock
 * @}
 */

#include <ztest.h>
#include <irq_offload.h>

#define TIMEOUT 500
#define STACK_SIZE 512

/**TESTPOINT: init via K_MUTEX_DEFINE*/
K_MUTEX_DEFINE(kmutex);
static struct k_mutex mutex;

static char __noinit __stack tstack[STACK_SIZE];

static void tThread_entry_lock_forever(void *p1, void *p2, void *p3)
{
	assert_false(k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0,
		     "access locked resource from spawn thread");
	/* should not hit here */
}

static void tThread_entry_lock_no_wait(void *p1, void *p2, void *p3)
{
	assert_true(k_mutex_lock((struct k_mutex *)p1, K_NO_WAIT) != 0, NULL);
	TC_PRINT("bypass locked resource from spawn thread\n");
}

static void tThread_entry_lock_timeout_fail(void *p1, void *p2, void *p3)
{
	assert_true(k_mutex_lock((struct k_mutex *)p1, TIMEOUT - 100) != 0, NULL);
	TC_PRINT("bypass locked resource from spawn thread\n");
}

static void tThread_entry_lock_timeout_pass(void *p1, void *p2, void *p3)
{
	assert_true(k_mutex_lock((struct k_mutex *)p1, TIMEOUT + 100) == 0, NULL);
	TC_PRINT("access resource from spawn thread\n");
	k_mutex_unlock((struct k_mutex *)p1);
}

static void tmutex_test_lock(struct k_mutex *pmutex,
			     void (*entry_fn)(void *, void *, void *))
{
	k_mutex_init(pmutex);
	k_tid_t tid = k_thread_spawn(tstack, STACK_SIZE,
				     entry_fn, pmutex, NULL, NULL,
				     K_PRIO_PREEMPT(0), 0, 0);
	k_mutex_lock(pmutex, K_FOREVER);
	TC_PRINT("access resource from main thread\n");

	/* wait for spawn thread to take action */
	k_sleep(TIMEOUT);

	/* teardown */
	k_thread_abort(tid);
}

static void tmutex_test_lock_timeout(struct k_mutex *pmutex,
				     void (*entry_fn)(void *, void *, void *))
{
	/**TESTPOINT: test k_mutex_init mutex*/
	k_mutex_init(pmutex);
	k_tid_t tid = k_thread_spawn(tstack, STACK_SIZE,
				     entry_fn, pmutex, NULL, NULL,
				     K_PRIO_PREEMPT(0), 0, 0);
	k_mutex_lock(pmutex, K_FOREVER);
	TC_PRINT("access resource from main thread\n");

	/* wait for spawn thread to take action */
	k_sleep(TIMEOUT);
	k_mutex_unlock(pmutex);
	k_sleep(TIMEOUT);

	/* teardown */
	k_thread_abort(tid);
}

static void tmutex_test_lock_unlock(struct k_mutex *pmutex)
{
	k_mutex_init(pmutex);
	assert_true(k_mutex_lock(pmutex, K_FOREVER) == 0,
		    "fail to lock K_FOREVER");
	k_mutex_unlock(pmutex);
	assert_true(k_mutex_lock(pmutex, K_NO_WAIT) == 0,
		    "fail to lock K_NO_WAIT");
	k_mutex_unlock(pmutex);
	assert_true(k_mutex_lock(pmutex, TIMEOUT) == 0,
		    "fail to lock TIMEOUT");
	k_mutex_unlock(pmutex);
}

/*test cases*/
void test_mutex_reent_lock_forever(void)
{
	/**TESTPOINT: test k_mutex_init mutex*/
	k_mutex_init(&mutex);
	tmutex_test_lock(&mutex, tThread_entry_lock_forever);

	/**TESTPOINT: test K_MUTEX_DEFINE mutex*/
	tmutex_test_lock(&kmutex, tThread_entry_lock_forever);
}

void test_mutex_reent_lock_no_wait(void)
{
	/**TESTPOINT: test k_mutex_init mutex*/
	tmutex_test_lock(&mutex, tThread_entry_lock_no_wait);

	/**TESTPOINT: test K_MUTEX_DEFINE mutex*/
	tmutex_test_lock(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_reent_lock_timeout_fail(void)
{
	/**TESTPOINT: test k_mutex_init mutex*/
	tmutex_test_lock_timeout(&mutex, tThread_entry_lock_timeout_fail);

	/**TESTPOINT: test K_MUTEX_DEFINE mutex*/
	tmutex_test_lock_timeout(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_reent_lock_timeout_pass(void)
{
	/**TESTPOINT: test k_mutex_init mutex*/
	tmutex_test_lock_timeout(&mutex, tThread_entry_lock_timeout_pass);

	/**TESTPOINT: test K_MUTEX_DEFINE mutex*/
	tmutex_test_lock_timeout(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_lock_unlock(void)
{
	/**TESTPOINT: test k_mutex_init mutex*/
	tmutex_test_lock_unlock(&mutex);

	/**TESTPOINT: test K_MUTEX_DEFINE mutex*/
	tmutex_test_lock_unlock(&kmutex);
}
