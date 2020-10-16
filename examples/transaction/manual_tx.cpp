// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

//! [manual_tx_example]
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
	mutex pmutex;
	shared_mutex shared_pmutex;
	p<int> count;
	persistent_ptr<root> another_root;
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
	try {

		pop = pool<root>::open(path, layout);
		auto proot = pop.root();
		transaction::manual tx(pop, proot->pmutex,
				       proot->shared_pmutex);

		/* atomically allocate objects */
		proot->another_root = make_persistent<root>();

		/* atomically modify objects */
		proot->count++;

		/* It's necessary to commit the transaction manually and
		 * it has to be the last operation in the transaction. */
		transaction::commit();
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

	/* In complex cases with library calls, remember to check the status of
	 * the previous transaction. */
	return transaction::error();
}
//! [manual_tx_example]
