#ifndef DF8BB6A9_D11B_4DA4_AFCD_0ACB441D1D5D
#define DF8BB6A9_D11B_4DA4_AFCD_0ACB441D1D5D
#pragma once

#include <vex/compiler.h>

#define ATOMIC_RELAXED __ATOMIC_RELAXED
#define ATOMIC_CONSUME __ATOMIC_CONSUME
#define ATOMIC_ACQUIRE __ATOMIC_ACQUIRE
#define ATOMIC_RELEASE __ATOMIC_RELEASE
#define ATOMIC_ACQ_REL __ATOMIC_ACQ_REL
#define ATOMIC_SEQ_CST __ATOMIC_SEQ_CST

#define atomic_thread_fence(order) __atomic_thread_fence(order)

#define atomic_load(ptr, order)           __atomic_load_n((ptr), (order))
#define atomic_load_n(ptr, order)         __atomic_load_n((ptr), (order))
#define atomic_store(ptr, value, order)   __atomic_store_n((ptr), (value), (order))
#define atomic_store_n(ptr, value, order) __atomic_store_n((ptr), (value), (order))

#define atomic_fetch_add(ptr, value, order) __atomic_fetch_add((ptr), (value), (order))
#define atomic_fetch_sub(ptr, value, order) __atomic_fetch_sub((ptr), (value), (order))
#define atomic_fetch_and(ptr, value, order) __atomic_fetch_and((ptr), (value), (order))
#define atomic_fetch_or(ptr, value, order)  __atomic_fetch_or((ptr), (value), (order))
#define atomic_fetch_xor(ptr, value, order) __atomic_fetch_xor((ptr), (value), (order))

#define atomic_exchange(ptr, value, order) __atomic_exchange_n((ptr), (value), (order))

#define atomic_cmpxchg(ptr, expected, desired, succ, fail)      __atomic_compare_exchange_n((ptr), (expected), (desired), 0, (succ), (fail))
#define atomic_cmpxchg_weak(ptr, expected, desired, succ, fail) __atomic_compare_exchange_n((ptr), (expected), (desired), 1, (succ), (fail))

#endif /* DF8BB6A9_D11B_4DA4_AFCD_0ACB441D1D5D */
