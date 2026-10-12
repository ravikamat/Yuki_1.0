// RandomEngine.cpp - Single definition of global random engine
// Ensures exactly one mt19937 instance across all translation units

#include "brain/core/RandomEngine.h"
#include <chrono>

namespace yuki {
namespace core {

std::mt19937& getRandomEngine() noexcept {
    // Function-local static - guaranteed thread-safe initialization (C++20)
    // Single definition because this is in a .cpp file, not a header
    static std::mt19937 engine([]() -> std::mt19937 {
        std::mt19937 e;
        e.seed(static_cast<std::mt19937::result_type>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
        return e;
    }());
    return engine;
}

} // namespace core
} // namespace yuki