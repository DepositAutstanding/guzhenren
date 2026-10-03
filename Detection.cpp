// 天外之魔侦测实现：纯行为判定，无任何魔气值 / 概率掷骰
#include "gr/ai/Detection.hpp"

namespace gr {

void DemonExposure::add(ExposureSource s, const std::string& witness) {
    if (s == ExposureSource::None) return;
    sources.push_back(s);
    if (!witness.empty()) witnesses.push_back(witness);
}

void DemonExposure::reset() {
    sources.clear();
    witnesses.clear();
    publiclyExposed = false;
}

DetectionResult OtherworldlyDemonDetection::evaluate(const DemonExposure& exposure,
                                                     DetectorClass observer,
                                                     bool targetIsOtherworldlyDemon) {
    DetectionResult r;
    r.byWhom = observer;

    if (!targetIsOtherworldlyDemon) {
        r.level  = DetectionLevel::Undetected;
        r.detail = "目标为本土生灵，无天外之魔身份";
        return r;
    }
    // 1. 普通蛊师、凡人、普通蛊仙：无法侦测
    if (!canDetectOtherworldlyDemon(observer)) {
        r.level  = DetectionLevel::Undetected;
        r.detail = "普通蛊师 / 凡人 / 普通蛊仙无法侦测天外之魔";
        return r;
    }
    // 4. 无暴露行为 → 不暴露（纯行为判定）
    if (!exposure.any()) {
        r.level  = DetectionLevel::Undetected;
        r.detail = "无任何暴露行为（战斗泄露道痕 / 侦查杀招 / 情报出卖），侦测无果";
        return r;
    }

    // 3. 暴露仅来自三类行为，按严重程度定级
    int score = 0;
    for (ExposureSource s : exposure.sources) {
        if (s == ExposureSource::BattleDaoMarkLeak) score += 1;
        if (s == ExposureSource::DetectionMove)     score += 2;
        if (s == ExposureSource::IntelBetrayal)     score += 3;
    }

    // 2. 仅特定存在可微弱感应；资格越高，同一暴露程度下的结论越强
    switch (observer) {
        case DetectorClass::TianTingScoutGu:
        case DetectorClass::DaoTianEarthSpirit:
            r.level = score >= 5 ? DetectionLevel::Confirmed
                                 : (score >= 2 ? DetectionLevel::Suspected
                                               : DetectionLevel::Faint);
            break;
        case DetectorClass::YouHunFragment:
            r.level = score >= 4 ? DetectionLevel::Confirmed
                                 : (score >= 2 ? DetectionLevel::Suspected
                                               : DetectionLevel::Faint);
            break;
        case DetectorClass::HighRankHeaven:
            r.level = score >= 3 ? DetectionLevel::Confirmed : DetectionLevel::Suspected;
            break;
        default:
            r.level = DetectionLevel::Undetected;
            break;
    }

    r.detail = std::string("侦测者：") + to_string(observer) +
               "；行为依据：" + std::to_string(exposure.sources.size()) +
               " 项；结论：" + to_string(r.level);
    return r;
}

void OtherworldlyDemonDetection::recordBattleLeak(DemonExposure& e,
                                                  const std::string& witness,
                                                  Dao leakedDao) {
    e.add(ExposureSource::BattleDaoMarkLeak,
          witness + "（泄露" + to_string(leakedDao) + "道痕）");
}

void OtherworldlyDemonDetection::recordDetectionMove(DemonExposure& e,
                                                     const std::string& witness) {
    e.add(ExposureSource::DetectionMove, witness);
}

void OtherworldlyDemonDetection::recordIntelBetrayal(DemonExposure& e,
                                                     const std::string& witness) {
    e.add(ExposureSource::IntelBetrayal, witness);
    e.publiclyExposed = true;
}

} // namespace gr
