#pragma once
// RandomEngine.h - Thread-safe global random engine accessor
// Provides a single shared mt19937 instance with proper initialization order
// Implementation in RandomEngine.cpp to ensure single definition across TUs

#include <random>

namespace yuki {
namespace core {

// Thread-safe accessor for the global random engine
// Guaranteed single instance across all translation units
std::mt19937& getRandomEngine() noexcept;

} // namespace core
} // namespace yuki