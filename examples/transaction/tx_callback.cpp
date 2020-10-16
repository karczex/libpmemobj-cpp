// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

//! [tx_callback_example]
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/mutex.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pext.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/shared_mutex.hpp>
#include <libpmemobj++/transaction.hpp>

using namespace pmem::obj;

/* pool root structure */
struct root {
	p<int> count;
};

void
show_usage(char *argv[])
{
	std::cerr << "usage: " << argv[0] << " pool-path" << std::endl;
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		show_usage(argv);
		return 1;
	}

	const char *path = argv[1];
	const char *layout = "transaction-example-layout";

	pool<root> pop;

	bool cb_called = false;
	auto internal_tx_function = [&] {
		/* callbacks can be registered even in inner transaction but
		 * will be called when outer transaction ends */
		transaction::run(pop, [&] {
			transaction::register_callback(
				transaction::stage::oncommit,
				[&] { cb_called = true; });
		});

		/* cb_called is false here if internal_tx_function is called
		 * inside another transaction */
	};

	try {
		pop = pool<root>::open(path, layout);
		transaction::run(pop, [&] { internal_tx_function(); });

		/* cb_called == true if transaction ended successfully */
	} catch (pmem::pool_error &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << "To create pool run: pmempool create obj --layout="
			  << layout << " -s 100M path_to_pool" << std::endl;
	} catch (pmem::transaction_error &) {
		/* an internal transaction error occurred, tx aborted
		 * reacquire locks if necessary */
	} catch (const std::exception &e) {
		/* some other exception thrown, tx aborted
		 * reacquire locks if necessary */
		std::cerr << "Exception " << e.what() << std::endl;
		return -1;
	}
}

//! [tx_callback_example]
