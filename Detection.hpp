// ============================================================================
//  十、天外之魔侦测机制（保留优化，无魔气值）
//    1. 普通蛊师、凡人、普通蛊仙：无法侦测
//    2. 仅高阶天道存在、幽魂高阶分魂、天庭侦查蛊、盗天地灵可微弱感应
//    3. 暴露仅来自：战斗泄露道痕、专属侦查杀招、情报出卖
//    4. 无任何数值概率暴露，纯行为判定
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

struct Cultivator;  // 前向声明

// 谁能侦测天外之魔
enum class DetectorClass : std::uint8_t {
    None            = 0,  // 普通蛊师、凡人、普通蛊仙
    HighRankHeaven  = 1,  // 高阶天道存在
    YouHunFragment  = 2,  // 幽魂高阶分魂
    TianTingScoutGu = 3,  // 天庭侦查蛊
    DaoTianEarthSpirit = 4  // 盗天地灵
};

inline const char* to_string(DetectorClass d) {
    switch (d) {
        case DetectorClass::None:            return "无法侦测";
        case DetectorClass::HighRankHeaven:  return "高阶天道存在";
        case DetectorClass::YouHunFragment:  return "幽魂高阶分魂";
        case DetectorClass::TianTingScoutGu: return "天庭侦查蛊";
        case DetectorClass::DaoTianEarthSpirit: return "盗天地灵";
    }
    return "？";
}

inline bool canDetectOtherworldlyDemon(DetectorClass d) {
    return d != DetectorClass::None;
}

// 暴露来源（纯行为判定，无魔气值）
enum class ExposureSource : std::uint8_t {
    None            = 0,
    BattleDaoMarkLeak = 1,  // 战斗泄露道痕
    DetectionMove     = 2,  // 专属侦查杀招
    IntelBetrayal     = 3   // 情报出卖
};

inline const char* to_string(ExposureSource s) {
    switch (s) {
        case ExposureSource::None:              return "无";
        case ExposureSource::BattleDaoMarkLeak: return "战斗泄露道痕";
        case ExposureSource::DetectionMove:     return "专属侦查杀招";
        case ExposureSource::IntelBetrayal:     return "情报出卖";
    }
    return "？";
}

// 暴露记录（不含任何数值概率字段）
struct DemonExposure {
    std::vector<ExposureSource> sources;
    std::vector<std::string>    witnesses;   // 目击/知悉者
    bool publiclyExposed = false;

    bool any() const { return !sources.empty(); }
    void add(ExposureSource s, const std::string& witness);
    void reset();
};

// 侦测等级（行为判定结果，非概率）
enum class DetectionLevel : std::uint8_t {
    Undetected = 0,
    Faint      = 1,   // 微弱感应
    Suspected  = 2,   // 起疑
    Confirmed  = 3    // 确认
};

inline const char* to_string(DetectionLevel l) {
    switch (l) {
        case DetectionLevel::Undetected: return "未暴露";
        case DetectionLevel::Faint:      return "微弱感应";
        case DetectionLevel::Suspected:  return "起疑";
        case DetectionLevel::Confirmed:  return "确认";
    }
    return "？";
}

// 侦测结算
struct DetectionResult {
    DetectionLevel level = DetectionLevel::Undetected;
    DetectorClass  byWhom = DetectorClass::None;
    std::string    detail;
    bool detected() const { return level != DetectionLevel::Undetected; }
};

class OtherworldlyDemonDetection {
public:
    // 纯行为判定：
    //  - 无暴露行为 → 无论谁在场都侦测不到（无魔气值，不做概率掷骰）
    //  - 有暴露行为 → 只有具备资格的侦测者能捕捉；资格越强，判定层级越高
    static DetectionResult evaluate(const DemonExposure& exposure,
                                    DetectorClass observer,
                                    bool targetIsOtherworldlyDemon);

    // 记录一次战斗泄露道痕
    static void recordBattleLeak(DemonExposure& e, const std::string& witness,
                                 Dao leakedDao);
    // 记录一次侦查杀招命中
    static void recordDetectionMove(DemonExposure& e, const std::string& witness);
    // 记录一次情报出卖
    static void recordIntelBetrayal(DemonExposure& e, const std::string& witness);
};

} // namespace gr
