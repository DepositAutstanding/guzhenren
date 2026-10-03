// ---------------------------------------------------------------------------
//  原著地点总表（数据层）
//
//  源：《蛊真人两天五域地点总表》
//    截止线 —— 2019 年 5 月断更，末节为第六卷《魔尊永生》
//              第三百六十八节「方源、巨阳战星宿」。
//    口径   —— 只收原著真实出现过的地形地貌与人文聚落；
//              不采 2026 年以后所谓续更页，不纳入同人创作与读者推测。
//
//  本模块只做【数据登记】，不含任何推测性推导。
//  坐标纪律（照总表口径）：
//    · 总表写明相对方位的，按相对方位落点，positionCanon = true；
//    · 总表写「具体方位原文未明」的，在其所属域内布点，
//      positionCanon = false —— 明确它是工程落点，不冒充原著坐标。
//
//  数据由 tools/gen_canon_places.py 从总表 Markdown 自动生成，勿手改。
// ---------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include "gr/core/Types.hpp"

namespace gr {
namespace canon {

struct CanonPlace {
    std::string name;        // 名称
    std::string type;        // 类型（海域／山寨／福地……）
    std::string domainText;  // 所属域／天（原文表述）
    Domain      domain = Domain::None;

    //  坐标分三层 —— 这是总表最容易出错的地方，务必区分：
    //    五域地表 ／ 五域上方的两天 ／ 洞天福地仙窍内部
    enum class Kind : std::uint8_t {
        Surface = 0,     // 五域地表
        TwoHeavens,      // 太古白天 / 太古黑天
        InsideCave       // 洞天、福地、仙窍内部
    };
    Kind kind = Kind::Surface;

    double fx = -1.0, fy = -1.0;   // 相对坐标（仅地表条目有效）
    bool   positionCanon = false;  // 方位是否为原著所载

    std::string scaleDesc;   // 规模／占地面积（照抄原文；未给则「原文未明」）
    std::string statusDesc;  // 第六卷时状况（截至第368节）

    //  第六卷时状况
    enum class Status : std::uint8_t {
        Unknown = 0,   // 原文未明
        Intact,        // 存续
        Ruined,        // 毁坏 / 荒废
        ChangedOwner,  // 易主
        Relocated,     // 迁址
        Absorbed,      // 被方源吞并 / 搬入至尊仙窍
        Battlefield,   // 成为战场
        Declined       // 衰败 / 空置
    };
    Status status = Status::Unknown;

    //  证据等级：A＝正文明确写过；B＝有伏笔或旁证推演；
    //            C＝读者推测、同人或不可靠转述。
    //  照录总表分级，不自行升降。
    enum class Evidence : std::uint8_t { A = 0, B, C, Unrated };
    Evidence evidence = Evidence::Unrated;

    std::string posText;     // 相对位置原文
};

const std::vector<CanonPlace>& allPlaces();

//  —— 便捷查询 ——
std::vector<const CanonPlace*> placesByDomain(Domain d);
std::vector<const CanonPlace*> placesByKind(CanonPlace::Kind k);
const CanonPlace* findPlace(const std::string& name);

inline const char* to_string(CanonPlace::Kind k) {
    switch (k) {
        case CanonPlace::Kind::Surface:    return "五域地表";
        case CanonPlace::Kind::TwoHeavens: return "两天";
        case CanonPlace::Kind::InsideCave: return "洞天内";
    }
    return "—";
}

inline const char* to_string(CanonPlace::Status s) {
    switch (s) {
        case CanonPlace::Status::Unknown:      return "未明";
        case CanonPlace::Status::Intact:       return "存续";
        case CanonPlace::Status::Ruined:       return "毁坏";
        case CanonPlace::Status::ChangedOwner: return "易主";
        case CanonPlace::Status::Relocated:    return "迁址";
        case CanonPlace::Status::Absorbed:     return "被方源吞并";
        case CanonPlace::Status::Battlefield:  return "战场";
        case CanonPlace::Status::Declined:     return "衰败/空置";
    }
    return "未明";
}

inline const char* to_string(CanonPlace::Evidence e) {
    switch (e) {
        case CanonPlace::Evidence::A:       return "A";
        case CanonPlace::Evidence::B:       return "B";
        case CanonPlace::Evidence::C:       return "C";
        case CanonPlace::Evidence::Unrated: return "—";
    }
    return "—";
}

} // namespace canon
} // namespace gr
