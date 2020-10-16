# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

include(${SRC_DIR}/../helpers.cmake)

setup()


execute_process(COMMAND pmempool create obj --layout=transaction-example-layout -s 100M ${DIR}/testfile )

execute(${TEST_EXECUTABLE} ${DIR}/testfile)

finish()
