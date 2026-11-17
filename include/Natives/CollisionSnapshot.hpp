#pragma once
#include <mutex>
#include <type_traits>

namespace cvr::physhands {
// Seqlocks around a plain memcpy still race in C++. Readers never wait for the
// physics/animation writer: a busy snapshot is unavailable, using the existing
// freshness/fallback paths. The lock only protects a POD copy, never PhysX calls.
template<class T> class CollisionSnapshot {
    static_assert(std::is_trivially_copyable_v<T>);
    mutable std::mutex mutex;
    T data{};
    bool initialized = false;
public:
    void Write(const T& value) {
        std::lock_guard guard(mutex);
        data = value;
        initialized = true;
    }
    bool Read(T& out) const {
        std::unique_lock guard(mutex, std::try_to_lock);
        if (!guard.owns_lock() || !initialized) return false;
        out = data;
        return true;
    }
};
}
