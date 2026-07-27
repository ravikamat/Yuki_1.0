// NeuromodulatorState.h — global chemical neuromodulator state with atomic<float>.
#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace ync {

struct NeuromodulatorState {
    // Direct atomic<float> — MSVC C++17 stable for load/store
    std::atomic<float> dopamine{0.5f};
    std::atomic<float> serotonin{0.5f};
    std::atomic<float> acetylcholine{0.5f};
    std::atomic<float> noradrenaline{0.3f};

    NeuromodulatorState() = default;

    // Explicit copy ctor/assign (atomics non-copyable)
    NeuromodulatorState(const NeuromodulatorState& o) noexcept {
        dopamine.store(     o.dopamine.load(std::memory_order_relaxed),      std::memory_order_relaxed);
        serotonin.store(    o.serotonin.load(std::memory_order_relaxed),     std::memory_order_relaxed);
        acetylcholine.store(o.acetylcholine.load(std::memory_order_relaxed), std::memory_order_relaxed);
        noradrenaline.store(o.noradrenaline.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    NeuromodulatorState& operator=(const NeuromodulatorState& o) noexcept {
        dopamine.store(     o.dopamine.load(std::memory_order_relaxed),      std::memory_order_relaxed);
        serotonin.store(    o.serotonin.load(std::memory_order_relaxed),     std::memory_order_relaxed);
        acetylcholine.store(o.acetylcholine.load(std::memory_order_relaxed), std::memory_order_relaxed);
        noradrenaline.store(o.noradrenaline.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    // Compatibility accessors (call sites that use getDopamine() etc.)
    float getDopamine()      const noexcept { return dopamine.load(std::memory_order_relaxed); }
    float getSerotonin()     const noexcept { return serotonin.load(std::memory_order_relaxed); }
    float getAcetylcholine() const noexcept { return acetylcholine.load(std::memory_order_relaxed); }
    float getNoradrenaline() const noexcept { return noradrenaline.load(std::memory_order_relaxed); }

    static constexpr float kDecayRate = 0.001f;
    static constexpr float kBaseDopamine      = 0.5f;
    static constexpr float kBaseSerotonin     = 0.5f;
    static constexpr float kBaseAcetylcholine = 0.5f;
    static constexpr float kBaseNoradrenaline = 0.3f;

    void decay() noexcept {
        auto decay_toward = [](std::atomic<float>& a, float base) {
            float v = a.load(std::memory_order_relaxed);
            v += (base - v) * kDecayRate;
            a.store(v, std::memory_order_relaxed);
        };
        decay_toward(dopamine,      kBaseDopamine);
        decay_toward(serotonin,     kBaseSerotonin);
        decay_toward(acetylcholine, kBaseAcetylcholine);
        decay_toward(noradrenaline, kBaseNoradrenaline);
    }

    void onReward(float magnitude) noexcept {
        float d = dopamine.load(std::memory_order_relaxed) + magnitude * 0.3f;
        dopamine.store(std::min(d, 1.0f), std::memory_order_release);
        float s = serotonin.load(std::memory_order_relaxed) + magnitude * 0.1f;
        serotonin.store(std::min(s, 1.0f), std::memory_order_release);
    }

    void onPunishment(float magnitude) noexcept {
        float d = dopamine.load(std::memory_order_relaxed) - magnitude * 0.2f;
        dopamine.store(std::max(d, 0.0f), std::memory_order_release);
        float na = noradrenaline.load(std::memory_order_relaxed) + magnitude * 0.2f;
        noradrenaline.store(std::min(na, 1.0f), std::memory_order_release);
    }

    void onSurprise(float free_energy) noexcept {
        float na = noradrenaline.load(std::memory_order_relaxed) + free_energy * 0.15f;
        noradrenaline.store(std::min(na, 1.0f), std::memory_order_release);
        float ach = acetylcholine.load(std::memory_order_relaxed) + free_energy * 0.1f;
        acetylcholine.store(std::min(ach, 1.0f), std::memory_order_release);
    }

    void onFocus() noexcept {
        float ach = acetylcholine.load(std::memory_order_relaxed) + 0.05f;
        acetylcholine.store(std::min(ach, 1.0f), std::memory_order_release);
    }

    void onRelax() noexcept {
        float s = serotonin.load(std::memory_order_relaxed) + 0.05f;
        serotonin.store(std::min(s, 1.0f), std::memory_order_release);
        float na = noradrenaline.load(std::memory_order_relaxed) - 0.1f;
        noradrenaline.store(std::max(na, 0.0f), std::memory_order_release);
    }

    void serialize(uint8_t* buf, size_t& offset) const noexcept {
        auto writeF = [&](float v) {
            std::memcpy(buf + offset, &v, 4); offset += 4;
        };
        writeF(dopamine.load(std::memory_order_relaxed));
        writeF(serotonin.load(std::memory_order_relaxed));
        writeF(acetylcholine.load(std::memory_order_relaxed));
        writeF(noradrenaline.load(std::memory_order_relaxed));
    }

    void deserialize(const uint8_t* buf, size_t& offset) noexcept {
        auto readF = [&]() -> float {
            float v; std::memcpy(&v, buf + offset, 4); offset += 4; return v;
        };
        dopamine.store(readF(),      std::memory_order_relaxed);
        serotonin.store(readF(),     std::memory_order_relaxed);
        acetylcholine.store(readF(), std::memory_order_relaxed);
        noradrenaline.store(readF(), std::memory_order_relaxed);
    }
};

} // namespace ync
