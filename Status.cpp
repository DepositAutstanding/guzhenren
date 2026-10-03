// 状态效果表实现
#include "gr/core/Status.hpp"

#include <algorithm>

namespace gr {

const StatusEffect* find_status(const std::vector<StatusEffect>& v, StatusKind k) {
    auto it = std::find_if(v.begin(), v.end(),
                           [k](const StatusEffect& s) { return s.kind == k; });
    return it == v.end() ? nullptr : &(*it);
}

StatusEffect* find_status_mut(std::vector<StatusEffect>& v, StatusKind k) {
    auto it = std::find_if(v.begin(), v.end(),
                           [k](const StatusEffect& s) { return s.kind == k; });
    return it == v.end() ? nullptr : &(*it);
}

void apply_status(std::vector<StatusEffect>& v, StatusEffect s) {
    if (StatusEffect* cur = find_status_mut(v, s.kind)) {
        cur->magnitude   = std::max(cur->magnitude, s.magnitude);
        cur->remainTicks = std::max(cur->remainTicks, s.remainTicks);
        cur->name        = s.name;
    } else {
        v.push_back(std::move(s));
    }
}

void clear_env_statuses(std::vector<StatusEffect>& v) {
    auto is_env = [](const StatusEffect& s) {
        return s.kind == StatusKind::BeiYuanSevereCold ||
               s.kind == StatusKind::NanJiangMiasma ||
               s.kind == StatusKind::HeavenSuppression ||
               s.kind == StatusKind::TiangangGale;
    };
    v.erase(std::remove_if(v.begin(), v.end(), is_env), v.end());
}

void tick_statuses(std::vector<StatusEffect>& v, int dt) {
    for (auto& s : v) {
        if (s.remainTicks > 0) s.remainTicks -= dt;
    }
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const StatusEffect& s) { return s.remainTicks < 0; }),
            v.end());
}

} // namespace gr
