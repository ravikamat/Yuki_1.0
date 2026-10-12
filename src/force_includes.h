// Force include to fix MSVC STL bug in <shared_mutex>
// MSVC's shared_mutex uses chrono/mutex/condition_variable without std:: prefix
// This must be included BEFORE any header that includes <shared_mutex>
#include <chrono>
#include <mutex>
#include <condition_variable>