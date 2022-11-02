#include "minunit.h"
#include "extprocess.h"

const char * run_simple_external_process_test() {
	struct extprocess_context * extprocess_ctx;
	int rc;

	extprocess_ctx = extprocess_create();
	mu_assert(extprocess_ctx, "context creation failed");
	extprocess_argumentappend(extprocess_ctx, "testsrc/extprocess_tests.sh");
	rc = extprocess_execute(extprocess_ctx);
	mu_assert(rc == 0, "execute should be successful");
	rc = extprocess_read(extprocess_ctx, 120);
	mu_assert(rc == 0, "should be successful after waiting max of 120 seconds");
	extprocess_dispose(extprocess_ctx);
	return NULL;
}

const char * all_tests() {
	mu_suite_start();
	mu_run_test(run_simple_external_process_test);
	return NULL;
}

RUN_TESTS(all_tests)

