// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "unittest.hpp"
#include <cstring>
#include <iostream>

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/string_view.hpp>
#include <libpmemobj++/transaction.hpp>

#include <libpmemobj++/container/mpsc_queue.hpp>
#include <string>

#define LAYOUT "layout"

struct root {
	pmem::obj::persistent_ptr<char[]> log;
};

int
basic_test(int argc, char *argv[])
{

	if (argc != 3)
		UT_FATAL("usage: %s file-name create", argv[0]);

	const char *path = argv[1];
	bool create = std::string(argv[2]) == "1";

	pmem::obj::pool<struct root> pop;

	if (create) {
		pop = pmem::obj::pool<root>::create(std::string(path), LAYOUT,
						    PMEMOBJ_MIN_POOL,
						    S_IWUSR | S_IRUSR);

		pmem::obj::transaction::run(pop, [&] {
			pop.root()->log =
				pmem::obj::make_persistent<char[]>(10000);
		});
	} else {
		pop = pmem::obj::pool<root>::open(std::string(path), LAYOUT);
	}

	auto proot = pop.root();

	auto queue = pmem::obj::experimental::mpsc_queue(proot->log, 10000, 1);
	auto worker = queue.register_worker();

	std::vector<std::string> values = {"xxx", "aaaaaaa", "bbbbb"};

	if (create) {
		for (auto &e : values) {
			auto acc = worker.produce(e.size());
			acc.add(e.data(), e.size());
		}
		{
			auto rd_acc = queue.consume();
			std::vector<std::string> values_on_pmem;
			for (auto str : rd_acc) {
				values_on_pmem.emplace_back(str.data(),
							    str.size());
			}

			UT_ASSERT(values_on_pmem == values);
		}

		{
			auto acc = worker.produce(5);
			const char *tmp = "old";
			acc.add(tmp, 3);
		}
	} else {
		std::vector<std::string> values_on_pmem;
		queue.recover(
			[&](pmem::obj::experimental::mpsc_queue::entry &entry) {
				values_on_pmem.emplace_back(entry.data,
							    entry.size);
			});

		UT_ASSERTeq(values_on_pmem.size(), 1);
		UT_ASSERT(values_on_pmem[0] == "old");
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	return run_test([&] { basic_test(argc, argv); });
}