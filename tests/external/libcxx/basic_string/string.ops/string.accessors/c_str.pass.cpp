//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Copyright 2019-2020, Intel Corporation
//
// Modified to test pmem::obj containers
//

#include "unittest.hpp"

#include <libpmemobj++/container/string.hpp>

namespace nvobj = pmem::obj;

using string_type = pmem::obj::string;

struct root {
	nvobj::persistent_ptr<string_type> s1, s2, s3, s4;
};

template <class S>
void
test(const S &s)
{
	typedef typename S::traits_type T;
	const typename S::value_type *str = s.c_str();
	if (s.size() > 0) {
		UT_ASSERT(T::compare(str, &s[0], s.size()) == 0);
		UT_ASSERT(T::eq(str[s.size()], typename S::value_type()));
	} else
		UT_ASSERT(T::eq(str[0], typename S::value_type()));
}

static void
test(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	auto path = argv[1];
	auto pop = nvobj::pool<root>::create(
		path, "string_test", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);

	auto r = pop.root();

	try {
		nvobj::transaction::run(pop, [&] {
			r->s1 = nvobj::make_persistent<string_type>("");
			r->s2 = nvobj::make_persistent<string_type>("abcde");
			r->s3 = nvobj::make_persistent<string_type>(
				"abcdefghij");
			r->s4 = nvobj::make_persistent<string_type>(
				"abcdefghijklmnopqrst");
		});

		test(*r->s1);
		test(*r->s2);
		test(*r->s3);
		test(*r->s4);

		nvobj::transaction::run(pop, [&] {
			nvobj::delete_persistent<string_type>(r->s1);
			nvobj::delete_persistent<string_type>(r->s2);
			nvobj::delete_persistent<string_type>(r->s3);
			nvobj::delete_persistent<string_type>(r->s4);
		});
	} catch (std::exception &e) {
		UT_FATALexc(e);
	}

	pop.close();
}

int
main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
