/* Emulates the FrankenPHP worker-mode request loop: one PHP request per
 * thread stays open while sapi_deactivate()/sapi_activate() run between
 * HTTP requests, the way frankenphp_worker_request_shutdown() and
 * frankenphp_worker_request_startup() do. Each HTTP request creates the
 * procedural table without destroying it and must get a fresh one. */

#include <sapi/embed/php_embed.h>
#include <main/php_output.h>
#include <main/SAPI.h>
#include <zend_exceptions.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ZTS
# error "worker-requests needs a ZTS libphp"
#endif

#define THREADS 8
#define WORKER_RUNS 20
#define HTTP_REQUESTS 10

static int failures[THREADS];

static int handle_http_request(int tid, int run, int req)
{
	char code[512];
	zval rv;
	volatile int ok = 0;

	snprintf(code, sizeof(code),
		"(function () {"
		" $c = @lchash_create(16);"
		" lchash_insert('u', 't%dr%dq%d');"
		" return $c === true && lchash_find('u') === 't%dr%dq%d';"
		" })()",
		tid, run, req, tid, run, req);

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

static void between_http_requests(void)
{
	zend_try { php_output_end_all(); } zend_end_try();
	zend_try { php_output_deactivate(); } zend_end_try();
	zend_try { sapi_deactivate(); } zend_end_try();
	php_output_activate();
	sapi_activate();
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
			if (req + 1 < HTTP_REQUESTS) {
				between_http_requests();
			}
		}
		php_request_shutdown(NULL);
	}
	ts_free_thread();
	return NULL;
}

int main(int argc, char **argv)
{
	volatile int total = 0, loaded = 0;

	PHP_EMBED_START_BLOCK(argc, argv)
		loaded = zend_hash_str_exists(&module_registry, "lchash", sizeof("lchash") - 1);
		if (loaded) {
			pthread_t th[THREADS];
			for (int i = 0; i < THREADS; i++) {
				pthread_create(&th[i], NULL, worker_thread, (void *) (intptr_t) i);
			}
			for (int i = 0; i < THREADS; i++) {
				pthread_join(th[i], NULL);
			}
			for (int i = 0; i < THREADS; i++) {
				total += failures[i];
			}
		}
	PHP_EMBED_END_BLOCK()

	if (!loaded) {
		fprintf(stderr, "lchash is not loaded\n");
		return 2;
	}
	printf("http_requests=%d failures=%d\n", THREADS * WORKER_RUNS * HTTP_REQUESTS, total);
	return total == 0 ? 0 : 1;
}
