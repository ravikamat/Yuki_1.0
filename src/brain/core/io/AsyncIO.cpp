// ═══════════════════════════════════════════════════════════════════════════
// AsyncIO.cpp — Platform-specific async I/O implementations
//
// Windows : IOCP (I/O Completion Ports) + GetQueuedCompletionStatusEx
// Linux   : io_uring (preferred) / epoll (fallback)
// macOS   : kqueue
// ═══════════════════════════════════════════════════════════════════════════
#include "AsyncIO.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>

// ── Platform detection ───────────────────────────────────────────────────
#if defined(_WIN32)
#  define YUKI_IOCP
#  ifndef NOMINMAX
#    define NOMINMAX    // prevent windows.h from defining min/max macros
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")

#elif defined(__linux__)
#  if __has_include(<liburing.h>)
#    define YUKI_IO_URING
#    include <liburing.h>
#  else
#    define YUKI_EPOLL
#    include <sys/epoll.h>
#    include <unistd.h>
#    include <fcntl.h>
#    include <cerrno>
#  endif

#elif defined(__APPLE__) || defined(__FreeBSD__)
#  define YUKI_KQUEUE
#  include <sys/types.h>
#  include <sys/event.h>
#  include <sys/time.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cerrno>
#endif

namespace yuki::core {

// ── Pending operation record ─────────────────────────────────────────────
struct PendingOp {
    int                  fd        = -1;
    void*                buf       = nullptr;
    size_t               len       = 0;
    IOOpcode             opcode    = IOOpcode::READ;
    IOCompletionCallback callback;
    void*                user_data = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════
// Windows IOCP Implementation
// ═════════════════════════════════════════════════════════════════════════
#if defined(YUKI_IOCP)

class AsyncIO::Impl {
public:
    Impl()  = default;
    ~Impl() { shutdown(); }

    bool init() {
        // Create IOCP with concurrency = 0 (let OS pick)
        iocp_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                          nullptr, 0, 0);
        if (!iocp_) {
            std::cerr << "[AsyncIO/IOCP] CreateIoCompletionPort failed: "
                      << ::GetLastError() << "\n";
            return false;
        }
        events_.resize(64);
        return true;
    }

    void shutdown() {
        if (iocp_) {
            ::CloseHandle(iocp_);
            iocp_ = nullptr;
        }
    }

    bool register_fd(int fd) {
        auto h = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd));
        return ::CreateIoCompletionPort(h, iocp_,
                                        static_cast<ULONG_PTR>(fd),
                                        0) == iocp_;
    }

    void unregister_fd(int /*fd*/) {}

    bool submit(int fd, void* buf, size_t len,
                IOOpcode opcode,
                IOCompletionCallback callback, void* user_data) {
        // Allocate an OVERLAPPED on the heap; freed in poll() after callback.
        auto* ov = new OVERLAPPED{};
        std::memset(ov, 0, sizeof(OVERLAPPED));

        auto op = std::make_unique<PendingOp>();
        op->fd        = fd;
        op->buf       = buf;
        op->len       = len;
        op->opcode    = opcode;
        op->callback  = std::move(callback);
        op->user_data = user_data;
        // Store op pointer in hEvent (unused by IOCP itself)
        ov->hEvent = reinterpret_cast<HANDLE>(op.get());

        {
            // Minimal lock: just inserting into the map
            pending_[ov] = std::move(op);
        }

        SOCKET  sock = static_cast<SOCKET>(fd);
        WSABUF  wb{ static_cast<ULONG>(len),
                    static_cast<CHAR*>(buf) };
        DWORD   transferred = 0;
        DWORD   flags_io    = 0;
        BOOL    ok          = FALSE;

        switch (opcode) {
        case IOOpcode::READ:
            ok = (::WSARecv(sock, &wb, 1, &transferred,
                            &flags_io, ov, nullptr) == 0)
              || (::WSAGetLastError() == WSA_IO_PENDING);
            break;
        case IOOpcode::WRITE:
            ok = (::WSASend(sock, &wb, 1, &transferred,
                            0, ov, nullptr) == 0)
              || (::WSAGetLastError() == WSA_IO_PENDING);
            break;
        default:
            // ACCEPT/CONNECT require platform-specific setup not handled here
            ok = FALSE;
            break;
        }

        if (!ok) {
            pending_.erase(ov);
            delete ov;
            return false;
        }
        return true;
    }

    size_t poll(size_t max_events) {
        ULONG removed = 0;
        BOOL  ok = ::GetQueuedCompletionStatusEx(
            iocp_,
            events_.data(),
            static_cast<ULONG>(std::min(max_events, events_.size())),
            &removed,
            /*dwMilliseconds=*/0,   // non-blocking
            FALSE);

        if (!ok) {
            if (::GetLastError() == WAIT_TIMEOUT) return 0;
            return 0;
        }

        for (ULONG i = 0; i < removed; ++i) {
            auto& entry = events_[i];
            auto* ov    = entry.lpOverlapped;
            int   fd    = static_cast<int>(entry.lpCompletionKey);
            auto  bytes = static_cast<int64_t>(
                              entry.dwNumberOfBytesTransferred);

            auto it = pending_.find(ov);
            if (it != pending_.end()) {
                if (it->second->callback)
                    it->second->callback(fd, bytes, it->second->user_data);
                pending_.erase(it);
            }
            delete ov;
        }
        return static_cast<size_t>(removed);
    }

