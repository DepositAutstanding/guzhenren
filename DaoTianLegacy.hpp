// ============================================================================
//  盗天传承（需求说明书 十六·一期；研究报告 4.3、Table 5）
//
//  原著事实基础：
//    · 盗天魔尊主修偷道、护道宇道（资料库仙蛊总表）
//    · 疯魔窟为其真传所在 —— 赌蛊条：「疯魔窟盗天真传中的核心蛊之一」
//    · 彭达原持有电脑蛊（钢铁侠真传），后被卷入争夺 —— 新天外之魔线与盗天传承的联结
//    · 盗天、红莲「与天外之魔、光阴长河、宿命线深度相关」（研究报告 Table 5，A/B 级）
//    · 「盗天、红莲直接出场」为 C 类推测，不作正文事实处理
//
//  与需求 13 的衔接：
//    玩家死亡轮回「保留盗天传承记忆」——传承一旦获得，跨越死亡亦不丢失。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

struct Cultivator;  // 前向声明

// 盗天传承的分支（依资料库可核验的盗天真传蛊划分）
enum class DaoTianBranch : std::uint8_t {
    None      = 0,
    ThiefDao,      // 偷道正统：大盗蛊（房家真传核心）、偷生蛊
    Stealth,       // 神不知 / 鬼不觉：防备感知的传承杀招
    Disguise,      // 态度蛊：改变他人「心中所见」
    IronMan,       // 钢铁侠真传：电脑蛊、电路蛊（机甲体系）
    Gambling       // 赌蛊：疯魔窟真传，结果完全随机
};

inline const char* to_string(DaoTianBranch b) {
    switch (b) {
        case DaoTianBranch::None:      return "未得传承";
        case DaoTianBranch::ThiefDao:  return "偷道正统";
        case DaoTianBranch::Stealth:   return "神不知鬼不觉";
        case DaoTianBranch::Disguise:  return "态度蛊·见面曾相识";
        case DaoTianBranch::IronMan:   return "钢铁侠真传（机甲）";
        case DaoTianBranch::Gambling:  return "疯魔窟赌运";
    }
    return "？";
}

// 传承进度
struct DaoTianLegacy {
    bool obtained = false;                 // 是否已获得传承（疯魔窟等地）
    std::vector<DaoTianBranch> branches;   // 已解锁的分支
    double comprehension = 0.0;            // 领悟度 0~1，影响相关杀招威力与稳定
    bool survivesDeath = true;             // 轮回不丢失（需求 13）

    bool has(DaoTianBranch b) const {
        for (auto x : branches) if (x == b) return true;
        return false;
    }
    bool any() const { return obtained && !branches.empty(); }
};

class DaoTianLegacySystem {
public:
    // 在疯魔窟等真传之地获取传承
    static Result<void> obtain(Cultivator& c, DaoTianBranch branch);

    // 精进修习：提升领悟度（消耗心力 / 时间）
    static Result<void> cultivate(Cultivator& c, double effort);

    // 对某流派杀招的增幅：偷道、变化道（态度蛊）、宇道（护道）受益
    static double bonusFor(const DaoTianLegacy& legacy, Dao dao);

    // 需求 13：死亡轮回保留盗天传承记忆
    // 返回 true 表示传承被保留（蛊虫会失去，但传承本身不失）
    static bool preserveOnDeath(const Cultivator& c, DaoTianLegacy& legacy);

    // 研究报告 4.3 限制：完整天外之魔不等于免疫一切；
    // 盗天传承同样不能免疫战斗伤害、仙蛊反噬或他人算计。
    static bool grantsImmunity() { return false; }
};

} // namespace gr
