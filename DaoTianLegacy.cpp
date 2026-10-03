// 盗天传承实现
#include "gr/cultivator/DaoTianLegacy.hpp"
#include "gr/cultivator/Cultivator.hpp"

#include <algorithm>

namespace gr {

Result<void> DaoTianLegacySystem::obtain(Cultivator& c, DaoTianBranch branch) {
    if (branch == DaoTianBranch::None)
        return Result<void>::fail(Err::RankTooLow, "无效传承分支");
    if (!c.alive)
        return Result<void>::fail(Err::RankTooLow, "施术者已陨落，无法接受传承");

    DaoTianLegacy& L = c.daoTianLegacy;
    if (L.has(branch))
        return Result<void>::fail(Err::RankTooLow,
                                  "已掌握该分支：" + std::string(to_string(branch)));

    L.obtained = true;
    L.branches.push_back(branch);
    L.comprehension = std::max(L.comprehension, 0.1);
    return Result<void>::success("得盗天真传分支：" + std::string(to_string(branch)) +
                                 "（疯魔窟一脉）");
}

Result<void> DaoTianLegacySystem::cultivate(Cultivator& c, double effort) {
    DaoTianLegacy& L = c.daoTianLegacy;
    if (!L.any())
        return Result<void>::fail(Err::RankTooLow, "尚未获得任何盗天真传分支");

    const double gain = 0.02 * std::max(0.0, effort);
    L.comprehension = std::min(1.0, L.comprehension + gain);
    return Result<void>::success("盗天真传领悟度提升至 " +
                                 std::to_string(L.comprehension));
}

double DaoTianLegacySystem::bonusFor(const DaoTianLegacy& legacy, Dao dao) {
    if (!legacy.any()) return 1.0;
    bool benefit = false;
    switch (dao) {
        case Dao::Thief:     benefit = legacy.has(DaoTianBranch::ThiefDao);   break;
        case Dao::Transform: benefit = legacy.has(DaoTianBranch::Disguise);   break;
        case Dao::Space:     benefit = legacy.has(DaoTianBranch::ThiefDao);   break;  // 护道宇道
        case Dao::Soul:      benefit = legacy.has(DaoTianBranch::Stealth);    break;
        case Dao::Wisdom:    benefit = legacy.has(DaoTianBranch::IronMan);    break;
        case Dao::Luck:      benefit = legacy.has(DaoTianBranch::Gambling);   break;
        default:             benefit = false; break;
    }
    if (!benefit) return 1.0;
    return 1.0 + 0.5 * legacy.comprehension;   // 最高 +50%
}

bool DaoTianLegacySystem::preserveOnDeath(const Cultivator& c, DaoTianLegacy& legacy) {
    if (!c.daoTianLegacy.survivesDeath) {
        legacy = DaoTianLegacy{};
        return false;
    }
    // 需求 13：死亡轮回保留盗天传承记忆 ——蛊虫尽失，传承不失
    legacy = c.daoTianLegacy;
    return true;
}

} // namespace gr
