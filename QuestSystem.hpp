// ============================================================================
//  支线任务系统
//
//  当前落点：支线「方源将定仙游借给你」。
//
//  为什么必须有这条支线：定仙游是仙蛊、世间唯一，原著归方源所有。
//  若把它做成「六转自动获得」，就与项目自身的仙蛊唯一铁律直接冲突
//  （这正是此前版本的做法，属设计错误，已修正）。
//  但玩家若永远拿不到，定仙游机制就无从体验 —— 于是以「借用」这条
//  支线来调和：所有权始终在方源，玩家只取得有期限的使用权。
//
//  借用期间仍受同一铁律约束：只能跳往亲眼见过 / 抵达过 / 感知过的坐标。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/cultivator/Cultivator.hpp"

#include <string>
#include <vector>

namespace gr {

enum class QuestState : std::uint8_t {
    Locked = 0,      // 未满足条件，不可接取
    Available,       // 可接取
    Active,          // 进行中
    Completed,       // 已完成
    Failed           // 已失败
};

inline const char* to_string(QuestState s) {
    switch (s) {
        case QuestState::Locked:    return "未解锁";
        case QuestState::Available: return "可接取";
        case QuestState::Active:    return "进行中";
        case QuestState::Completed: return "已完成";
        case QuestState::Failed:    return "已失败";
    }
    return "？";
}

enum class QuestId : std::uint8_t {
    None = 0,
    BorrowDingXianYou = 1,   // 支线：方源将定仙游借给你
};

inline const char* to_string(QuestId q) {
    switch (q) {
        case QuestId::None:               return "无";
        case QuestId::BorrowDingXianYou:  return "方源将定仙游借给你";
    }
    return "？";
}

struct QuestView {
    QuestId    id = QuestId::None;
    std::string name;
    std::string desc;
    QuestState state = QuestState::Locked;
    std::string objective;      // 当前目标描述
    std::string reward;
    std::string giver;          // 发布者
};

class QuestSystem {
public:
    // ------------------------------------------------------------------
    //  情报：定仙游在谁手上并非公开信息
    //
    //  须先得知此条情报，支线「方源将定仙游借给你」才谈得上触发 ——
    //  否则等于默认玩家全知，既不合理也让支线失去了「获知」这一环节。
    // ------------------------------------------------------------------
    static constexpr const char* kIntelFangYuanDingXianYou = "intel_fangyuan_dingxianyou";

    // 接取（仅 Available 时可接）
    struct AcceptResult {
        bool ok = false;
        std::string detail;
    };
    AcceptResult accept(QuestId q, Cultivator& player);

    // 每次世界推进后调用，检查完成条件并结算
    struct ProgressReport {
        bool completed = false;
        bool expired   = false;
        std::vector<std::string> notes;
    };
    ProgressReport update(Cultivator& player, const class WorldMap& world, Tick now);

    // 定仙游借用期限到期时收回
    void tickLend(Cultivator& player, Tick now);

    bool isCompleted(QuestId q) const;
    QuestState stateOf(QuestId q) const;

    // UI 只读视图
    std::vector<QuestView> views(const Cultivator& player) const;

    // 借用剩余刻度（0 = 未借用或不限期）
    int lendTicksLeft(const Cultivator& player, Tick now) const;

private:
    // 支线：方源将定仙游借给你
    bool   dxAvailable(const Cultivator& p) const;
    bool   dxConditionMet(const Cultivator& p) const;

    bool accepted_ = false;
    bool completed_ = false;
    Tick lendUntil_ = 0;
    Tick lendStart_ = 0;
    bool unlimited_ = false;
};

} // namespace gr
