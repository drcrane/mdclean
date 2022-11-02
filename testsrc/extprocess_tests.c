#include "minunit.h"
#include "extprocess.h"

const char * run_simple_external_process_test() {
	struct extprocess_context * extprocess_ctx;
	struct dynstring_context process_output = DYNSTRING_DEFAULT_INIT;

	extprocess_ctx = extprocess_create();
	mu_assert(extprocess_ctx, "context creation failed");
	extprocess_argumentappend(extprocess_ctx, "appbin/jhead");
	extprocess_execute_getoutput(extprocess_ctx->arg, &process_output);
	dynstring_free(&process_output);
	extprocess_dispose(extprocess_ctx);
	return NULL;
}

const char * all_tests() {
	mu_suite_start();
	mu_run_test(run_simple_external_process_test);
	return NULL;
}

RUN_TESTS(all_tests)

