// ============================================================================
//  十地：乐土真传所载的十种特殊地形
//
//  原著依据（《蛊真人》第五部 888 章「应声虫」，又见南疆乐土真传）：
//    「十地分别是：地渊、地沟、地道、地穴、地牢、产地、飞地、阵地、藏地、墓地」
//    「这十地都可以承担地脉节点的重任」
//
//  其中：
//    · 飞地 —— 可使蛊仙在地脉中穿梭，迅疾如飞，短时间内遍历五域
//    · 墓地 —— 成形之秘即便乐土仙尊也未探索出来；蛊仙能据此重生，
//               墓碑具信道威能，似从光阴长河中汲取蛊仙一生信息凝聚而成
//    · 方源人造地脉只能制造十地中的一半：地渊、地沟、地道、产地、阵地
//      （其余五种在他能力之外）
//
//  为什么单列：此前「地渊」只被当作中洲一处普通深壑登记，
//  「地沟」更被误当成地脉翻涌的产物 —— 实则二者各是十地之一，
//  且都可作地脉节点，这一层设定完全缺失。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

enum class TenLandKind : std::uint8_t {
    DiYuan = 0,   // 地渊
    DiGou,        // 地沟
    DiDao,        // 地道
    DiXue,        // 地穴
    DiLao,        // 地牢
    ChanDi,       // 产地
    FeiDi,        // 飞地
    ZhenDi,       // 阵地
    CangDi,       // 藏地
    MuDi,         // 墓地
};

struct TenLandInfo {
    TenLandKind kind = TenLandKind::DiYuan;
    std::string name;
    std::string desc;
    bool isVeinNode = true;          // 可否承担地脉节点（十地皆可）
    bool fangYuanCanMake = false;    // 方源人造地脉能否制造
    std::string source;
};

inline const std::vector<TenLandInfo>& tenLands() {
    static const std::vector<TenLandInfo> k = {
        {TenLandKind::DiYuan, "地渊",
         "中洲极西的地下世界，分为数十层（已探明一百零七层），"
         "每层数亿亩；溶洞甬道、地下湖泊、空旷如平原，生机盎然",
         true, true,
         "原著：地渊位于中洲极西，数十层，古魂门占据其上"},
        {TenLandKind::DiGou, "地沟",
         "大地裂开沟壑，有的绵延千万里，有的深达数百万丈，深不见底；"
         "五域皆有，乃蛊师世界最雄伟壮观的自然奇观之一",
         true, true,
         "原著 283 节「终入地沟」：并不是北原才有地沟，五域都存在着地沟"},
        {TenLandKind::DiDao, "地道",
         "十地之一，可承担地脉节点；原著未详述其形貌",
         true, true, "原著：十地之三"},
        {TenLandKind::DiXue, "地穴",
         "十地之一，可承担地脉节点；原著未详述其形貌",
         true, false, "原著：十地之四"},
        {TenLandKind::DiLao, "地牢",
         "十地之一，可承担地脉节点；原著未详述其形貌",
         true, false, "原著：十地之五"},
        {TenLandKind::ChanDi, "产地",
         "十地之一，出产资源之地，可承担地脉节点",
         true, true, "原著：十地之六"},
        {TenLandKind::FeiDi, "飞地",
         "可使蛊仙在地脉中穿梭，迅疾如飞，短时间内遍历五域",
         true, false,
         "原著：飞地可以使蛊仙在地脉中穿梭，迅疾如飞，短时间内遍历五域"},
        {TenLandKind::ZhenDi, "阵地",
         "十地之一，可承担地脉节点；原著未详述其形貌",
         true, true, "原著：十地之八"},
        {TenLandKind::CangDi, "藏地",
         "十地之一，可承担地脉节点；原著未详述其形貌",
         true, false, "原著：十地之九"},
        {TenLandKind::MuDi, "墓地",
         "成形之秘即便乐土仙尊也未探索出来；蛊仙能据此重生，"
         "墓碑具信道威能，似从光阴长河汲取蛊仙一生信息凝聚而成。"
         "疯魔窟第八层黄土大墓即运用了墓地",
         true, false,
         "原著：墓地成形之秘乐土仙尊亦未探明；疯魔窟第八层黄土大墓用墓地"},
    };
    return k;
}

inline const TenLandInfo* tenLand(TenLandKind k) {
    for (const auto& t : tenLands())
        if (t.kind == k) return &t;
    return nullptr;
}

inline const char* to_string(TenLandKind k) {
    if (const TenLandInfo* t = tenLand(k)) return t->name.c_str();
    return "？";
}

} // namespace gr
