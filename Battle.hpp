// ============================================================================
//  十三、战斗体系完整版（整合优化全部）
//    1. 仙凡鸿沟完整保留
//    2. 蛊虫损毁、抢夺、献祭、道痕对抗完整闭环
//    3. 道境高低直接压制杀招强度
//    4. 地灵战斗、空间战斗、闭关调息战斗禁止全部完善
//    5. 玩家死亡轮回保留盗天传承记忆
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/cultivator/Cultivator.hpp"
#include "gr/cultivator/DaoTianLegacy.hpp"
#include "gr/gu/GuRegistry.hpp"
#include "gr/gu/KillerMove.hpp"
#include "gr/world/WorldMap.hpp"

#include <string>
#include <vector>

namespace gr {

// 战斗发生的场景类型
enum class BattleContext : std::uint8_t {
    Open          = 0,  // 常规野外战斗
    EarthSpirit   = 1,  // 地灵战斗（洞天内）
    Spatial       = 2,  // 空间战斗（两天/位面夹层）
    Seclusion     = 3,  // 闭关调息中 —— 禁止战斗
    ParadiseInterior = 4
};

inline const char* to_string(BattleContext b) {
    switch (b) {
        case BattleContext::Open:              return "常规战斗";
        case BattleContext::EarthSpirit:       return "地灵战斗";
        case BattleContext::Spatial:           return "空间战斗";
        case BattleContext::Seclusion:         return "闭关调息（禁战）";
        case BattleContext::ParadiseInterior:  return "洞天内部战斗";
    }
    return "？";
}

// 战斗结束后的蛊虫归属变动
struct GuTransfer {
    GuId        instanceId = 0;
    std::string guName;
    enum class Kind : std::uint8_t { Looted, Sacrificed, Shattered } kind = Kind::Shattered;
    std::string from, to;
};

struct BattleReport {
    bool   occurred = false;
    Err    err      = Err::Ok;
    std::string detail;

    // 杀招结算
    MoveOutcome attackerMove;
    MoveOutcome defenderMove;

    // 伤害
    double damageToDefender = 0;
    double damageToAttacker = 0;

    // 道痕对抗
    double attackerEffectiveMarks = 0;
    double defenderResistance    = 0;

    // 蛊虫变动
    std::vector<GuTransfer> guTransfers;

    bool attackerDied = false;
    bool defenderDied = false;
};

class BattleSystem {
public:
    // 4. 闭关调息战斗禁止：任一方处于闭关中，战斗不发生
    static bool isBattleAllowed(const Cultivator& a, const Cultivator& d,
                                BattleContext ctx);

    // 战斗场景判定
    static BattleContext contextOf(const Cultivator& a, const WorldMap& world);

    // 仙凡鸿沟：凡俗无法对仙蛊手段造成伤害
    static bool blockedByImmortalMortalGap(Rank attacker, Rank defender);

    // 单次交锋结算
    static BattleReport engage(Cultivator& attacker, Cultivator& defender,
                               const KillerMoveDef& atkMove,
                               const KillerMoveDef& defMove,
                               const std::vector<GuTemplate>& templates,
                               const WorldMap& world,
                               ImmortalGuRegistry& registry,
                               double rngRollAtk, double rngRollDef,
                               double rngRollLoot);

    // 蛊虫抢夺：转移注册表持有者（不清除注册表）
    static Result<void> lootGu(Cultivator& from, Cultivator& to, GuId instanceId,
                               ImmortalGuRegistry& registry,
                               const std::vector<GuTemplate>& templates);

    // 蛊虫献祭：彻底毁灭 → 释放唯一名额
    static Result<void> sacrificeGu(Cultivator& owner, GuId instanceId,
                                    ImmortalGuRegistry& registry,
                                    const std::vector<GuTemplate>& templates);

    // 至尊仙窍吞并（研究报告 2.2／7.2）：
    //   至尊仙胎体可无常规流派壁垒地吞并其他仙窍，构成
    //   「联盟—资源—洞天—至尊仙窍」的扩张链条。
    //   吞并后洞天从世界表移除（不再是无主/他人可夺的目标），
    //   但会带来内部平衡压力：万劫倒计时被压缩。
    static Result<void> swallowCave(Cultivator& owner, CaveParadise& target,
                                    WorldMap& world);

    // 定仙游迁移（需求 3.2「地灵、万劫灾劫、定仙游迁移机制完整保留」）：
    //   以定仙游将整座洞天搬迁至另一已探索坐标。
    //   约束：迁移目标必须已被亲眼见过/抵达过/感知过；迁移中不可处于闭关。
    static Result<void> migrateCaveByDingXianYou(Cultivator& mover,
                                                 CaveParadise& target,
                                                 const Location& destination,
                                                 const WorldMap& world);

    // 5. 玩家死亡轮回保留盗天传承记忆
    struct DeathResult {
        bool   reincarnated = false;
        bool   keptDaoTianLegacy = false;
        DaoTianLegacy legacy;                        // 轮回后保留的盗天传承
        std::vector<std::string> legacyBranches;     // 保留的传承分支名
        std::vector<std::string> lostGuNames;
        std::vector<std::string> knownSitesPreserved;  // 定仙游坐标是否保留
        std::string detail;
    };
    static DeathResult handlePlayerDeath(Cultivator& player,
                                         ImmortalGuRegistry& registry,
                                         const std::vector<GuTemplate>& templates,
                                         bool keepDingXianYouCoordinates = true);
};

} // namespace gr
