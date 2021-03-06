#include <cgreen/reporter.h>
#include <cgreen/messaging.h>
#include <cgreen/breadcrumb.h>
#include <stdlib.h>
#include <stdio.h>
#if !defined WIN32 && !defined WINCE
#include <sys/msg.h>
#endif
#include <stdarg.h>

enum {pass = 1, fail, completion};

struct TestContext_ {
	TestReporter *reporter;
};
static TestContext context;

static void show_pass(TestReporter *reporter, const char *file, int line, const char *message, va_list arguments);
static void show_fail(TestReporter *reporter, const char *file, int line, const char *message, va_list arguments);
static void show_incomplete(TestReporter *reporter, const char *name);
static void assert_true(TestReporter *reporter, const char *file, int line, int result, const char *message, ...);
static void read_reporter_results(TestReporter *reporter);

TestReporter *get_test_reporter() {
	return context.reporter;
}

TestReporter *create_reporter() {
    TestReporter *reporter = (TestReporter *)malloc(sizeof(TestReporter));
    reporter->destroy = &destroy_reporter;
	reporter->start = &reporter_start;
	reporter->finish = &reporter_finish;
	reporter->show_pass = &show_pass;
	reporter->show_fail = &show_fail;
	reporter->show_incomplete = &show_incomplete;
	reporter->assert_true = &assert_true;
	reporter->passes = 0;
	reporter->failures = 0;
	reporter->exceptions = 0;
	reporter->breadcrumb = (void *)create_breadcrumb();
	reporter->ipc = start_cgreen_messaging(45);
	context.reporter = reporter;
    return reporter;
}

void destroy_reporter(TestReporter *reporter) {
	destroy_breadcrumb((CgreenBreadcrumb *)reporter->breadcrumb);
    free(reporter);
    context.reporter = NULL;
}

void reporter_start(TestReporter *reporter, const char *name)  {
	push_breadcrumb((CgreenBreadcrumb *)reporter->breadcrumb, name);
}

void reporter_finish(TestReporter *reporter, const char *name) {
    read_reporter_results(reporter);
    pop_breadcrumb((CgreenBreadcrumb *)reporter->breadcrumb);
}

void add_reporter_result(TestReporter *reporter, int result) {
    send_cgreen_message(reporter->ipc, result ? pass : fail);
}

void send_reporter_completion_notification(TestReporter *reporter) {
    send_cgreen_message(reporter->ipc, completion);
}

static void show_pass(TestReporter *reporter, const char *file, int line, const char *message, va_list arguments) {
}

static void show_fail(TestReporter *reporter, const char *file, int line, const char *message, va_list arguments) {
}

static void show_incomplete(TestReporter *reporter, const char *name) {
}

static void assert_true(TestReporter *reporter, const char *file, int line, int result, const char *message, ...) {
    va_list arguments;
    va_start(arguments, message);
	if (result) {
    	(*reporter->show_pass)(reporter, file, line, message, arguments);
	} else {
    	(*reporter->show_fail)(reporter, file, line, message, arguments);
	}
	add_reporter_result(reporter, result);
	va_end(arguments);
}

static void read_reporter_results(TestReporter *reporter) {
    int completed = 0;
    int result;
    while ((result = receive_cgreen_message(reporter->ipc)) > 0) {
        if (result == pass) {
            reporter->passes++;
        } else if (result == fail) {
            reporter->failures++;
        } else if (result == completion) {
            completed = 1;
        }
    }
    if (! completed) {
        (*reporter->show_incomplete)(reporter, get_current_from_breadcrumb((CgreenBreadcrumb *)reporter->breadcrumb));
        reporter->exceptions++;
    }
}
