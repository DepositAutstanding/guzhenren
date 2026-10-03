// ============================================================================
//  状态效果（DEBUFF / BUFF）统一模型
//  第二章「五域环境 DEBUFF 全新修正」：
//    · 北原严寒、南疆瘴气均为「特定时段 / 特定区域」触发，非全域常驻
//    · 本域出生单位对本域环境负面状态高额免疫；跨域者全额承受
//    · 黑天、白天位面压制属天地规则压制，无本土免疫，凡俗生灵平等承受
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

enum class StatusKind : std::uint8_t {
    // 环境类
    BeiYuanSevereCold = 0,  // 北原严寒
    NanJiangMiasma,         // 南疆瘴气
    HeavenSuppression,      // 黑天/白天位面压制（天地规则，无本土免疫）
    TiangangGale,           // 天罡罡风（白天天罡风带）
    // 修行类
    ThreeQiImbalance,       // 三气失衡
    DaoBacklash,            // 道境不足的反噬
    // 其他
    Custom
};

inline const char* to_string(StatusKind k) {
    switch (k) {
        case StatusKind::BeiYuanSevereCold: return "北原严寒";
        case StatusKind::NanJiangMiasma:    return "南疆瘴气";
        case StatusKind::HeavenSuppression: return "两天位面压制";
        case StatusKind::TiangangGale:      return "天罡罡风";
        case StatusKind::ThreeQiImbalance:  return "三气失衡";
        case StatusKind::DaoBacklash:       return "道痕反噬";
        case StatusKind::Custom:            return "自定义状态";
    }
    return "未知状态";
}

struct StatusEffect {
    StatusKind kind = StatusKind::Custom;
    std::string name;
    double magnitude = 0.0;   // 强度 0.0~1.0+
    int   remainTicks = 0;    // <=0 表示持续型，由环境每帧重算

    // 该状态是否允许被「本土免疫」规则削减（天地规则压制不可免疫）
    bool immuneByNativeRule() const {
        return kind == StatusKind::BeiYuanSevereCold ||
               kind == StatusKind::NanJiangMiasma;
    }
};

// 从列表取同类状态指针，无则 nullptr
const StatusEffect* find_status(const std::vector<StatusEffect>& v, StatusKind k);
StatusEffect*       find_status_mut(std::vector<StatusEffect>& v, StatusKind k);

// upsert：存在则刷新强度，不存在则追加
void apply_status(std::vector<StatusEffect>& v, StatusEffect s);

// 清除指定环境类状态（离开触发区域时调用）
void clear_env_statuses(std::vector<StatusEffect>& v);

// 推进持续型状态的剩余时间
void tick_statuses(std::vector<StatusEffect>& v, int dt = 1);

} // namespace gr
