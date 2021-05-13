// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "unittest.hpp"
#include <cstring>
#include <iostream>

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>

#include <algorithm>
#include <libpmemobj++/container/mpsc_queue.hpp>
#include <string>

#define LAYOUT "multithreaded_mpsc_queue_test"

struct root {
	pmem::obj::persistent_ptr<char[]> log;
};

int
mt_test(int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s file-name create", argv[0]);

	const char *path = argv[1];
	bool create = std::string(argv[2]) == "1";

	size_t concurrency = 16;

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
		/* If running this test second time, on already exiting pool,
		 * the queue should be empty */
		pop = pmem::obj::pool<root>::open(std::string(path), LAYOUT);
	}

	auto proot = pop.root();

	auto queue = pmem::obj::experimental::mpsc_queue(proot->log, 10000,
							 concurrency);

	std::vector<std::string> values = {"xxx", "aaaaaaa", "bbbbb", "cccc"};

	std::vector<std::thread> threads;
	threads.reserve(concurrency);

	volatile bool execution_end = false;

	for (size_t i = 0; i < concurrency; ++i) {
		threads.emplace_back([&]() {
			auto worker = queue.register_worker();
			for (auto &e : values) {
				auto acc = worker.produce(e.size());
				acc.add(e.data(), e.size());
			}
		});
	};

	std::thread consumer([&]() {
		std::vector<std::string> values_on_pmem;
		/* Read data while writting */
		while (!execution_end) {
			auto rd_acc = queue.consume();
			for (auto str : rd_acc) {
				values_on_pmem.emplace_back(str.data(),
							    str.size());
			}
		}
		/* Read rest of data */
		{
			auto rd_acc = queue.consume();
			for (auto str : rd_acc) {
				values_on_pmem.emplace_back(str.data(),
							    str.size());
			}
		}
		for (auto v : values) {
			auto count = std::count(values_on_pmem.begin(),
						values_on_pmem.end(), v);
			UT_ASSERTeq(count, concurrency);
		}
	});

	for (auto &t : threads) {
		t.join();
	}
	execution_end = true;
	consumer.join();

	return 0;
}

int
main(int argc, char *argv[])
{

	return run_test([&] { mt_test(argc, argv); });
}