// ============================================================================
//  喂养（GuFeeding）—— 蛊虫平时要吃饭
//
//  原著核心设定：蛊虫不是死物，须持续喂养。
//    · 酒虫以酒水为食，一坛青竹酒约维持 4 日
//    · 月光蛊食月兰花瓣，每日早晚各 2 片；知心草可减少消耗
//    · 玉皮蛊每十日吞食二两玉石
//    · 骨竹蛊以白骨为食
//  吃不饱则功效衰减，久饿则死。
//
//  本模块把「feed 描述字符串」变成可运转的机制：
//    饱食度（fullness）随时间下降 → 影响功效 → 归零并持续则饿死
//    喂养两条途径：专食（背包物品）／真元（通用但效果较差）
//
//  时间基准：工程尚未定义「1 天 = 几世界刻度」，此处取
//    kDaysPerTick = 1（一个世界刻度按一天计）
//  若日后定了换算，改这一个常量即可，其余逻辑不受影响。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/gu/GuWorm.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 时间基准
//  工程暂未定义刻度↔日的换算，此处按「1 刻度 = 1 天」处理。
//  凡需改换算，只改此常量。
constexpr int kDaysPerTick = 1;

// ---------------------------------------------------------------- 消耗速率
//  每日饱食度下降量，依转数递增：高阶蛊食量更大。
//    drain(rank) = kDrainBase + kDrainPerRank * rank
//    一转 ≈ 0.045（约 22 日吃完）  五转 ≈ 0.105  九转 ≈ 0.165（约 6 日）
//
//  速率是权衡后的结果：若照「一坛青竹酒维持 4 日」硬套，
//  一转蛊十来天便死，玩家推进一次世界（闭关、赶路常以数十日计）
//  囊中蛊就全灭了 —— 那不是还原，是刁难。故取较缓的速率，
//  高阶蛊仍明显更快，保留「等级越高越难养」的原著体感。
constexpr double kDrainBase     = 0.03;
constexpr double kDrainPerRank  = 0.015;

inline double dailyDrain(Rank rank) {
    const int r = static_cast<int>(rank);
    return kDrainBase + kDrainPerRank * static_cast<double>(r);
}

// ---------------------------------------------------------------- 状态分档
enum class FeedState : std::uint8_t {
    Full    = 0,   // 饱食：功效无损
    Hungry,        // 饥饿：功效开始衰减
    Starving,      // 濒死：功效大幅衰减，随时可能饿死
    Dead,          // 已饿死
};

inline const char* to_string(FeedState s) {
    switch (s) {
        case FeedState::Full:     return "饱食";
        case FeedState::Hungry:   return "饥饿";
        case FeedState::Starving: return "濒死";
        case FeedState::Dead:     return "已饿死";
    }
    return "？";
}

//  饿死耐性：饱食度归零后还能撑几天。
//  取 10 天是为留出缓冲 —— 玩家不至于一次远行回来发现蛊全饿死。
constexpr int kStarveDaysBeforeDeath = 10;

FeedState feedStateOf(double fullness, int starveTicks);

// ---------------------------------------------------------------- 功效系数
//  饱食 → 1.0；饥饿线性衰减；濒死只剩三成；饿死为 0（不可用）。
//
//  这是「不喂就降功效」的落点：杀招威力、产出效率等凡用到蛊处，
//  都应乘上这个系数，而不是只看蛊虫在不在囊中。
double efficacyOf(double fullness, int starveTicks);

// ---------------------------------------------------------------- 食物解析
//  feed 是资料库的描述串，例如：
//    「月兰花瓣，每日早晚各2片；知心草可减少消耗」
//    「以酒水为食；一坛青竹酒约维持4日；浊酒、米酒也可」
//    「每十日吞食二两玉石」
//  此处抽出可能的食物名，供背包匹配。
//
//  返回按「可能是主食」排序的候选名；调用方依次在背包中查找。
std::vector<std::string> parseFeedNames(const std::string& feed);

//  从描述串里挑第一个像是食物名的片段（去数量、去量词、去说明）
std::string primaryFeedName(const std::string& feed);

// ---------------------------------------------------------------- 喂养结果
enum class FeedResult : std::uint8_t {
    Ok = 0,            // 已喂饱
    NoFood,            // 背包里没有对应食物
    AlreadyFull,       // 已是饱食，不必喂
    GuNotFound,        // 囊中无此蛊
    NotHungryEnough,   // 用真元喂时：饱食度已超过真元喂养上限
};

inline const char* to_string(FeedResult r) {
    switch (r) {
        case FeedResult::Ok:              return "已喂饱";
        case FeedResult::NoFood:          return "无此食物";
        case FeedResult::AlreadyFull:     return "已然饱食";
        case FeedResult::GuNotFound:      return "囊中无此蛊";
        case FeedResult::NotHungryEnough: return "尚不须喂";
    }
    return "？";
}

// 真元喂养只能喂到这个上限 —— 专食方为完全恢复。
//  原著里蛊师确以真元豢养蛊虫，但专食效果更佳，
//  故真元喂养上限 0.8，专食 1.0。
constexpr double kEssenceFeedCap = 0.8;

//  喂一顿真元的消耗（依蛊虫转数）
inline double essenceFeedCost(Rank rank) {
    return 1.0 + 0.5 * static_cast<double>(static_cast<int>(rank));
}

// ---------------------------------------------------------------- 推进
//  推进 days 天：扣饱食度、累计饥饿、判定饿死。
//  返回本次推进中饿死的蛊虫实例 id（供上层记入见闻录）。
struct FeedTickReport {
    std::vector<GuId> starved;              // 本次饿死
    std::vector<GuId> becameHungry;         // 本次转入饥饿
    std::vector<GuId> becameStarving;       // 本次转入濒死
};

//  skipIds：炼制中的材料蛊不扣饱食 —— 它们正处于炼化过程，
//  不该因为「没空喂」而在炉里饿死（否则长时炼制必然失败）。
FeedTickReport tickFeeding(std::vector<GuInstance>& gus,
                           const std::vector<GuTemplate>& tpls,
                           int days,
                           const std::vector<GuId>* skipIds = nullptr);

} // namespace gr
