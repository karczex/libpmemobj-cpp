// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

#ifndef LIBPMEMOBJ_CPP_UNITTEST_HPP
#define LIBPMEMOBJ_CPP_UNITTEST_HPP

#include "../test_backtrace.h"
#include "../valgrind_internal.hpp"
#include "iterators_support.hpp"
#include "thread_helpers.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>

#ifndef _WIN32
#define os_stat_t struct stat
#define os_stat stat
#else
#define os_stat_t struct _stat64
#define os_stat _stat64
#endif

static inline void
UT_OUT(const char *format, ...)
{
	va_list args_list;
	va_start(args_list, format);
	std::vfprintf(stdout, format, args_list);
	va_end(args_list);

	fprintf(stdout, "\n");
}

static inline void
UT_EXCEPTION(std::exception &e)
{
	std::cerr << e.what() << std::endl;
}

static inline void
UT_FATAL(const char *format, ...)
{
	va_list args_list;
	va_start(args_list, format);
	std::vfprintf(stderr, format, args_list);
	va_end(args_list);

	fprintf(stderr, "\n");

	abort();
}

#ifdef _WIN32
#include "unittest_windows.hpp"
#endif

/*
 * ut_stat -- stat that never returns -1
 */
int
ut_stat(const char *file, int line, const char *func, const char *path,
	os_stat_t *st)
{
	int ret = os_stat(path, st);

	if (ret < 0)
		UT_FATAL("%s:%d %s - !stat: %s", file, line, func, path);

#ifdef _WIN32
	/* clear unused bits to avoid confusion */
	st->st_mode &= 0600;
#endif

	return ret;
}

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __func__
#endif
#define PRINT_TEST_PARAMS                                                      \
	do {                                                                   \
		std::cout << "TEST: " << __PRETTY_FUNCTION__ << std::endl;     \
	} while (0)

#define STAT(path, st) ut_stat(__FILE__, __LINE__, __func__, path, st)

/* assert a condition is true at runtime */
#define UT_ASSERT(cnd)                                                         \
	((void)((cnd) ||                                                       \
		(UT_FATAL("%s:%d %s - assertion failure: %s", __FILE__,        \
			  __LINE__, __func__, #cnd),                           \
		 0)))

/* assertion with exception related string printed */
#define UT_FATALexc(exception)                                                 \
	((void)(UT_EXCEPTION(exception),                                       \
		(UT_FATAL("%s:%d %s - assertion failure", __FILE__, __LINE__,  \
			  __func__),                                           \
		 0)))

/* assertion with extra info printed if assertion fails at runtime */
#define UT_ASSERTinfo(cnd, info)                                               \
	((void)((cnd) ||                                                       \
		(UT_FATAL("%s:%d %s - assertion failure: %s (%s = %s)",        \
			  __FILE__, __LINE__, __func__, #cnd, #info, info),    \
		 0)))

/* assert two integer values are equal at runtime */
#define UT_ASSERTeq(lhs, rhs)                                                  \
	((void)(((lhs) == (rhs)) ||                                            \
		(UT_FATAL("%s:%d %s - assertion failure: %s (0x%llx) == %s "   \
			  "(0x%llx)",                                          \
			  __FILE__, __LINE__, __func__, #lhs,                  \
			  (unsigned long long)(lhs), #rhs,                     \
			  (unsigned long long)(rhs)),                          \
		 0)))

/* assert two integer values are not equal at runtime */
#define UT_ASSERTne(lhs, rhs)                                                  \
	((void)(((lhs) != (rhs)) ||                                            \
		(UT_FATAL("%s:%d %s - assertion failure: %s (0x%llx) != %s "   \
			  "(0x%llx)",                                          \
			  __FILE__, __LINE__, __func__, #lhs,                  \
			  (unsigned long long)(lhs), #rhs,                     \
			  (unsigned long long)(rhs)),                          \
		 0)))

#define UT_ASSERT_NOEXCEPT(...)                                                \
	static_assert(noexcept(__VA_ARGS__), "Operation must be noexcept")

#define STR(x) #x

#define ASSERT_ALIGNED_BEGIN(type, ref)                                        \
	do {                                                                   \
		size_t off = 0;                                                \
		const char *last = "(none)";

#define ASSERT_ALIGNED_FIELD(type, ref, field)                                               \
	do {                                                                                 \
		if (offsetof(type, field) != off)                                            \
			UT_FATAL(                                                            \
				"%s: padding, missing field or fields not in order between " \
				"'%s' and '%s' -- offset %lu, real offset %lu",              \
				STR(type), last, STR(field), off,                            \
				offsetof(type, field));                                      \
		off += sizeof((ref).field);                                                  \
		last = STR(field);                                                           \
	} while (0)

#define ASSERT_FIELD_SIZE(field, ref, size)                                    \
	do {                                                                   \
		UT_COMPILE_ERROR_ON(size != sizeof((ref).field));              \
	} while (0)

#define ASSERT_ALIGNED_CHECK(type)                                             \
	if (off != sizeof(type))                                               \
		UT_FATAL("%s: missing field or padding after '%s': "           \
			 "sizeof(%s) = %lu, fields size = %lu",                \
			 STR(type), last, STR(type), sizeof(type), off);       \
	}                                                                      \
	while (0)

#define ASSERT_OFFSET_CHECKPOINT(type, checkpoint)                             \
	do {                                                                   \
		if (off != checkpoint)                                         \
			UT_FATAL("%s: violated offset checkpoint -- "          \
				 "checkpoint %lu, real offset %lu",            \
				 STR(type), checkpoint, off);                  \
	} while (0)

#define ASSERT_REACHED                                                         \
	do {                                                                   \
		UT_FATAL("%s:%d in function %s should never be reached",       \
			 __FILE__, __LINE__, __func__);                        \
	} while (0)

static inline int
run_test(std::function<void()> test)
{
	test_register_sighandlers();
	set_valgrind_internals();

	try {
		test();
	} catch (std::exception &e) {
		UT_FATALexc(e);
	} catch (...) {
		UT_FATAL("catch(...){}");
	}

	return 0;
}

#endif /* LIBPMEMOBJ_CPP_UNITTEST_HPP */