private:
    HANDLE                                               iocp_    = nullptr;
    std::vector<OVERLAPPED_ENTRY>                        events_;
    std::unordered_map<OVERLAPPED*, std::unique_ptr<PendingOp>> pending_;
};

// ═════════════════════════════════════════════════════════════════════════
// Linux io_uring Implementation
// ═════════════════════════════════════════════════════════════════════════
#elif defined(YUKI_IO_URING)

class AsyncIO::Impl {
public:
    Impl()  = default;
    ~Impl() { shutdown(); }

    bool init() {
        int ret = ::io_uring_queue_init(4096, &ring_, 0);
        if (ret < 0) {
            std::cerr << "[AsyncIO/io_uring] queue_init failed: "
                      << ret << "\n";
            return false;
        }
        return true;
    }

    void shutdown() {
        ::io_uring_queue_exit(&ring_);
    }

    bool register_fd(int /*fd*/) { return true; }
    void unregister_fd(int /*fd*/) {}

    bool submit(int fd, void* buf, size_t len,
                IOOpcode opcode,
                IOCompletionCallback callback, void* user_data) {
        struct io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        if (!sqe) return false;

        uint64_t id = next_id_++;
        auto op = std::make_unique<PendingOp>();
        op->fd = fd; op->buf = buf; op->len = len;
        op->opcode = opcode; op->callback = std::move(callback);
        op->user_data = user_data;
        pending_[id] = std::move(op);

        switch (opcode) {
        case IOOpcode::READ:
            ::io_uring_prep_read(sqe, fd, buf, static_cast<unsigned>(len), 0);
            break;
        case IOOpcode::WRITE:
            ::io_uring_prep_write(sqe, fd, buf, static_cast<unsigned>(len), 0);
            break;
        case IOOpcode::ACCEPT:
            ::io_uring_prep_accept(sqe, fd, nullptr, nullptr, 0);
            break;
        case IOOpcode::CONNECT:
            ::io_uring_prep_connect(sqe, fd, nullptr, 0);
            break;
        }
        ::io_uring_sqe_set_data64(sqe, id);
        ::io_uring_submit(&ring_);
        return true;
    }

    size_t poll(size_t max_events) {
        struct io_uring_cqe* cqes[64];
        int n = ::io_uring_peek_batch_cqe(
            &ring_, cqes,
            static_cast<int>(std::min(max_events, size_t(64))));
        if (n <= 0) return 0;

        for (int i = 0; i < n; ++i) {
            uint64_t id    = ::io_uring_cqe_get_data64(cqes[i]);
            int64_t  bytes = cqes[i]->res;
            auto it = pending_.find(id);
            if (it != pending_.end()) {
                if (it->second->callback)
                    it->second->callback(it->second->fd, bytes,
                                        it->second->user_data);
                pending_.erase(it);
            }
            ::io_uring_cqe_seen(&ring_, cqes[i]);
        }
        return static_cast<size_t>(n);
    }

private:
    struct io_uring ring_{};
    std::atomic<uint64_t>                              next_id_{ 1 };
    std::unordered_map<uint64_t, std::unique_ptr<PendingOp>> pending_;
};

// ═════════════════════════════════════════════════════════════════════════
// Linux epoll Fallback
// ═════════════════════════════════════════════════════════════════════════
#elif defined(YUKI_EPOLL)

class AsyncIO::Impl {
public:
    Impl()  = default;
    ~Impl() { shutdown(); }

    bool init() {
        epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ < 0) {
            std::cerr << "[AsyncIO/epoll] epoll_create1 failed\n";
            return false;
        }
        return true;
    }

    void shutdown() {
        if (epfd_ >= 0) { ::close(epfd_); epfd_ = -1; }
    }

    bool register_fd(int fd) {
        int fl = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        struct epoll_event ev{};
        ev.events   = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd  = fd;
        return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0;
    }

    void unregister_fd(int fd) {
        ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        pending_.erase(fd);
    }

    bool submit(int fd, void* buf, size_t len,
                IOOpcode opcode,
                IOCompletionCallback callback, void* user_data) {
        auto op = std::make_unique<PendingOp>();
        op->fd = fd; op->buf = buf; op->len = len;
        op->opcode = opcode; op->callback = std::move(callback);
        op->user_data = user_data;
        pending_[fd] = std::move(op);
        return true;
    }

    size_t poll(size_t max_events) {
        struct epoll_event events[64];
        int n = ::epoll_wait(epfd_, events,
                             static_cast<int>(std::min(max_events,size_t(64))),
                             0);
        if (n <= 0) return 0;

        size_t done = 0;
        for (int i = 0; i < n; ++i) {
            int fd  = events[i].data.fd;
            auto it = pending_.find(fd);
            if (it == pending_.end()) continue;
            auto& op  = it->second;
            int64_t bytes = 0;
            switch (op->opcode) {
            case IOOpcode::READ:
                bytes = ::read(fd, op->buf, op->len);
                break;
            case IOOpcode::WRITE:
                bytes = ::write(fd, op->buf, op->len);
                break;
            default:
                bytes = -1;
                break;
            }
            bool would_block = (bytes < 0) &&
                               (errno == EAGAIN || errno == EWOULDBLOCK);
            if (!would_block) {
                if (op->callback) op->callback(fd, bytes, op->user_data);
                pending_.erase(it);
                ++done;
            }
        }
        return done;
    }

