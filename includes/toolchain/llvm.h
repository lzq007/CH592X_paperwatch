/*
 * Copyright (c) 2010-2014,2017 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_TOOLCHAIN_LLVM_H_
#define ZEPHYR_INCLUDE_TOOLCHAIN_LLVM_H_

/**
 * @file
 * @brief LLVM/Clang toolchain abstraction
 *
 * Clang is compatible with GCC for the attributes/builtins used by this
 * project (atomic builtins, packed structs, section attributes, etc.).
 * Reuse the GCC definitions so the code resolves identically under both
 * toolchains.
 */

#include <toolchain/gcc.h>

#endif /* ZEPHYR_INCLUDE_TOOLCHAIN_LLVM_H_ */
