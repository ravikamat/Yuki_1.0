// ═══════════════════════════════════════════════════════════════════════════
// AsyncIO.h — Cross-platform async I/O abstraction layer
//
// Platforms:
//   • Windows : I/O Completion Ports (IOCP) + OVERLAPPED
//   • Linux   : io_uring (kernel 5.1+); epoll fallback
//   • macOS   : kqueue
//
// Unified interface:
//   asyncio.submit(fd, buf, len, opcode, callback, user_data)
//   asyncio.poll(max_events)   <- called from event-loop thread
//
// Thread safety:
//   • submit() is thread-safe (may be called from any thread)
//   • poll()   must be called from the single event-loop thread only
//
// Reference:
//   • "I/O Completion Ports", MSDN / Windows Documentation
//   • J. Corbet, "Ringing in a new asynchronous I/O API", LWN.net 2019
//   • kqueue(2), FreeBSD / macOS man pages
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>

namespace yuki::core {

// ── I/O operation types ─────────────────────────────────────────────────
enum class IOOpcode : uint8_t {
    READ    = 0,
    WRITE   = 1,
    ACCEPT  = 2,
    CONNECT = 3,
};

// ── Completion callback ──────────────────────────────────────────────────
// Called from poll() on the event-loop thread.
//   fd        : file descriptor / socket handle
//   bytes     : bytes transferred (negative = error code)
//   user_data : opaque pointer passed to submit()
using IOCompletionCallback =
    std::function<void(int fd, int64_t bytes, void* user_data)>;

// ── AsyncIO ─────────────────────────────────────────────────────────────
// Singleton-friendly, owns platform-specific kernel handles.
// Pimpl idiom keeps platform headers out of user code.
class AsyncIO {
public:
    AsyncIO();
    ~AsyncIO();

    // Non-copyable, non-movable (owns OS handles)
    AsyncIO(const AsyncIO&)            = delete;
    AsyncIO& operator=(const AsyncIO&) = delete;
    AsyncIO(AsyncIO&&)                 = delete;
    AsyncIO& operator=(AsyncIO&&)      = delete;

    // Initialise the I/O subsystem (create IOCP / kqueue / epoll fd).
    // Must be called exactly once before any other method.
    bool init();

    // Graceful shutdown: cancels pending ops and closes kernel handles.
    void shutdown();

    // Register fd so the kernel can deliver completions for it.
    // Must be called before the first submit() on a new fd.
    bool register_fd(int fd);

    // Unregister fd (e.g. on close).
    void unregister_fd(int fd);

    // Submit an async I/O operation.
    //   buf       : must remain valid until the completion callback fires!
    //   callback  : invoked from poll() on the event-loop thread
    // Returns false if the internal submission queue is full.
    bool submit(int fd, void* buf, size_t len,
                IOOpcode opcode,
                IOCompletionCallback callback,
                void* user_data = nullptr);

    // Drain up to max_events completions (non-blocking; returns immediately
    // if no completions are ready).  Must be called from the event-loop
    // thread only.
    size_t poll(size_t max_events = 64);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace yuki::core
