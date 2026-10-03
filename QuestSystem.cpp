// 支线任务系统实现
#include "gr/sim/QuestSystem.hpp"
#include "gr/world/WorldMap.hpp"

namespace gr {

namespace {
// 方源借出定仙游的期限（世界刻度）。
// 方源是实用主义者，不会无条件长期外借九转级数都不到的稀缺仙蛊，
// 故设为有期限的借用，到期自动收回。
constexpr int kDingXianYouLendTicks = 500;
}

// ---------------------------------------------------------------------------
//  接取
// ---------------------------------------------------------------------------
QuestSystem::AcceptResult QuestSystem::accept(QuestId q, Cultivator& player) {
    if (q != QuestId::BorrowDingXianYou)
        return { false, "暂无此支线" };

    if (completed_)
        return { false, "该支线已完成" };
    if (accepted_)
        return { false, "该支线已在进行中" };
    if (!dxAvailable(player))
        return { false, "尚未满足接取条件：需晋六转、并已知「至尊仙窍」所在" };

    accepted_ = true;
    return { true, "已接取支线「方源将定仙游借给你」—— 前往至尊仙窍面见方源" };
}

// ---------------------------------------------------------------------------
//  推进与结算
// ---------------------------------------------------------------------------
QuestSystem::ProgressReport QuestSystem::update(Cultivator& player,
                                                const WorldMap& world, Tick now) {
    ProgressReport rep;
    (void)world;   // 当前支线只判「是否抵达至尊仙窍」，暂用不到地图数据

    if (!accepted_ || completed_) return rep;

    if (dxConditionMet(player)) {
        completed_ = true;
        // 借用：所有权仍在方源，玩家仅得使用权
        player.dingXianYouPossession = DingXianYouPossession::Lent;
        player.holdsDingXianYou = true;
        lendStart_ = now;
        lendUntil_ = now + kDingXianYouLendTicks;
        unlimited_ = false;
        player.dingXianYouLendTicks = kDingXianYouLendTicks;

        rep.completed = true;
        rep.notes.push_back("方源将定仙游借予你（使用期 " +
                            std::to_string(kDingXianYouLendTicks) + " 刻）");
        rep.notes.push_back("铁律不变：仅可跳往亲眼见过 / 抵达过 / 感知过的坐标");
        rep.notes.push_back("仙蛊唯一：所有权仍属方源，到期收回");
    }
    return rep;
}

void QuestSystem::tickLend(Cultivator& player, Tick now) {
    if (!completed_ || unlimited_) return;
    if (player.dingXianYouPossession != DingXianYouPossession::Lent) return;
    if (now < lendUntil_) {
        player.dingXianYouLendTicks = static_cast<int>(lendUntil_ - now);
        return;
    }
    // 到期收回
    player.dingXianYouPossession = DingXianYouPossession::None;
    player.holdsDingXianYou = false;
    player.dingXianYouLendTicks = 0;
    (void)lendStart_;
}

int QuestSystem::lendTicksLeft(const Cultivator& player, Tick now) const {
    if (player.dingXianYouPossession != DingXianYouPossession::Lent) return 0;
    if (unlimited_) return 0;   // 0 表示不限期
    return static_cast<int>(lendUntil_ > now ? lendUntil_ - now : 0);
}

// ---------------------------------------------------------------------------
//  条件判定
// ---------------------------------------------------------------------------
bool QuestSystem::dxAvailable(const Cultivator& p) const {
    // 凡人见不得方源，也无资格借用仙蛊 —— 须六转以上
    if (!p.isImmortal()) return false;
    // 须先【得知】方源持有定仙游这条情报，支线才成立
    if (!p.knowsIntel(kIntelFangYuanDingXianYou)) return false;
    // 须已知至尊仙窍所在（亲眼见过 / 抵达过 / 感知过）
    return p.dingXianYou.knows("cn_zunzhe_xiantiao");
}

bool QuestSystem::dxConditionMet(const Cultivator& p) const {
    // 完成条件：亲自抵达至尊仙窍，面见方源
    return p.location.siteId == "cn_zunzhe_xiantiao";
}

bool QuestSystem::isCompleted(QuestId q) const {
    if (q == QuestId::BorrowDingXianYou) return completed_;
    return false;
}

QuestState QuestSystem::stateOf(QuestId q) const {
    if (q != QuestId::BorrowDingXianYou) return QuestState::Locked;
    if (completed_) return QuestState::Completed;
    if (accepted_)  return QuestState::Active;
    return QuestState::Locked;
}

// ---------------------------------------------------------------------------
//  UI 视图
// ---------------------------------------------------------------------------
std::vector<QuestView> QuestSystem::views(const Cultivator& player) const {
    std::vector<QuestView> out;

    {
        QuestView v;
        v.id   = QuestId::BorrowDingXianYou;
        v.name = to_string(QuestId::BorrowDingXianYou);
        v.giver = "方源（炼天魔尊）";
        v.desc = "定仙游乃仙蛊、世间唯一，为方源所有。"
                 "若想使用，须亲自前往至尊仙窍向他开口 —— "
                 "他肯不肯借，看你有没有让他动心的理由。";
        v.reward = "定仙游·借用（使用期 " + std::to_string(kDingXianYouLendTicks) +
                   " 刻，到期收回）";

        if (completed_) {
            v.state     = QuestState::Completed;
            v.objective = "已借得定仙游，所有权仍属方源";
        } else if (accepted_) {
            v.state     = QuestState::Active;
            v.objective = "前往至尊仙窍，面见方源（当前位于：" +
                          player.location.region + "）";
        } else if (dxAvailable(player)) {
            v.state     = QuestState::Available;
            v.objective = "可前往至尊仙窍求借";
        } else {
            v.state = QuestState::Locked;
            if (!player.isImmortal()) {
                v.objective = "需晋六转（凡人无缘仙蛊）";
            } else if (!player.knowsIntel(kIntelFangYuanDingXianYou)) {
                // 未闻其事，则无从求借 —— 先打探情报
                v.objective = "尚未听说谁握有定仙游：需先打探情报";
            } else {
                v.objective = "需先知晓「至尊仙窍」所在：先观察或抵达该处";
            }
        }
        out.push_back(v);
    }
    return out;
}

} // namespace gr
