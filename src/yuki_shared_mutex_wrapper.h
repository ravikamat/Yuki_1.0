#pragma once
// yuki_shared_mutex_wrapper.h — always uses yuki::concurrency::shared_mutex.
//
// All call sites use the nested types:
//   yuki::concurrency::shared_mutex::unique_lock  (exclusive / writer)
//   yuki::concurrency::shared_mutex::shared_lock  (shared / reader)
//
// std::shared_mutex does NOT provide these nested types, so we cannot
// transparently alias it.  The custom implementation in yuki_shared_mutex.h
// is portable C++17/20 using only std::mutex + std::condition_variable and
// avoids the old MSVC STL <shared_mutex> bug (pre-19.38) entirely.

#include "yuki_shared_mutex.h"