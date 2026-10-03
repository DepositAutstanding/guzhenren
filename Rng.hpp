// ============================================================================
//  确定性随机源
//  世界为单机（预留联机），一切随机必须可复现：
//  同一 seed + 同一 tick 序列 => 完全相同的世界演化，便于测试与存档回放。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace gr {

class Rng {
public:
    explicit Rng(std::uint64_t seed = 0x5EED1234u) : eng_(seed) {}

    void reseed(std::uint64_t seed) { eng_.seed(seed); }

    // [0.0, 1.0)
    Ratio next() { return std::uniform_real_distribution<Ratio>(0.0, 1.0)(eng_); }

    // [lo, hi) 实数
    Ratio range(Ratio lo, Ratio hi) { return lo + (hi - lo) * next(); }

    // [lo, hi] 整数
    std::int64_t rangeInt(std::int64_t lo, std::int64_t hi) {
        return std::uniform_int_distribution<std::int64_t>(lo, hi)(eng_);
    }

    bool chance(Ratio p) { return next() < p; }

    template <typename T>
    const T& pick(const std::vector<T>& v) {
        return v[static_cast<std::size_t>(rangeInt(0, static_cast<std::int64_t>(v.size()) - 1))];
    }

private:
    std::mt19937_64 eng_;
};

} // namespace gr
