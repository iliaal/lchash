/* Emulates the FrankenPHP worker-mode request loop: one PHP request per
 * thread stays open while sapi_deactivate()/sapi_activate() run between
 * HTTP requests, the way frankenphp_worker_request_shutdown() and
 * frankenphp_worker_request_startup() do. Each HTTP request creates the
 * procedural table without destroying it and must get a fresh one.
 *
 * FrankenPHP releases the handler's return value after sapi_deactivate(),
 * and the worker loop body runs before the next sapi_activate(), so PHP code
 * can recreate the table between requests. Each HTTP request leaves an
 * object whose destructor does that, released after sapi_deactivate(). */

#include <sapi/embed/php_embed.h>
#include <main/php_output.h>
#include <main/SAPI.h>
#include <zend_exceptions.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZTS
# error "worker-requests needs a ZTS libphp"
#endif

#define THREADS 8
#define WORKER_RUNS 20
#define HTTP_REQUESTS 10

static int failures[THREADS];
static int handled[THREADS];

static int eval_true(const char *code)
{
	zval rv;
	volatile int ok = 0;

	zend_try {
		if (zend_eval_string(code, &rv, "worker-request") == SUCCESS) {
			ok = Z_TYPE(rv) == IS_TRUE;
			zval_ptr_dtor(&rv);
		}
		if (EG(exception)) {
			zend_clear_exception();
			ok = 0;
		}
	} zend_end_try();
	return ok;
}

static int handle_http_request(int tid, int run, int req)
{
	char code[1024];

	snprintf(code, sizeof(code),
		"(function () {"
		" $c = @lchash_create(16);"
		" lchash_insert('u', 't%dr%dq%d');"
		" $GLOBALS['late'] = new class {"
		"  public function __destruct() {"
		"   if (@lchash_create(4)) { lchash_insert('late', 'stale'); }"
		"  }"
		" };"
		" return $c === true"
		"  && lchash_find('u') === 't%dr%dq%d'"
		"  && lchash_find('late') === false;"
		" })()",
		tid, run, req, tid, run, req);
	return eval_true(code);
}

/* The released object's destructor can only create the table, and store
 * the late marker, if sapi_deactivate() freed the previous request's. */
static int between_http_requests(void)
{
	int ok;

	zend_try { php_output_end_all(); } zend_end_try();
	zend_try { php_output_deactivate(); } zend_end_try();
	zend_try { sapi_deactivate(); } zend_end_try();
	ok = eval_true("(function () {"
		" unset($GLOBALS['late']);"
		" return @lchash_find('late') === 'stale';"
		" })()");
	php_output_activate();
	sapi_activate();
	return ok;
}

static void *worker_thread(void *arg)
{
	int tid = (int) (intptr_t) arg;

	(void) ts_resource(0);
	for (int run = 0; run < WORKER_RUNS; run++) {
		if (php_request_startup() == FAILURE) {
			failures[tid] += HTTP_REQUESTS;
			continue;
		}
		for (int req = 0; req < HTTP_REQUESTS; req++) {
			if (!handle_http_request(tid, run, req)) {
				failures[tid]++;
			}
			handled[tid]++;
			if (req + 1 < HTTP_REQUESTS && !between_http_requests()) {
				failures[tid]++;
			}
		}
		php_request_shutdown(NULL);
	}
	ts_free_thread();
	return NULL;
}

int main(int argc, char **argv)
{
	const int expected = THREADS * WORKER_RUNS * HTTP_REQUESTS;
	volatile int total = 0, completed = 0, loaded = 0, thread_errors = 0;

	PHP_EMBED_START_BLOCK(argc, argv)
		loaded = zend_hash_str_exists(&module_registry, "lchash", sizeof("lchash") - 1);
		if (loaded) {
			pthread_t th[THREADS];
			int started[THREADS] = {0};
			for (int i = 0; i < THREADS; i++) {
				int rc = pthread_create(&th[i], NULL, worker_thread, (void *) (intptr_t) i);
				if (rc != 0) {
					fprintf(stderr, "pthread_create: %s\n", strerror(rc));
					thread_errors++;
				} else {
					started[i] = 1;
				}
			}
			for (int i = 0; i < THREADS; i++) {
				if (!started[i]) {
					continue;
				}
				int rc = pthread_join(th[i], NULL);
				if (rc != 0) {
					fprintf(stderr, "pthread_join: %s\n", strerror(rc));
					thread_errors++;
				}
			}
			for (int i = 0; i < THREADS; i++) {
				total += failures[i];
				completed += handled[i];
			}
		}
	PHP_EMBED_END_BLOCK()

	if (!loaded) {
		fprintf(stderr, "lchash is not loaded\n");
		return 2;
	}
	printf("http_requests=%d/%d failures=%d thread_errors=%d\n",
		completed, expected, total, thread_errors);
	return total == 0 && completed == expected && thread_errors == 0 ? 0 : 1;
}
