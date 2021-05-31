// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "unittest.hpp"
#include <cstring>
#include <iostream>

#include <atomic>
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
mt_test(pmem::obj::pool<root> pop, size_t concurrency, size_t buffer_size)
{

	auto proot = pop.root();

	auto queue = pmem::obj::experimental::mpsc_queue(
		proot->log, buffer_size, concurrency);

	std::vector<std::string> values = {"xxx", "aaaaaaa", "bbbbb", "cccc"};

	std::atomic<size_t> semaphore(concurrency);

	std::vector<std::string> values_on_pmem;
	parallel_exec(concurrency + 1, [&](size_t thread_id) {
		if (thread_id == 0) {
			/* Read data while writting */
			while (semaphore.load() > 0) {
				queue.consume([&](auto rd_acc) {
					for (auto str : rd_acc) {
						values_on_pmem.emplace_back(
							str.data(), str.size());
					}
				});
			}
			UT_ASSERTeq(values_on_pmem.empty(), false);
		} else {
			/* Concurrently add data to queue */
			auto worker = queue.register_worker();
			int x = 0;
			for (auto &e : values) {
				bool insert_succeed = false;
				while (!insert_succeed) {
					insert_succeed = worker.produce(
						e.size(), [&](auto range) {
							x++;
							std::copy_n(
								e.begin(),
								e.size(),
								range.begin());
						});
				};
			}
			semaphore--;
		}
	});

	/* Consume rest of the data */
	queue.consume([&](auto rd_acc1) {
		for (auto str : rd_acc1) {
			std::cout << str.size() << " ";
			values_on_pmem.emplace_back(str.data(), str.size());
		}
	});

	for (auto &v : values) {
		auto count = std::count(values_on_pmem.begin(),
					values_on_pmem.end(), v);
		UT_ASSERTeq(count, concurrency);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	constexpr size_t concurrency = 16;
	size_t buffer_size =
		pmem::obj::experimental::CACHELINE_SIZE * concurrency * 3;

	pmem::obj::pool<struct root> pop;

	pop = pmem::obj::pool<root>::create(
		std::string(path), LAYOUT, PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);

	pmem::obj::transaction::run(pop, [&] {
		pop.root()->log =
			pmem::obj::make_persistent<char[]>(buffer_size);
	});

	return run_test([&] { mt_test(pop, concurrency, buffer_size); });
}