private:
    int  epfd_ = -1;
    std::unordered_map<int, std::unique_ptr<PendingOp>> pending_;
};

// ═════════════════════════════════════════════════════════════════════════
// macOS / BSD kqueue Implementation
// ═════════════════════════════════════════════════════════════════════════
#elif defined(YUKI_KQUEUE)

class AsyncIO::Impl {
public:
    Impl()  = default;
    ~Impl() { shutdown(); }

    bool init() {
        kq_ = ::kqueue();
        if (kq_ < 0) {
            std::cerr << "[AsyncIO/kqueue] kqueue() failed\n";
            return false;
        }
        return true;
    }

    void shutdown() {
        if (kq_ >= 0) { ::close(kq_); kq_ = -1; }
    }

    bool register_fd(int fd) {
        int fl = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        struct kevent ev[2];
        EV_SET(&ev[0], fd, EVFILT_READ,  EV_ADD | EV_CLEAR, 0, 0, nullptr);
        EV_SET(&ev[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        return ::kevent(kq_, ev, 2, nullptr, 0, nullptr) == 0;
    }

    void unregister_fd(int fd) {
        struct kevent ev[2];
        EV_SET(&ev[0], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
        EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        ::kevent(kq_, ev, 2, nullptr, 0, nullptr);
        pending_.erase(fd);
    }

    bool submit(int fd, void* buf, size_t len,
                IOOpcode opcode,
                IOCompletionCallback callback, void* user_data) {
        auto op = std::make_unique<PendingOp>();
        op->fd = fd; op->buf = buf; op->len = len;
        op->opcode = opcode; op->callback = std::move(callback);
        op->user_data = user_data;
        pending_[fd] = std::move(op);
        return true;
    }

    size_t poll(size_t max_events) {
        struct kevent events[64];
        struct timespec ts{ 0, 0 };
        int n = ::kevent(kq_, nullptr, 0, events,
                         static_cast<int>(std::min(max_events, size_t(64))),
                         &ts);
        if (n <= 0) return 0;

        size_t done = 0;
        for (int i = 0; i < n; ++i) {
            int fd  = static_cast<int>(events[i].ident);
            auto it = pending_.find(fd);
            if (it == pending_.end()) continue;
            auto& op    = it->second;
            int64_t bytes = 0;
            switch (op->opcode) {
            case IOOpcode::READ:
                bytes = ::read(fd, op->buf, op->len);
                break;
            case IOOpcode::WRITE:
                bytes = ::write(fd, op->buf, op->len);
                break;
            default:
                bytes = -1;
                break;
            }
            bool would_block = (bytes < 0) &&
                               (errno == EAGAIN || errno == EWOULDBLOCK);
            if (!would_block) {
                if (op->callback) op->callback(fd, bytes, op->user_data);
                pending_.erase(it);
                ++done;
            }
        }
        return done;
    }

private:
    int  kq_ = -1;
    std::unordered_map<int, std::unique_ptr<PendingOp>> pending_;
};

// ═════════════════════════════════════════════════════════════════════════
// Null / Stub Implementation (unknown platform)
// ═════════════════════════════════════════════════════════════════════════
#else

class AsyncIO::Impl {
public:
    bool   init()                             { return true; }
    void   shutdown()                         {}
    bool   register_fd(int)                   { return true; }
    void   unregister_fd(int)                 {}
    bool   submit(int, void*, size_t,
                  IOOpcode,
                  IOCompletionCallback, void*) { return false; }
    size_t poll(size_t)                       { return 0; }
};

#endif // platform

// ── AsyncIO public API (thin delegation to Impl) ─────────────────────────

AsyncIO::AsyncIO()  : impl_(std::make_unique<Impl>()) {}
AsyncIO::~AsyncIO() = default;

bool   AsyncIO::init()                      { return impl_->init(); }
void   AsyncIO::shutdown()                  { impl_->shutdown(); }
bool   AsyncIO::register_fd(int fd)         { return impl_->register_fd(fd); }
void   AsyncIO::unregister_fd(int fd)       { impl_->unregister_fd(fd); }
size_t AsyncIO::poll(size_t max_events)     { return impl_->poll(max_events); }

bool AsyncIO::submit(int fd, void* buf, size_t len,
                     IOOpcode opcode,
                     IOCompletionCallback callback,
                     void* user_data) {
    return impl_->submit(fd, buf, len, opcode,
                         std::move(callback), user_data);
}

} // namespace yuki::core
