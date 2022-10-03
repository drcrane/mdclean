#include "minunit.h"

const char * formdecoder_read_test() {
	return NULL;
}

const char * all_tests() {
	mu_suite_start();
	mu_run_test(formdecoder_read_test);
	return NULL;
}

RUN_TESTS(all_tests)
