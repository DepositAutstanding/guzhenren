// ============================================================================
//  七、道境体系（新增完善）
//    7.1 每一道分为：入门 → 小成 → 大成 → 圆满 → 道基 → 道痕贯通
//    7.2 道境绑定战力：
//        玩家某一道境界越高 ——
//          · 该道蛊虫威力大幅增幅
//          · 该道杀招消耗降低、反噬降低
//          · 该道道痕抗性提升
//          · 组合该道杀招更稳定、更少崩解
//        道境不足，强行使用高阶杀招 ——
//          · 反噬暴涨 · 杀招残缺 · 大概率直接崩碎蛊虫
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <algorithm>
#include <map>
#include <cmath>
#include <utility>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 道境档案
class DaoProficiency {
public:
    void   set(Dao d, DaoLevel l) { levels_[index(d)] = l; }
    DaoLevel get(Dao d) const {
        auto it = levels_.find(index(d));
        return it == levels_.end() ? DaoLevel::Entry : it->second;
    }
    void raise(Dao d, int steps = 1) {
        int v = static_cast<int>(get(d)) + steps;
        v = std::clamp(v, 0, static_cast<int>(DaoLevel::MarkFusion));
        set(d, static_cast<DaoLevel>(v));
    }
    std::vector<std::pair<Dao, DaoLevel>> all() const {
        std::vector<std::pair<Dao, DaoLevel>> v;
        v.reserve(levels_.size());
        for (const auto& kv : levels_)
            v.emplace_back(static_cast<Dao>(kv.first), kv.second);
        return v;
    }
    // 最高道境（用于判断「道主级」）
    DaoLevel peak() const {
        DaoLevel p = DaoLevel::Entry;
        for (const auto& kv : levels_) p = std::max(p, kv.second);
        return p;
    }
    bool empty() const { return levels_.empty(); }

private:
    static std::size_t index(Dao d) { return static_cast<std::size_t>(d); }
    std::map<std::size_t, DaoLevel> levels_;  // 稀疏存储，按道索引有序
};

// ---------------------------------------------------------------- 系数表
// 以「入门」为基准 1.0，逐级递增
struct DaoCoefficients {
    double power;        // 该道蛊虫威力增幅
    double cost;         // 杀招消耗系数（越低越省）
    double backlash;     // 反噬系数（越低越安全）
    double resistance;   // 道痕抗性
    double stability;    // 杀招稳定度（越高越不易崩解）
};

inline DaoCoefficients dao_coefficients(DaoLevel l) {
    switch (l) {
        case DaoLevel::Entry:      return {1.00, 1.00, 1.00, 0.00, 0.30};
        case DaoLevel::Small:      return {1.45, 0.88, 0.82, 0.15, 0.48};
        case DaoLevel::Great:      return {2.10, 0.74, 0.62, 0.32, 0.66};
        case DaoLevel::Perfection: return {3.20, 0.60, 0.42, 0.52, 0.82};
        case DaoLevel::Foundation: return {5.00, 0.48, 0.26, 0.70, 0.92};
        case DaoLevel::MarkFusion: return {8.00, 0.35, 0.12, 0.88, 0.98};
    }
    return {1.00, 1.00, 1.00, 0.00, 0.30};
}

// 数值型等级，便于比较与插值（0 ~ 5）
inline int dao_level_value(DaoLevel l) { return static_cast<int>(l); }

// 道境是否满足杀招要求
inline bool dao_meets(DaoLevel have, DaoLevel need) {
    return dao_level_value(have) >= dao_level_value(need);
}

// 道境缺口（负数表示超出要求）
inline int dao_gap(DaoLevel have, DaoLevel need) {
    return dao_level_value(have) - dao_level_value(need);
}

} // namespace gr
