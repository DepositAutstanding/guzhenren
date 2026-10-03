// ImGui 界面层实现 —— 纯 ImGui 调用，与渲染后端无关
#include "gr/ui/ImGuiApp.hpp"
#include "gr/core/CanonFigures.hpp"

#include "imgui.h"
#include "gr/core/CanonNumbers.hpp"
#include "gr/gu/GuFeeding.hpp"
#include "gr/world/Settlement.hpp"

#include <algorithm>
#include <cstdio>

namespace gr {
namespace gui {

namespace {

// 进度条辅助：显示 数值/上限
void bar(float frac, const ImVec4& col, const char* label) {
    frac = std::clamp(frac, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::ProgressBar(frac, ImVec2(-1, 0), label);
    ImGui::PopStyleColor();
}

// 三气条：三项并列，直观呈现「差值过大即失衡」
void qiBar(const char* name, double v, double lo, double hi, const ImVec4& col) {
    float frac = 0.0f;
    if (hi > lo) frac = static_cast<float>((v - lo) / (hi - lo));
    frac = std::clamp(frac, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %.0f", name, v);
    ImGui::ProgressBar(frac, ImVec2(-1, 0), buf);
    ImGui::PopStyleColor();
}

// 地形配色：按地貌给自然色，未探索区域压暗
ImU32 terrainColor(Terrain t, bool explored, std::uint8_t elev, float shade = 1.0f) {
    if (!explored) return IM_COL32(38, 41, 50, 255);   // 迷雾（未探明）

    // 海拔微调，制造层次感
    //  加大高程明暗（0.12 → 0.28），使山系内部的峰与谷看得出高低
    const int lift = static_cast<int>((elev - 128) * 0.28);
    //  shade 为山体阴影系数（依坡度算），叠加在高程明暗之上
    auto mix = [&](int r, int g, int b) {
        auto m = [&](int c) {
            return std::clamp(static_cast<int>((c + lift) * shade), 0, 255);
        };
        return IM_COL32(m(r), m(g), m(b), 255);
    };
    switch (t) {
        case Terrain::Void:        return IM_COL32(12, 13, 16, 255);
        case Terrain::Plain:       return mix(74, 96, 62);
        case Terrain::Mountain:    return mix(122, 110, 96);
        case Terrain::Water:       return mix(52, 92, 138);
        case Terrain::Sea:         return mix(32, 64, 104);
        case Terrain::Island:      return mix(96, 122, 78);
        case Terrain::Desert:      return mix(196, 172, 108);
        case Terrain::Oasis:       return mix(78, 148, 96);
        case Terrain::Grassland:   return mix(108, 132, 70);
        case Terrain::IceField:    return mix(196, 212, 228);
        case Terrain::ToxicForest: return mix(92, 78, 116);
        case Terrain::Wall:        return IM_COL32(180, 150, 70, 255);  // 界壁：醒目
        case Terrain::SkyRift:     return IM_COL32(150, 200, 255, 255); // 气墙裂缝
        case Terrain::Landmark:    return IM_COL32(240, 210, 90, 255);
        // —— 依地理研究新增 ——
        case Terrain::Rainforest:  return mix(52, 108, 64);
        case Terrain::LavaRock:    return mix(122, 66, 44);
        case Terrain::MistCity:    return mix(138, 140, 148);
        case Terrain::Bamboo:      return mix(96, 140, 76);
        case Terrain::Abyss:       return mix(28, 30, 44);
        case Terrain::SandDune:    return mix(214, 190, 128);
        case Terrain::Undercurrent:return mix(40, 96, 138);
        case Terrain::SpiritVein:  return mix(120, 210, 190);
        case Terrain::EarthRift:   return mix(96, 82, 72);
        // —— 细化地形图新增 ——
        case Terrain::SnowPeak:    return mix(238, 244, 252);  // 雪线以上
        case Terrain::Foothill:    return mix(146, 130, 104);  // 山麓
        case Terrain::Hill:        return mix(132, 144, 88);   // 丘陵
        case Terrain::Forest:      return mix(56, 92, 52);     // 森林
        case Terrain::Lake:        return mix(58, 108, 152);   // 湖泊
        case Terrain::Wetland:     return mix(86, 116, 92);    // 湿地
        case Terrain::Gobi:        return mix(156, 138, 108);  // 戈壁砾石
        case Terrain::Shoal:       return mix(96, 152, 180);   // 浅滩
        case Terrain::Coast:       return mix(176, 168, 128);  // 海岸
        case Terrain::Volcano:     return mix(188, 72, 44);    // 火山
        case Terrain::Sinkhole:    return mix(58, 46, 40);     // 天坑
        case Terrain::FlyingIsland:return mix(150, 132, 178);  // 飞岛
        case Terrain::Waterfall:   return mix(96, 168, 192);   // 瀑布
        case Terrain::Cave:        return mix(72, 62, 56);     // 洞窟
        case Terrain::StoneForest: return mix(136, 130, 118);  // 石林
        case Terrain::Spring:      return mix(88, 176, 184);   // 泉
    }
    return IM_COL32(60, 60, 60, 255);
}

// ---------------------------------------------------------------------------
//  山体阴影：依西北向光照，用相邻格高差算坡度明暗
//
//  光有分层设色，山地看着仍是平的；加了阴影才显出山脊与沟谷的走向。
//  这是「细化地形图」里最直接提升立体感的一步。
// ---------------------------------------------------------------------------
static float hillshadeAt(const TileMap& tm, int gx, int gy, int W, int H) {
    const int e0 = tm.at(gx, gy).elevation;
    const int ex = (gx + 1 < W) ? tm.at(gx + 1, gy).elevation : e0;
    const int ey = (gy + 1 < H) ? tm.at(gx, gy + 1).elevation : e0;
    const float dzdx = static_cast<float>(ex - e0) / 255.0f;
    const float dzdy = static_cast<float>(ey - e0) / 255.0f;
    // 法向量（高差放大以显出坡度）
    float nx = -dzdx * 9.0f, ny = -dzdy * 9.0f, nz = 1.0f;
    const float len = std::sqrt(nx * nx + ny * ny + 1.0f);
    nx /= len; ny /= len; nz /= len;
    // 光源自西北
    constexpr float lx = -0.55f, ly = -0.55f, lz = 0.63f;
    const float d = nx * lx + ny * ly + nz * lz;
    return std::clamp(0.62f + d * 0.62f, 0.35f, 1.25f);
}

// 秘境地形配色
ImU32 caveTerrainColor(CaveTerrain t, bool explored) {
    if (!explored) return IM_COL32(38, 41, 50, 255);
    switch (t) {
        case CaveTerrain::Void:        return IM_COL32(12, 13, 16, 255);
        case CaveTerrain::Floor:       return IM_COL32(88, 84, 72, 255);
        case CaveTerrain::SpiritField: return IM_COL32(96, 140, 78, 255);
        case CaveTerrain::SpiritSpring:return IM_COL32(90, 170, 200, 255);
        case CaveTerrain::Mountain:    return IM_COL32(122, 110, 96, 255);
        case CaveTerrain::Water:       return IM_COL32(52, 92, 138, 255);
        case CaveTerrain::Forest:      return IM_COL32(60, 104, 62, 255);
        case CaveTerrain::Settlement:  return IM_COL32(168, 140, 96, 255);
        case CaveTerrain::MineVein:    return IM_COL32(140, 128, 148, 255);
        case CaveTerrain::BeastNest:   return IM_COL32(148, 76, 62, 255);
        case CaveTerrain::ChaoticRift: return IM_COL32(96, 72, 128, 255);
        case CaveTerrain::Ruins:       return IM_COL32(112, 108, 100, 255);
        case CaveTerrain::CaveEntrance:return IM_COL32(120, 210, 140, 255);
        case CaveTerrain::CaveExit:    return IM_COL32(240, 120, 110, 255);
        case CaveTerrain::Landmark:    return IM_COL32(240, 210, 90, 255);
        case CaveTerrain::LavaRock:    return IM_COL32(150, 68, 44, 255);
        case CaveTerrain::MistCity:    return IM_COL32(138, 142, 152, 255);
        case CaveTerrain::Bamboo:      return IM_COL32(96, 148, 80, 255);
        case CaveTerrain::Rainforest:  return IM_COL32(52, 108, 64, 255);
        case CaveTerrain::YuanJing:    return IM_COL32(180, 150, 240, 255);
        case CaveTerrain::BookMountain:return IM_COL32(200, 186, 140, 255);
        // —— 大地裂缝 ——
        case CaveTerrain::CrackEdge:   return IM_COL32(126, 118, 104, 255);
        case CaveTerrain::CrackWall:   return IM_COL32(74, 68, 62, 255);
        case CaveTerrain::FallenDebris:return IM_COL32(146, 132, 112, 255);
        case CaveTerrain::EarthVeinRift:return IM_COL32(88, 176, 208, 255);
        case CaveTerrain::DarkDepths:  return IM_COL32(18, 16, 22, 255);
        case CaveTerrain::BlackOil:    return IM_COL32(34, 30, 26, 255);
    }
    return IM_COL32(60, 60, 60, 255);
}

const ImVec4 kColHeaven = ImVec4(0.45f, 0.72f, 1.00f, 1.0f);
const ImVec4 kColEarth  = ImVec4(0.62f, 0.85f, 0.42f, 1.0f);
const ImVec4 kColHuman  = ImVec4(1.00f, 0.76f, 0.38f, 1.0f);
const ImVec4 kColEssence= ImVec4(0.55f, 0.55f, 0.95f, 1.0f);
const ImVec4 kColHealth = ImVec4(0.90f, 0.35f, 0.35f, 1.0f);
const ImVec4 kColMark   = ImVec4(0.85f, 0.65f, 0.95f, 1.0f);

} // namespace

void App::queue(CmdKind k, const std::string& siteId, int ticks) {
    pending_.has = true;
    pending_.cmd.kind = k;
    pending_.cmd.siteId = siteId;
    pending_.cmd.ticks = ticks;
}

// ---------------------------------------------------------------------------
//  主帧
// ---------------------------------------------------------------------------
void App::buildFrame() {
    if (!session_) return;

    // 开局取名：未定名则弹出模态，挡住一切 —— 正式开始游戏之前须先定名
    if (!namingDone_) {
        drawStartNaming();
        // 其余界面仍绘制于其后（被遮住），但不响应操作
        drawMenuBar();
        return;
    }

    drawMenuBar();

    // 布局：左＝角色面板＋指令，中＝主区（地图为主页面），
    //       下＝图例与说明，右＝见闻录（默认收起，可打开）
    // 蛊虫攻击特效：按每帧时间推进（预留管线，验证可播放即可）
    effects_.update(ImGui::GetIO().DeltaTime);

    // 个人面板滑出 / 收起动画：按每帧时间推进，不依赖固定帧率
    {
        const float dt = ImGui::GetIO().DeltaTime;
        const float target = showPanel_ ? 320.0f : 0.0f;
        const float speed  = 1800.0f;            // 像素 / 秒
        if (panelSlide_ < target)      panelSlide_ = std::min(target, panelSlide_ + speed * dt);
        else if (panelSlide_ > target) panelSlide_ = std::max(target, panelSlide_ - speed * dt);
    }

    const float leftW  = panelSlide_;
    // 图例与说明只在【世界舆图】页出现 —— 查看其他信息时不该占着地方
    //
    //  图例的显示条件（需求 1：查看其他信息时不显示图例）。
    //
    //  不止「不是世界舆图页」这一种情形 —— 下面两种虽在世界舆图页，
    //  但地图已被遮住或尚未开始，图例同样不该占着地方：
    //
    //    · 炼蛊台覆盖层打开时：地图被半透明灰层压住，
    //      此时读的是炼蛊台三部分，不是地形图例 —— 图例纯属多余。
    //    · 开局取名界面（模态）未完成时：游戏尚未开始。
    //
    //  此前只判了页签，故这两种情形下图例仍显示，与需求不符。
    //
    const bool  showLegend = (tab_ == Tab::World)
                          && !(session_ && session_->bench().isOpen())
                          && namingDone_;
    const float botH   = showLegend ? 136.0f : 0.0f;
    const float rightW = showLog_ ? 380.0f : 0.0f;
    const float mainW  = dispW_ - leftW - rightW;
    // 记录主区矩形，供炼蛊台覆盖层对齐
    mainX_ = leftW; mainY_ = 22.0f;
    mainW_ = mainW; mainH_ = dispH_ - 22.0f - botH;

    // ---- 左：角色面板 + 指令栏（可关闭；关闭时宽度为 0）----
    // 高度多给几像素，让窗口底边框落到可视区之外 ——
    // 否则画面最下一行会出现一条横贯的边框线
    if (leftW > 1.0f) {
    ImGui::SetNextWindowPos(ImVec2(0, 22));
    ImGui::SetNextWindowSize(ImVec2(leftW, dispH_ - 22 + 6));
    ImGui::Begin("角色", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    // 收起按钮：关闭后顶栏出现「个人面板」字样，点击即从左侧滑出
    ImGui::SetCursorPosX(leftW - 30.0f);
    if (ImGui::Button("×", ImVec2(24, 22))) showPanel_ = false;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("收起个人面板");
    drawPlayerPanel();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("指令");
    drawCommandBar();
    ImGui::End();
    }

    // ---- 中：主区 ----
    ImGui::SetNextWindowPos(ImVec2(leftW, 22));
    ImGui::SetNextWindowSize(ImVec2(mainW, dispH_ - 22 - botH));
    ImGui::Begin("世界", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    // 炼蛊台开炉后须切到地图界面（需求：画面切到地图并盖半透明灰层）
    if (session_->bench().isOpen()) {
        const RefineSession* rs = session_->bench().cur();
        if (rs && rs->stage != RefineStage::Preparing) tab_ = Tab::World;
    }

    switch (tab_) {
        case Tab::World: drawWorldTab(); break;
        case Tab::Cave:  drawCaveTab();  break;
        case Tab::Gu:    drawGuTab();    break;
        case Tab::Dao:   drawDaoTab();   break;
        case Tab::Quest: drawQuestPanel(); break;
        case Tab::Help:  drawHelpTab();  break;
        case Tab::Realm: drawRealmTab();  break;
        case Tab::Bag:   drawBagTab();    break;
        case Tab::Npc:   drawNpcTab();    break;
        case Tab::Beast: drawBeastTab();  break;
    }
    ImGui::End();

    // 开发者面板：独立浮窗，仅 devMode 开启时出现
    if (devMode_) drawDevPanel();

    // ---- 下：图例与说明 ----
    // 只在世界舆图页出现；查看其他信息时收起，把空间让给内容
    if (showLegend) {
    // 窗口底边框延伸到画面之外，使画面最下一行不出现多余横线
    const float botTop = dispH_ - botH;
    ImGui::SetNextWindowPos(ImVec2(leftW, botTop));
    ImGui::SetNextWindowSize(ImVec2(mainW, botH + 6));
    ImGui::Begin("图例与说明", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    drawLegendBar();
    ImGui::End();
    }

    //
    //  设置窗口与炼蛊台覆盖层：画在【图例窗口 End 之后】。
    //
    //  此前这两个被写在 if (showLegend) 块内，导致只有世界舆图页
    //  才画得出来 —— 在别的页签打开设置或开炉，界面什么都不显示。
    //  它们与页签无关，本就该无条件绘制。
    //
    if (settingsOpen_) drawSettingsWindow();

    //  炼蛊台覆盖层：必须画在所有窗口 Begin/End 配对之外 ——
    //  若嵌在某窗口未 End 时再 Begin，属嵌套 Begin，
    //  ImGui 会把整个窗口丢弃，一帧下来什么都不画。
    drawRefineBench();

    // ---- 右：见闻录（可开关）----
    if (showLog_) {
        ImGui::SetNextWindowPos(ImVec2(dispW_ - rightW, 22));
        ImGui::SetNextWindowSize(ImVec2(rightW, dispH_ - 22));
        ImGui::Begin("见闻录", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        drawLogPanel();
        ImGui::End();
    }

}

// ---------------------------------------------------------------------------
//  顶栏
// ---------------------------------------------------------------------------
void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    ImGui::TextUnformatted("《蛊真人》开放世界");
    ImGui::SameLine();
    ImGui::TextDisabled("第六卷开局 · 疯魔窟大战后");

    ImGui::SameLine();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "世界刻度 %llu",
                  static_cast<unsigned long long>(session_->now()));
    ImGui::TextUnformatted(buf);

    ImGui::Separator();

    // 页签
    struct TabDef { const char* name; Tab t; };
    // 秘境页签只在身处洞天内时出现 —— 平时没有洞天可看
    const bool inRealm = session_->inCaveRealm();
    const TabDef tabs[] = {
        {"世界舆图", Tab::World}, {"洞天", Tab::Cave},
        {"蛊虫", Tab::Gu}, {"道境", Tab::Dao},
        {"人物", Tab::Npc}, {"支线", Tab::Quest}, {"背包", Tab::Bag},
        {"说明", Tab::Help},
        {"秘境", Tab::Realm},
        {"荒兽", Tab::Beast}
    };
    //
    //  遍历全部页签，按条件跳过 —— 不用 tabCount 截断。
    //
    //  原写法用 `tabCount = inRealm ? 8 : 7` 取数组前 N 个：
    //  数组顺序是 World,Cave,Gu,Dao,Npc,Quest,Bag,Help,Realm,Beast，
    //  前 8 个不含 Realm（索引 8），前 7 个连 Help（索引 7）都没有，
    //  Beast（索引 9）更是永远取不到。
    //  结果：菜单里「说明」「荒兽」点不到，进了洞天也点不到「秘境」——
    //  洞天内部地图无从查看。
    //
    for (const auto& t : tabs) {
        if (t.t == Tab::Realm && !inRealm) continue;   // 未入洞天则无秘境可看
        const bool sel = (tab_ == t.t);
        if (ImGui::MenuItem(t.name, nullptr, sel)) tab_ = t.t;
    }

    // 面板收起时顶栏出现「个人面板」字样 —— 点击即从左滑出
    if (!showPanel_) {
        ImGui::Separator();
        if (ImGui::MenuItem("个人面板")) { showPanel_ = true; panelSlide_ = 0.0f; }
    }

    ImGui::Separator();
    if (ImGui::MenuItem("改名", nullptr, renameOpen_)) renameOpen_ = !renameOpen_;
    // 更换形象：现阶段仅登记意图，图片资源尚未接入
    if (ImGui::MenuItem("更换形象", nullptr, changeSpriteOpen_))
        changeSpriteOpen_ = !changeSpriteOpen_;
    if (ImGui::MenuItem("开发者选项", nullptr, devMode_)) devMode_ = !devMode_;
    if (ImGui::MenuItem("见闻录", nullptr, showLog_)) showLog_ = !showLog_;   // 开局隐藏，需要时打开

    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
    ImGui::TextUnformatted(status_.c_str());
    ImGui::EndMainMenuBar();

    // 改名弹窗
    if (renameOpen_) {
        ImGui::OpenPopup("改名");
        ImGui::SetNextWindowSize(ImVec2(340, 150), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("改名", &renameOpen_,
                                   ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("自定名号（至多 16 字）");
            ImGui::SetNextItemWidth(-1);
            const bool enter = ImGui::InputText(
                "##newname", renameBuf_, sizeof(renameBuf_),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            if (ImGui::Button("确定", ImVec2(100, 0)) || enter) {
                queue(CmdKind::Rename);
                pending_.cmd.newName = renameBuf_;
                renameOpen_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(100, 0))) renameOpen_ = false;
            ImGui::EndPopup();
        }
    }
}

// ---------------------------------------------------------------------------
//  角色面板
// ---------------------------------------------------------------------------
void App::drawPlayerPanel() {
    PlayerView v = session_->playerView();
    if (!v.exists) {
        ImGui::TextWrapped("尚未创建玩家角色。");
        return;
    }

    ImGui::Text("%s", v.name.c_str());
    ImGui::Separator();
    ImGui::Text("修为：%s", v.rankName.c_str());
    ImGui::Text("出身：%s", v.domainName.c_str());
    ImGui::Text("身份：%s", v.demonName.c_str());
    ImGui::Spacing();

    // 能量条：凡人显示「真元」，蛊仙显示「仙元」—— 称谓随修为切换
    float ef = v.maxEssence > 0 ? static_cast<float>(v.essence / v.maxEssence) : 0.0f;
    char eb[64];
    std::snprintf(eb, sizeof(eb), "%s %.0f / %.0f",
                  v.essenceName.c_str(), v.essence, v.maxEssence);
    bar(ef, kColEssence, eb);

    // 气血
    char hb[64];
    std::snprintf(hb, sizeof(hb), "气血 %.0f%%", v.health * 100.0);
    bar(static_cast<float>(v.health), kColHealth, hb);

    // 三气：六转以上才有。凡人时期整块隐藏，不显示占位零值。
    if (v.showThreeQi) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("三气平衡");
        double mx = std::max({v.qiHeaven, v.qiEarth, v.qiHuman});
        double hi = std::max(mx * 1.15, 10.0);
        qiBar("天", v.qiHeaven, 0.0, hi, kColHeaven);
        qiBar("地", v.qiEarth,  0.0, hi, kColEarth);
        qiBar("人", v.qiHuman,  0.0, hi, kColHuman);

        const double dev = std::max({v.qiHeaven, v.qiEarth, v.qiHuman}) -
                           std::min({v.qiHeaven, v.qiEarth, v.qiHuman});
        if (v.balanced) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f),
                               "已平衡（差值 %.0f）", dev);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                               "失衡中（差值 %.0f，需 ≤ 8）", dev);
        }
    } else {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("三气平衡");
        ImGui::TextWrapped("三气乃蛊仙修行之事。凡人蛊师用的是真元，"
                           "尚无天、地、人三气之分。");
    }

    // 道痕：未过显形极点则不显示数值
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("道痕");
    if (v.daoMarksVisible) {
        char mb[80];
        std::snprintf(mb, sizeof(mb), "合计 %.0f", v.daoMarks);
        bar(static_cast<float>(std::min(v.daoMarks / 300000.0, 1.0)), kColMark, mb);
        ImGui::Text("主修：%s（%s）", v.mainDaoName.c_str(), v.mainFlowName.c_str());
        ImGui::Text("主修道痕：%.0f", v.mainDaoMarks);
    } else {
        ImGui::TextDisabled("尚未显形");
        ImGui::TextWrapped("道痕稀薄，尚不足以察觉。需累计至 %.0f 方可窥见。",
                           v.daoMarksThreshold);
        char pb[80];
        std::snprintf(pb, sizeof(pb), "%.0f / %.0f", v.daoMarks, v.daoMarksThreshold);
        bar(static_cast<float>(std::min(v.daoMarks / v.daoMarksThreshold, 1.0)),
            ImVec4(0.45f, 0.45f, 0.52f, 1.0f), pb);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("状态");
    if (v.inSeclusion) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                           "闭关中（已 %d 刻）", v.seclusionTicks);
        ImGui::TextDisabled("不可移动 / 战斗 / 外出");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "自由行动");
    }
    ImGui::Spacing();
    ImGui::Text("携带蛊虫：%zu", v.carriedGu);
    ImGui::Text("掌控福地：%zu 处", v.ownedSites);

    // 定仙游：仙蛊唯一，玩家只可能是借用
    ImGui::Spacing();
    ImGui::Separator();
    if (v.canUseDingXianYou) {
        ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f),
                           "定仙游：%s", v.dingXianYouState.c_str());
        if (v.dingXianYouTicksLeft > 0)
            ImGui::TextDisabled("剩余 %d 刻（所有权仍属方源）", v.dingXianYouTicksLeft);
        else
            ImGui::TextDisabled("所有权仍属方源");
    } else {
        ImGui::TextDisabled("定仙游：未持有");
        ImGui::TextWrapped("定仙游为仙蛊、世间唯一，归方源所有。"
                           "可向方源求借 —— 详见「支线」页签。");
    }
    ImGui::Text("已知坐标：%zu 处", v.knownCoords);

    // 成尊进度（八转以上）
    if (v.showVenerable) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "成尊四条件");
        auto mark = [](bool b, const char* t) {
            ImGui::TextColored(b ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f)
                                 : ImVec4(0.6f, 0.6f, 0.65f, 1.0f),
                               "%s %s", b ? "[√]" : "[ ]", t);
        };
        mark(v.vBaiLi, "①白荔本源");
        mark(v.vMarks, "②主修道痕 ≥30万");
        mark(v.vFlow,  "③无上大宗师");
        mark(v.vSeal,  "④突破天道封锁");
    }
}

// ---------------------------------------------------------------------------
//  世界舆图（五域示意 + 洞天分布）
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  开局取名：正式开始游戏之前进行
//
//  为什么单列这一步：游戏中改名是【取假名 / 代号】—— 本名仍在，
//  只是不对外示人，属游戏内动作。而开局取名定的是【本名】，
//  性质不同。两者混为一谈会让「代号」失去语义。
//
//  约束：不得与原著人物同名。
// ---------------------------------------------------------------------------
void App::drawStartNaming() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(dispW_, dispH_));
    ImGui::Begin("开局", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.06f, 0.08f, 1.0f));

    const float pw = 560.0f, ph = 460.0f;
    ImGui::SetCursorPos(ImVec2((dispW_ - pw) * 0.5f, (dispH_ - ph) * 0.5f));
    ImGui::BeginChild("##startbox", ImVec2(pw, ph), true);

    ImGui::TextUnformatted("《蛊真人》开放世界");
    ImGui::TextDisabled("第六卷开局 · 疯魔窟大战后");
    ImGui::Separator();

    // —— 步骤条 ——
    const char* steps[] = {"出生地", "身份", "入世", "名号"};
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        ImGui::TextColored(i == startStep_ ? ImVec4(0.45f, 0.92f, 0.55f, 1.0f)
                                           : ImVec4(0.42f, 0.45f, 0.52f, 1.0f),
                           "%d. %s", i + 1, steps[i]);
    }
    ImGui::Separator();

    if (startStep_ == 0) {
        // ================= ① 选出生地 =================
        ImGui::TextWrapped("你生于五域中的哪一域？");
        ImGui::Spacing();
        struct D { Domain d; const char* n; const char* desc; };
        const D ds[] = {
            {Domain::NanJiang,  "南疆", "十万大山连绵，瘴气遍布，部族与商家并立"},
            {Domain::DongHai,   "东海", "以海域划分势力，群岛星罗，海底潜流纵横"},
            {Domain::BeiYuan,   "北原", "苍莽草地与冰原，黄金部族把持大局"},
            {Domain::XiMo,      "西漠", "戈壁大漠与数十万里的移动沙丘，家族林立"},
            {Domain::ZhongZhou, "中洲", "元气最盛，十大古派立于此地"},
        };
        for (const auto& d : ds) {
            ImGui::PushID(static_cast<int>(d.d));
            if (ImGui::Selectable(d.n, startDomain_ == d.d)) startDomain_ = d.d;
            ImGui::SameLine(80);
            ImGui::TextDisabled("%s", d.desc);
            ImGui::PopID();
        }
    } else if (startStep_ == 1) {
        // ================= ② 选身份 =================
        ImGui::TextWrapped("%s出身的你，是何身份？", to_string(startDomain_));
        ImGui::Spacing();
        auto list = originsIn(startDomain_);
        if (startOriginIdx_ >= static_cast<int>(list.size())) startOriginIdx_ = 0;
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(list[i]->name.c_str(), i == startOriginIdx_))
                startOriginIdx_ = i;
            ImGui::SameLine(180);
            ImGui::TextDisabled("%s", list[i]->shortDesc.c_str());
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (!list.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", list[startOriginIdx_]->desc.c_str());
            ImGui::TextDisabled("溯源：%s", list[startOriginIdx_]->source.c_str());
            //
            //  降生之地 —— 此前只能选到「域」，落在哪儿由代码硬编码
            //  （一律百家寨），选了别的身份也落同一处。现列出该域的
            //  聚落供选，玩家可决定自家在哪。
            //
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("降生之地");
            {
                auto cands = session_->spawnCandidates(startDomain_);
                if (cands.empty()) {
                    ImGui::TextDisabled("  （此域暂无可降生的聚落，将由出身定夺）");
                } else {
                    if (startSpawnIdx_ >= static_cast<int>(cands.size()))
                        startSpawnIdx_ = 0;
                    const char* preview = cands[startSpawnIdx_].second.c_str();
                    if (ImGui::BeginCombo("##spawn", preview)) {
                        for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
                            const bool sel = (i == startSpawnIdx_);
                            if (ImGui::Selectable(cands[i].second.c_str(), sel))
                                startSpawnIdx_ = i;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }
            // 可选家族（如北原黄金家族择一）
            if (!list[startOriginIdx_]->optionalClans.empty()) {
                ImGui::Spacing();
                ImGui::Text("择一族属：");
                const auto& cl = list[startOriginIdx_]->optionalClans;
                if (startClanIdx_ >= static_cast<int>(cl.size())) startClanIdx_ = 0;
                for (int i = 0; i < static_cast<int>(cl.size()); ++i) {
                    ImGui::SameLine();
                    if (ImGui::RadioButton(cl[i].c_str(), &startClanIdx_, i))
                        startClan_ = cl[i];
                }
            }
        }
    } else if (startStep_ == 2) {
        // ================= ③ 开窍 / 身份确认 =================
        auto list = originsIn(startDomain_);
        const OriginDef* od = list.empty() ? nullptr : list[startOriginIdx_];
        if (od && od->awakens) {
            const AwakeningScene sc = awakeningSceneFor(od->id, "你");
            ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f), "开窍");
            ImGui::Separator();
            ImGui::Text("地点：%s", sc.where.c_str());
            ImGui::Text("仪式：%s", sc.rite.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", sc.detail.c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.45f, 0.92f, 0.55f, 1.0f), "%s", sc.result.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("溯源：%s", sc.source.c_str());
        } else if (od) {
            ImGui::Text("身份：%s", od->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", od->desc.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("此身份无开窍之仪，已是蛊师。");
        }
    } else {
        // ================= ④ 定名 =================
        auto list = originsIn(startDomain_);
        const OriginDef* od = list.empty() ? nullptr : list[startOriginIdx_];
        const bool needSurname = od && !od->requiredSurname.empty();

        ImGui::TextWrapped("在踏入此界之前，先定下你的名号。");
        if (needSurname)
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f),
                               "此身份须姓「%s」，如「%s无名」",
                               od->requiredSurname.c_str(), od->requiredSurname.c_str());
        else
            ImGui::TextDisabled("此为本名，日后行走世间另取假名，那是后话。");
        ImGui::Spacing();
        ImGui::TextDisabled("· 至多 16 字");
        ImGui::TextDisabled("· 不得与原著人物同名（方源、星宿、巨阳……）");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1);
        const bool enter = ImGui::InputText("##startname", startNameBuf_,
                                            sizeof(startNameBuf_),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (!startNameErr_.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", startNameErr_.c_str());

        ImGui::Spacing();
        if (ImGui::Button("就此入世", ImVec2(150, 0)) || enter) {
            std::string n = startNameBuf_;
            while (!n.empty() && (unsigned char)n.front() <= ' ') n.erase(n.begin());
            while (!n.empty() && (unsigned char)n.back() <= ' ')  n.pop_back();

            if (n.empty())                     startNameErr_ = "名号不可为空";
            else if (n.size() > 16)            startNameErr_ = "名号过长（至多 16 字）";
            else if (isCanonFigureName(n))     startNameErr_ = "「" + n + "」乃原著人物之名，不可冒用";
            else if (needSurname &&
                     n.substr(0, od->requiredSurname.size()) != od->requiredSurname)
                startNameErr_ = "此身份须姓「" + od->requiredSurname + "」";
            else {
                // 依出身开局：套用起始修为、人脉、蛊虫、传承线索与血脉
                startClan_ = od && !od->optionalClans.empty()
                           ? od->optionalClans[startClanIdx_] : std::string{};
                //
                //  降生之地：把所选地标传给规则层。
                //  此前该参数不存在，落点由 placePlayerAtStart 硬编码，
                //  玩家选了哪个身份都落在同一处。
                //
                std::string spawnId;
                {
                    auto cands = session_->spawnCandidates(startDomain_);
                    if (!cands.empty() &&
                        startSpawnIdx_ < static_cast<int>(cands.size()))
                        spawnId = cands[startSpawnIdx_].first;
                }
                auto r = session_->startWithOrigin(od ? od->id
                                                      : OriginId::NanJiang_Rogue,
                                                   n, startClan_, spawnId);
                if (!r.ok) { startNameErr_ = r.title; }
                else {
                    startNameErr_.clear();
                    namingDone_ = true;
                }
            }
        }
    }

    // —— 上一步 / 下一步 ——
    ImGui::Spacing();
    ImGui::Separator();
    if (startStep_ > 0) {
        if (ImGui::Button("上一步", ImVec2(110, 0))) {
            --startStep_; startNameErr_.clear();
        }
        ImGui::SameLine();
    }
    if (startStep_ < 3) {
        if (ImGui::Button("下一步", ImVec2(110, 0))) {
            if (startStep_ == 1) {
                auto list = originsIn(startDomain_);
                if (list.empty()) startNameErr_ = "此域暂无可用身份";
                else {
                    const OriginDef* od = list[startOriginIdx_];
                    startClan_ = od->optionalClans.empty()
                               ? std::string{}
                               : od->optionalClans[startClanIdx_];
                    ++startStep_;
                }
            } else ++startStep_;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}

void App::drawStartOrigin() {
    // 出身选择已并入开局流程（drawStartNaming），此函数保留供外部复用
    drawStartNaming();
}

// ---------------------------------------------------------------------------
//  底部固定栏：图例 + 地理说明
//
//  独立于页签之外，始终可见 —— 图例是读图的钥匙，不该切个页签就消失。
//  说明完整换行显示。
// ---------------------------------------------------------------------------
void App::drawLegendBar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ---------------- 图例 ----------------
    ImGui::TextDisabled("图例：");
    struct Leg { const char* n; Terrain t; };
    //  细化地形图后地类增至 26 种，图例须随之补全 ——
    //  否则新出现的雪峰、山麓、丘陵、戈壁等无从辨认。
    const Leg legs[] = {
        // 由高到低：垂直分带
        {"雪峰", Terrain::SnowPeak}, {"山川", Terrain::Mountain},
        {"山麓", Terrain::Foothill}, {"丘陵", Terrain::Hill},
        {"森林", Terrain::Forest},   {"平原", Terrain::Plain},
        // 水
        {"江河", Terrain::Water},    {"湖泊", Terrain::Lake},
        {"湿地", Terrain::Wetland},  {"海域", Terrain::Sea},
        {"浅滩", Terrain::Shoal},    {"海岸", Terrain::Coast},
        {"潜流", Terrain::Undercurrent},
        // 特殊
        {"深渊", Terrain::Abyss},    {"地沟", Terrain::EarthRift},
        {"沙漠", Terrain::Desert},   {"沙丘", Terrain::SandDune},
        {"戈壁", Terrain::Gobi},     {"绿洲", Terrain::Oasis},
        {"草原", Terrain::Grassland},{"冰原", Terrain::IceField},
        {"瘴气林", Terrain::ToxicForest}, {"雨林", Terrain::Rainforest},
        {"灵脉", Terrain::SpiritVein},
        {"火山", Terrain::Volcano},   {"天坑", Terrain::Sinkhole},
        {"飞岛", Terrain::FlyingIsland}, {"瀑布", Terrain::Waterfall},
        {"洞窟", Terrain::Cave},      {"石林", Terrain::StoneForest},
        {"泉", Terrain::Spring},
        {"界壁", Terrain::Wall},     {"气墙裂缝", Terrain::SkyRift},
    };
    for (const auto& l : legs) {
        ImGui::SameLine();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + 11, p.y + 11), terrainColor(l.t, true, 128));
        ImGui::Dummy(ImVec2(13, 11));
        ImGui::SameLine();
        ImGui::TextUnformatted(l.n);
    }

    // ---------------- 说明 ----------------
    ImGui::Spacing();
    ImGui::TextWrapped(
        "地理依据：中洲居中，东海在东、西漠在西、北原在北、南疆在南（四环围一中心）；"
        "域界为界壁带，层次越高穿越代价越大，故跨域是消耗战而非点一下即过。");
    ImGui::TextWrapped(
        "原著载明的地形（落天河、罐河、地渊、十万大山、三江、怎渡丘、"
        "四大海域、海底潜流等）均在固定地点生成，非随机；"
        "地点分四类，荡魂山与落魄谷属琅琊福地、逆流河属至尊仙窍、"
        "光阴长河属宙道域外，均不绘制于地表图。");
    ImGui::TextWrapped(
        "地形按【区域底色 × 垂直分带】生成：由低到高依次是水、平地、丘陵、"
        "山麓、山川、雪峰，故同一域内也有高低层次；"
        "明暗为山体阴影（西北向光照），细线为等高线，两者可在设置中关闭。");
}

void App::drawWorldTab() {
    const TileMap& tm = session_->tileMap();
    if (!tm.generated()) { ImGui::TextWrapped("地图尚未生成。"); return; }

    // 相机初始定位到玩家所在处
    if (!camInit_) {
        camX_ = tm.playerX() >= 0 ? static_cast<float>(tm.playerX())
                                  : tm.width() * 0.5f;
        camY_ = tm.playerY() >= 0 ? static_cast<float>(tm.playerY())
                                  : tm.height() * 0.5f;
        camInit_ = true;
    }

    // ---------------- 顶栏信息 ----------------
    ImGui::Text("五域地表舆图  %d×%d 格", tm.width(), tm.height());
    ImGui::SameLine();
    ImGui::TextDisabled("已探明 %.2f%%", tm.exploredRatio() * 100.0);
    ImGui::SameLine();
    ImGui::TextDisabled("缩放 %.1f 像素/格", zoom_);

    ImGui::SameLine(ImGui::GetWindowWidth() - 210);
    if (ImGui::Button("回到玩家", ImVec2(96, 0)) && tm.playerX() >= 0) {
        camX_ = static_cast<float>(tm.playerX());
        camY_ = static_cast<float>(tm.playerY());
    }
    ImGui::SameLine();
    if (ImGui::Button("全图揭示", ImVec2(96, 0))) tmRevealAll_ = true;

    if (targeting_) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f),
                           "催动「%s」：在地图上点选目标（右键取消）",
                           targetMoveName_.c_str());
    }

    ImGui::TextDisabled("视口中心 (%d, %d)　所属：%s　WASD 移动 · 拖拽平移 · 滚轮缩放",
                        static_cast<int>(camX_), static_cast<int>(camY_),
                        to_string(tm.domainAt(static_cast<int>(camX_),
                                              static_cast<int>(camY_))));
    ImGui::Separator();

    // ---------------- 地图画布：只绘制视口内的局部 ----------------
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 132.0f;                       // 底部留给图例与说明
    if (avail.y < 120.0f) avail.y = 120.0f;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y),
                      IM_COL32(12, 13, 16, 255));
    dl->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);

    // 视口对应的格子范围：地图已放大到 1024×768，全画既慢也看不清，
    // 只画当前视口覆盖到的那些格。
    const float halfW = avail.x * 0.5f / zoom_;
    const float halfH = avail.y * 0.5f / zoom_;
    const int gx0 = std::max(0, static_cast<int>(std::floor(camX_ - halfW)));
    const int gx1 = std::min(tm.width()  - 1, static_cast<int>(std::ceil(camX_ + halfW)));
    const int gy0 = std::max(0, static_cast<int>(std::floor(camY_ - halfH)));
    const int gy1 = std::min(tm.height() - 1, static_cast<int>(std::ceil(camY_ + halfH)));

    auto sx = [&](int gx) { return origin.x + avail.x * 0.5f + (gx - camX_) * zoom_; };
    auto sy = [&](int gy) { return origin.y + avail.y * 0.5f + (gy - camY_) * zoom_; };

    //  细化地形图：叠加山体阴影，并按需画出等高线
    const bool wantShade = showHillshade_ && zoom_ >= 3.0f;
    const bool wantContour = showContour_ && zoom_ >= 6.0f;
    constexpr int kContourStep = 12;      // 每 12 级高程一条等高线

    for (int gy = gy0; gy <= gy1; ++gy) {
        for (int gx = gx0; gx <= gx1; ++gx) {
            const Tile& t = tm.at(gx, gy);
            const float x0 = sx(gx), y0 = sy(gy);
            const float sh = wantShade ? hillshadeAt(tm, gx, gy, tm.width(), tm.height())
                                       : 1.0f;
            dl->AddRectFilled(ImVec2(x0, y0),
                              ImVec2(x0 + zoom_ + 0.6f, y0 + zoom_ + 0.6f),
                              terrainColor(t.terrain, t.explored, t.elevation, sh));
        }
    }

    // 等高线：相邻格跨过整级高程处画一条边，连起来即是等高线
    if (wantContour) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            for (int gx = gx0; gx <= gx1; ++gx) {
                const int b = tm.at(gx, gy).elevation / kContourStep;
                if (gx + 1 <= gx1) {
                    const int rb = tm.at(gx + 1, gy).elevation / kContourStep;
                    if (rb != b)
                        dl->AddLine(ImVec2(sx(gx + 1), sy(gy)),
                                    ImVec2(sx(gx + 1), sy(gy) + zoom_),
                                    IM_COL32(255, 255, 255, 46), 1.0f);
                }
                if (gy + 1 <= gy1) {
                    const int db = tm.at(gx, gy + 1).elevation / kContourStep;
                    if (db != b)
                        dl->AddLine(ImVec2(sx(gx), sy(gy + 1)),
                                    ImVec2(sx(gx) + zoom_, sy(gy + 1)),
                                    IM_COL32(255, 255, 255, 46), 1.0f);
                }
            }
        }
    }

    // 细节：放大到能分辨单格时画网格线，便于看清地形边界
    if (zoom_ >= 9.0f) {
        for (int gx = gx0; gx <= gx1 + 1; ++gx) {
            const float x0 = sx(gx);
            dl->AddLine(ImVec2(x0, sy(gy0)), ImVec2(x0, sy(gy1 + 1)),
                        IM_COL32(255, 255, 255, 18), 1.0f);
        }
        for (int gy = gy0; gy <= gy1 + 1; ++gy) {
            const float y0 = sy(gy);
            dl->AddLine(ImVec2(sx(gx0), y0), ImVec2(sx(gx1 + 1), y0),
                        IM_COL32(255, 255, 255, 18), 1.0f);
        }
    }

    // 域界描边（依分区矩形，只画视口相交部分）
    for (const auto& z : tm.zones()) {
        const float x0 = sx(z.x0), y0 = sy(z.y0);
        const float x1 = sx(z.x1), y1 = sy(z.y1);
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(190, 160, 80, 160), 0, 0, 1.5f);
        dl->AddText(ImVec2((x0 + x1) * 0.5f - 22.0f, y0 + 6.0f),
                    IM_COL32(238, 240, 248, 255), to_string(z.d));
    }

    // 地标（只画视口内的）
    for (const auto& lm : tm.landmarks()) {
        if (lm.kind != Landmark::Kind::Surface) continue;   // 非地表不画
        if (lm.x < gx0 || lm.x > gx1 || lm.y < gy0 || lm.y > gy1) continue;
        const float cx = sx(lm.x) + zoom_ * 0.5f, cy = sy(lm.y) + zoom_ * 0.5f;
        if (tm.isExplored(lm.x, lm.y)) {
            dl->AddCircleFilled(ImVec2(cx, cy), std::max(3.0f, zoom_ * 0.35f),
                                IM_COL32(250, 215, 90, 255));
            if (zoom_ >= 7.0f)
                dl->AddText(ImVec2(cx + 6.0f, cy - 6.0f), IM_COL32(250, 230, 160, 255),
                            lm.name.c_str());
        }
    }

    // 玩家位置
    //
    //  形象预留：接入图片后改走图片分支，坐标换算与缩放逻辑无需改动。
    //  现阶段 playerSpriteReady_ 恒为 false，仍以红点示位。
    if (tm.playerX() >= 0) {
        const float cx = sx(tm.playerX()) + zoom_ * 0.5f;
        const float cy = sy(tm.playerY()) + zoom_ * 0.5f;
        if (playerSpriteReady_) {
            // TODO: 以 playerSpritePath_ 的贴图绘制于 (cx, cy)，尺寸随 zoom_ 缩放
            dl->AddRectFilled(ImVec2(cx - zoom_ * 0.42f, cy - zoom_ * 0.42f),
                              ImVec2(cx + zoom_ * 0.42f, cy + zoom_ * 0.42f),
                              IM_COL32(255, 140, 140, 255));
        } else {
            dl->AddCircleFilled(ImVec2(cx, cy), std::max(3.5f, zoom_ * 0.4f),
                                IM_COL32(255, 90, 90, 255));
            dl->AddCircle(ImVec2(cx, cy), std::max(6.0f, zoom_ * 0.7f),
                          IM_COL32(255, 255, 255, 220), 0, 2.0f);
        }
    }
    // ---------------- 蛊虫攻击特效（交互预留）----------------
    //  现阶段以几何示意画出播放中的特效；接入贴图后改由 spriteReady 分支绘制。
    for (const auto& pb : effects_.active()) {
        const auto& r = pb.req;
        const float pr = pb.progress();
        const float ax = sx(r.fromX) + zoom_ * 0.5f, ay = sy(r.fromY) + zoom_ * 0.5f;
        const float bx = sx(r.toX)   + zoom_ * 0.5f, by = sy(r.toY)   + zoom_ * 0.5f;
        const int   alpha = static_cast<int>(220 * (1.0f - pr));
        if (effects_.spriteReady(r.spriteId)) {
            // TODO: 以 r.spriteId 对应贴图绘制，配合 progress() 做淡出与扩散
            continue;
        }
        switch (r.kind) {
            case GuEffectKind::Beam:
                dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by),
                            IM_COL32(255, 240, 180, alpha), 2.0f + 3.0f * (1.0f - pr));
                break;
            case GuEffectKind::Projectile: {
                const float t = pr;
                dl->AddCircleFilled(ImVec2(ax + (bx - ax) * t, ay + (by - ay) * t),
                                    3.0f + 2.0f * r.intensity,
                                    IM_COL32(255, 200, 120, alpha));
                break;
            }
            case GuEffectKind::Burst:
            case GuEffectKind::Shockwave: {
                const float rad = zoom_ * (0.3f + 1.6f * pr) * r.intensity;
                dl->AddCircle(ImVec2(bx, by), rad, IM_COL32(255, 170, 90, alpha), 0, 2.0f);
                break;
            }
            case GuEffectKind::Mist: {
                const float rad = zoom_ * (0.4f + 1.0f * pr);
                dl->AddCircleFilled(ImVec2(bx, by), rad, IM_COL32(120, 160, 110, alpha / 2));
                break;
            }
            case GuEffectKind::Aura: {
                const float rad = zoom_ * (0.5f + 0.15f * std::sin(pr * 12.0f));
                dl->AddCircle(ImVec2(ax, ay), rad, IM_COL32(150, 220, 255, alpha), 0, 2.0f);
                break;
            }
        }
    }

    dl->PopClipRect();

    // ---------------- 交互：拖拽平移 / 滚轮缩放 / 点击移动 ----------------
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("mapCanvas", avail);
    const bool hovered = ImGui::IsItemHovered();

    if (hovered) {
        // 滚轮缩放
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            const float old = zoom_;
            zoom_ = std::clamp(zoom_ * (wheel > 0 ? 1.18f : 1.0f / 1.18f), 2.0f, 48.0f);
            // 以鼠标所在格为锚点缩放，避免视野跳走
            const ImVec2 mp = ImGui::GetMousePos();
            const float ax = camX_ + (mp.x - origin.x - avail.x * 0.5f) / old;
            const float ay = camY_ + (mp.y - origin.y - avail.y * 0.5f) / old;
            camX_ = ax - (mp.x - origin.x - avail.x * 0.5f) / zoom_;
            camY_ = ay - (mp.y - origin.y - avail.y * 0.5f) / zoom_;
        }
        // 选目标模式下右键取消
        if (targeting_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            targeting_ = false;
        // 拖拽平移
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            camX_ -= d.x / zoom_;
            camY_ -= d.y / zoom_;
        }
    }

    // 悬停信息 + 点击下达移动指令
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 mp = ImGui::GetMousePos();
        const int gx = static_cast<int>(std::floor(camX_ + (mp.x - origin.x - avail.x * 0.5f) / zoom_));
        const int gy = static_cast<int>(std::floor(camY_ + (mp.y - origin.y - avail.y * 0.5f) / zoom_));
        if (tm.inBounds(gx, gy)) {
            dl->AddRect(ImVec2(sx(gx), sy(gy)),
                        ImVec2(sx(gx) + zoom_, sy(gy) + zoom_),
                        IM_COL32(255, 255, 255, 200), 0, 0, 2.0f);
            const Tile& t = tm.at(gx, gy);
            ImGui::BeginTooltip();
            if (t.explored) {
                ImGui::Text("(%d, %d) %s", gx, gy, to_string(t.terrain));
                // 原著固定地形：标明出处，与随机地形区分开
                if (const CanonTerrainZone* cz = tm.canonZoneAt(gx, gy)) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "［原著固定］");
                    ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%s", cz->name.c_str());
                    ImGui::TextWrapped("%s", cz->source.c_str());
                }
                if (t.domain != Domain::None)
                    ImGui::TextDisabled("所属：%s", to_string(t.domain));
                if (const Landmark* lm = tm.landmarkAt(gx, gy)) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", lm->name.c_str());
                    ImGui::TextWrapped("%s", lm->desc.c_str());
                    if (!lm->canon) ImGui::TextDisabled("（工程占位，非原著定点）");
                    //
                    //  依《两天五域地点总表》补：第六卷时状况与证据等级。
                    //  状况影响玩家对该地还能不能用的判断
                    //  （已毁 / 被方源吞并 / 成为战场），故值得一并给出。
                    //
                    if (lm->status != Landmark::Status::Unknown) {
                        ImGui::Separator();
                        ImGui::Text("第六卷时状况：%s", to_string(lm->status));
                        if (!lm->statusDesc.empty())
                            ImGui::TextWrapped("%s", lm->statusDesc.c_str());
                    }
                    if (!lm->scaleDesc.empty())
                        ImGui::TextDisabled("规模：%s", lm->scaleDesc.c_str());
                    if (lm->evidence != Landmark::Evidence::Unrated)
                        ImGui::TextDisabled("证据等级：%s（A＝正文明确，B＝伏笔旁证，C＝推测）",
                                            to_string(lm->evidence));
                    if (!lm->positionCanon)
                        ImGui::TextDisabled("（原文未给方位，此处为域内布点）");
                }
                ImGui::TextDisabled("点击前往");
            } else {
                ImGui::TextDisabled("未探明之地");
            }
            ImGui::EndTooltip();

            if (ImGui::IsItemClicked()) {
                if (targeting_) {
                    // 点选目标：以所点之格为落点催动杀招
                    Command c;
                    c.kind       = CmdKind::CastMove;
                    c.castMoveId = targetMoveId_;
                    c.castToX    = gx;
                    c.castToY    = gy;
                    queue(c);
                    targeting_ = false;
                } else if (t.passable()) {
                    queue(CmdKind::MoveOnMap, {}, 1);
                    pending_.cmd.mapX = gx;
                    pending_.cmd.mapY = gy;
                }
            }
        }
    }

    // ---------------- WASD 逐格移动 ----------------
    {
        int dx = 0, dy = 0;
        if (tm.playerX() >= 0 && pollWASD(dx, dy)) {
            const int nx = tm.playerX() + dx, ny = tm.playerY() + dy;
            if (tm.inBounds(nx, ny) && tm.at(nx, ny).passable()) {
                queue(CmdKind::MoveOnMap, {}, 1);
                pending_.cmd.mapX = nx;
                pending_.cmd.mapY = ny;
            }
        }
    }

    clampCamera(tm);

    // 相机跟随：走一步就把视野拉回玩家处。
    //  否则按 W 往北走，走几步人就出屏了，还得手动「回到玩家」。
    {
        const float followSpeed = ImGui::GetIO().DeltaTime * 900.0f;
        const float tx = static_cast<float>(tm.playerX());
        const float ty = static_cast<float>(tm.playerY());
        if (tm.playerX() >= 0) {
            if (camX_ < tx) camX_ = std::min(tx, camX_ + followSpeed);
            else if (camX_ > tx) camX_ = std::max(tx, camX_ - followSpeed);
            if (camY_ < ty) camY_ = std::min(ty, camY_ + followSpeed);
            else if (camY_ > ty) camY_ = std::max(ty, camY_ - followSpeed);
        }
    }
    clampCamera(tm);

    // ------------------------------------------------------------------
    //  此间聚落：站在人类群居地上时，看得到里头有什么建筑
    // ------------------------------------------------------------------
    if (const Landmark* here = tm.landmarkAt(tm.playerX(), tm.playerY())) {
        if (const Settlement* st =
                session_->world().settlements().byLandmark(here->id)) {
            ImGui::Separator();
            const std::string hdr = std::string("此间聚落：") + st->name +
                                    "（" + to_string(st->scale) + "，" +
                                    to_string(st->domain) + "）—— 建筑 " +
                                    std::to_string(st->buildings.size()) + " 处";
            if (ImGui::CollapsingHeader(hdr.c_str(),
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                if (st->ruined)
                    ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.95f, 1.0f),
                                       "【已成废墟】此地在第六卷时间线已毁，"
                                       "以下为原著所载形制，非现存建筑。");
                ImGui::TextWrapped("%s", st->desc.c_str());
                ImGui::TextDisabled("溯源：%s%s", st->source.c_str(),
                                    st->canon ? "" : "　【工程设定】");
                ImGui::Separator();

                if (ImGui::BeginTable("blds", 5,
                                      ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollY, ImVec2(-1, 190))) {
                    ImGui::TableSetupColumn("建筑", ImGuiTableColumnFlags_WidthFixed, 130);
                    ImGui::TableSetupColumn("类型", ImGuiTableColumnFlags_WidthFixed, 80);
                    ImGui::TableSetupColumn("用途", ImGuiTableColumnFlags_WidthFixed, 56);
                    ImGui::TableSetupColumn("说明", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("进入", ImGuiTableColumnFlags_WidthFixed, 64);
                    ImGui::TableHeadersRow();

                    for (const auto& b : st->buildings) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        if (b.canon)
                            ImGui::TextColored(ImVec4(0.4f,0.9f,0.5f,1.0f),
                                               "%s", b.name.c_str());
                        else
                            ImGui::TextUnformatted(b.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(to_string(b.type));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(to_string(b.use));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", b.desc.c_str());
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s%s", b.source.c_str(),
                                              b.canon ? "" : "　【工程设定】");

                        ImGui::TableSetColumnIndex(4);
                        ImGui::PushID(b.id.c_str());
                        if (ImGui::Button("进入", ImVec2(-1, 0))) {
                            Command c; c.kind = CmdKind::EnterBuilding;
                            c.settlementId = st->id;
                            c.buildingId   = b.id;
                            queue(c);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                //  商铺货架与买卖
                if (const Cultivator* p = session_->world().player()) {
                    bool hasShop = false;
                    for (const auto& b : st->buildings)
                        if (b.use == BuildingUse::Trade) hasShop = true;
                    if (hasShop) {
                        ImGui::TextDisabled(
                            "灵石 %.0f　—— 选中商铺后于此处买卖（正数买入／负数卖出）",
                            p->bag.count("灵石"));
                        static int  selItem = 0;
                        static float qty = 1.0f;
                        static const char* kItems[] = {
                            "痕石", "止血草", "兽肉", "酒水",
                            "月兰花瓣", "酸甜苦辣四味美酒", "玉石", "白骨"
                        };
                        ImGui::Combo("货品", &selItem, kItems,
                                     IM_ARRAYSIZE(kItems));
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(90);
                        ImGui::InputFloat("数量", &qty, 1.0f, 5.0f, "%.0f");
                        ImGui::SameLine();
                        if (ImGui::Button("买入", ImVec2(60, 0))) {
                            Command c; c.kind = CmdKind::EnterBuilding;
                            c.settlementId = st->id;
                            for (const auto& b : st->buildings)
                                if (b.use == BuildingUse::Trade) c.buildingId = b.id;
                            c.itemName   = kItems[selItem];
                            c.itemAmount = qty;
                            queue(c);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("卖出", ImVec2(60, 0))) {
                            Command c; c.kind = CmdKind::EnterBuilding;
                            c.settlementId = st->id;
                            for (const auto& b : st->buildings)
                                if (b.use == BuildingUse::Trade) c.buildingId = b.id;
                            c.itemName   = kItems[selItem];
                            c.itemAmount = -qty;
                            queue(c);
                        }
                    }
                }
            }
        }
    }

    // 图例与说明已移至底部固定栏（drawLegendBar），此处专注地图本身
    if (tmRevealAll_) {
        const_cast<TileMap&>(tm).revealAll();
        tmRevealAll_ = false;
    }
}

// ---------------------------------------------------------------------------
//  洞天列表
// ---------------------------------------------------------------------------
void App::drawCaveTab() {
    auto caveSites = session_->siteViews();
    const std::string targetId =
        (selectedCave_ >= 0 && selectedCave_ < static_cast<int>(caveSites.size()))
            ? caveSites[selectedCave_].siteId : std::string{};
    // 每帧只重建一次列表，避免频繁分配
    if (cachedSitesTick_ != static_cast<int>(session_->now()) || cachedSites_.empty()) {
        cachedSites_ = session_->siteViews();
        cachedSitesTick_ = static_cast<int>(session_->now());
    }
    auto& sites = cachedSites_;

    ImGui::Text("洞天福地（%zu）", sites.size());
    ImGui::SameLine();
    if (ImGui::Button("刷新")) {
        cachedSites_ = session_->siteViews();
    }
    ImGui::TextDisabled("选中一处后可对其执行移动 / 定仙游 / 观察");
    ImGui::Separator();

    // 过滤器
    static int layerFilter = 0;
    ImGui::Combo("层级筛选", &layerFilter,
                 "全部\0五域地表\0白天\0黑天\0\0");
    ImGui::SameLine();
    static bool onlyKnown = false;
    ImGui::Checkbox("仅显示已知坐标", &onlyKnown);

    ImGui::Separator();

    // 表格
    if (ImGui::BeginTable("caves", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(-1, -1))) {
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("域属", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("层级", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("分类", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("归属", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("已知", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int shown = 0;
        for (int i = 0; i < static_cast<int>(sites.size()); ++i) {
            const SiteView& s = sites[i];

            // 筛选
            if (layerFilter == 1 && s.className.find("五域") == std::string::npos) continue;
            if (layerFilter == 2 && s.layerName.find("白天") == std::string::npos) continue;
            if (layerFilter == 3 && s.layerName.find("黑天") == std::string::npos) continue;
            if (onlyKnown && !s.known && !s.isHome) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool sel = (selectedCave_ == i);
            ImGui::PushID(i);
            if (ImGui::Selectable(s.name.c_str(), sel,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                selectedCave_ = i;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(s.domainName.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(s.layerName.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(s.className.c_str());
            ImGui::TableSetColumnIndex(4);
            if (!s.ownerTag.empty())
                ImGui::Text("%s·%s", s.ownerName.c_str(), s.ownerTag.c_str());
            else
                ImGui::TextUnformatted(s.ownerName.c_str());
            ImGui::TableSetColumnIndex(5);
            if (s.isHome)      ImGui::TextColored(ImVec4(0.4f,0.9f,0.6f,1.0f), "自家");
            else if (s.known)  ImGui::TextColored(ImVec4(0.5f,0.8f,1.0f,1.0f), "是");
            else               ImGui::TextDisabled("否");
            ++shown;
        }
        ImGui::EndTable();
        ImGui::TextDisabled("显示 %d 处", shown);
    }

    // 进入洞天：内部是另一张地图（秘境）
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginDisabled(session_->inCaveRealm() || targetId.empty());
    if (ImGui::Button("进入此洞天（秘境）", ImVec2(180, 0)))
        session_->enterCave(targetId);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("进入后「秘境」页签即为其内部地图");
    if (session_->inCaveRealm()) {
        ImGui::SameLine();
        if (ImGui::Button("离开秘境", ImVec2(110, 0))) session_->leaveCave();
    }
}

// ---------------------------------------------------------------------------
//  蛊虫与蛊方
// ---------------------------------------------------------------------------
void App::drawGuTab() {
    auto recipes = session_->world().refinery().recipes();
    auto templates = session_->world().refinery().templates();

    ImGui::Text("蛊虫模板 %zu 只，蛊方 %zu 条", templates.size(), recipes.size());
    ImGui::TextDisabled("炼蛊铁律：无蛊方绝对无法炼制；残缺蛊方只出残次品");
    ImGui::Separator();

    if (ImGui::BeginTabBar("guTabs")) {
        if (ImGui::BeginTabItem("蛊方")) {
            // 溯源不再逐行列出 —— 那些是考据注记，摆在表格里既占地方
            // 也干扰查阅。已整体移入「说明」页签的「数据溯源」一节。
            if (ImGui::BeginTable("recipes", 4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY, ImVec2(-1, 0))) {
                ImGui::TableSetupColumn("目标蛊", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("转数", ImGuiTableColumnFlags_WidthFixed, 56);
                ImGui::TableSetupColumn("流派", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("完整度", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableHeadersRow();

                for (std::size_t i = 0; i < recipes.size(); ++i) {
                    const GuRecipe* r = recipes[i];
                    if (!r) continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(to_string(r->targetRank));
                    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(to_string(r->dao));

                    ImGui::TableSetColumnIndex(3);
                    switch (r->integrity) {
                        case RecipeIntegrity::Complete:
                            ImGui::TextColored(ImVec4(0.4f,0.9f,0.5f,1.0f), "完整"); break;
                        case RecipeIntegrity::Incomplete:
                            ImGui::TextColored(ImVec4(1.0f,0.7f,0.3f,1.0f), "残缺"); break;
                        default:
                            ImGui::TextColored(ImVec4(1.0f,0.4f,0.4f,1.0f), "无据推演"); break;
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        //  ———— 囊中蛊：带饱食度与喂养 ————
        //  蛊虫平时要吃饭，不喂则降功效乃至饿死。
        //  这是原著的核心设定，必须有专门一处看得见、喂得了。
        if (ImGui::BeginTabItem("囊中蛊")) {
            const Cultivator* p = session_->world().player();
            if (!p) {
                ImGui::TextDisabled("尚未创建角色");
            } else if (p->carriedGu.empty()) {
                ImGui::TextDisabled("囊中无蛊");
            } else {
                ImGui::TextDisabled("蛊虫须按时喂养：饱食则功效无损，"
                                    "饥饿则衰减，久饿至死。");
                ImGui::Separator();
                if (ImGui::BeginTable("mygu", 6,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollY, ImVec2(-1, 0))) {
                    ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 120);
                    ImGui::TableSetupColumn("转数", ImGuiTableColumnFlags_WidthFixed, 56);
                    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 64);
                    ImGui::TableSetupColumn("饱食", ImGuiTableColumnFlags_WidthFixed, 130);
                    ImGui::TableSetupColumn("功效", ImGuiTableColumnFlags_WidthFixed, 56);
                    ImGui::TableSetupColumn("喂养", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& g : p->carriedGu) {
                        const GuTemplate* t =
                            session_->world().refinery().guTemplate(g.templateId);
                        const std::string nm = t ? t->name : "？";
                        const FeedState fs = feedStateOf(g.fullness, g.starveTicks);
                        const double eff = efficacyOf(g.fullness, g.starveTicks);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(nm.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(t ? to_string(t->rank) : "？");
                        ImGui::TableSetColumnIndex(2);
                        switch (fs) {
                            case FeedState::Full:
                                ImGui::TextColored(ImVec4(0.4f,0.9f,0.5f,1.0f), "饱食"); break;
                            case FeedState::Hungry:
                                ImGui::TextColored(ImVec4(1.0f,0.75f,0.3f,1.0f), "饥饿"); break;
                            case FeedState::Starving:
                                ImGui::TextColored(ImVec4(1.0f,0.4f,0.4f,1.0f), "濒死"); break;
                            default:
                                ImGui::TextDisabled("已死"); break;
                        }
                        ImGui::TableSetColumnIndex(3);
                        ImGui::ProgressBar(static_cast<float>(g.fullness),
                                           ImVec2(-1, 0),
                                           (std::to_string((int)(g.fullness*100)) + "%").c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%d%%", (int)(eff * 100));

                        ImGui::TableSetColumnIndex(5);
                        if (fs == FeedState::Dead) {
                            ImGui::TextDisabled("—");
                        } else {
                            ImGui::PushID(static_cast<int>(g.instanceId));
                            const std::string food =
                                primaryFeedName(t ? t->feed : std::string{});
                            if (ImGui::Button("专食", ImVec2(56, 0))) {
                                Command c; c.kind = CmdKind::FeedGu;
                                c.feedGuId = g.instanceId;
                                c.feedByEssence = false;
                                queue(c);
                            }
                            if (!food.empty() && ImGui::IsItemHovered())
                                ImGui::SetTooltip("需：%s", food.c_str());
                            ImGui::SameLine();
                            if (ImGui::Button("真元", ImVec2(56, 0))) {
                                Command c; c.kind = CmdKind::FeedGu;
                                c.feedGuId = g.instanceId;
                                c.feedByEssence = true;
                                queue(c);
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("以真元豢养，只可喂至八成");
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("蛊虫")) {
            static bool onlyImmortal = false;
            ImGui::Checkbox("仅显示仙蛊（六转及以上）", &onlyImmortal);
            ImGui::TextDisabled("蓝色条目为【工程原创】，原著未记载；"
                                "各条目的资料溯源见「说明」页签");
            ImGui::Separator();

            if (ImGui::BeginTable("gus", 5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY, ImVec2(-1, 0))) {
                ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 130);
                ImGui::TableSetupColumn("转数", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("流派", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("功能", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("转数确认", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableHeadersRow();

                for (const auto& g : templates) {
                    if (onlyImmortal && !g.isImmortal()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (g.original)
                        ImGui::TextColored(ImVec4(0.6f,0.7f,1.0f,1.0f), "%s", g.name.c_str());
                    else
                        ImGui::TextUnformatted(g.name.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(to_string(g.rank));
                    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(to_string(g.dao));
                    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(g.effect.c_str());
                    ImGui::TableSetColumnIndex(4);
                    if (g.original) {
                        // 原创蛊虫：显著标注，绝不与原著条目混同
                        ImGui::TextColored(ImVec4(0.6f,0.7f,1.0f,1.0f), "工程原创");
                    } else if (g.rankConfirmed) {
                        ImGui::TextColored(ImVec4(0.4f,0.9f,0.5f,1.0f), "原著确认");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1.0f), "待考(占位)");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("自创炼蛊")) {
            drawInnovatePanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("杀招")) {
            //  催动杀招 —— 蛊虫攻击的入口。
            //  结算后由规则层给出特效请求，地图上随即播放，
            //  特效与结算同源，不会出现「催动失败却放特效」的脱节。
            const auto& moves = session_->world().killerMoves();
            ImGui::TextDisabled("共 %zu 式；绿色＝可催动，灰色＝门槛未至。"
                                "点「选目标」后到「世界舆图」点一格施放",
                                moves.size());
            ImGui::Separator();

            const Cultivator* p = session_->world().player();
            if (ImGui::BeginTable("moves", 5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY, ImVec2(-1, 0))) {
                ImGui::TableSetupColumn("杀招", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("流派", ImGuiTableColumnFlags_WidthFixed, 76);
                ImGui::TableSetupColumn("门槛", ImGuiTableColumnFlags_WidthFixed, 92);
                ImGui::TableSetupColumn("消耗", ImGuiTableColumnFlags_WidthFixed, 66);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 66);
                ImGui::TableHeadersRow();

                for (const auto& m : moves) {
                    ImGui::TableNextRow();
                    const bool rankOk = p && static_cast<int>(p->rank) >=
                                        static_cast<int>(m.requiredRank);
                    const bool daoOk  = p && static_cast<int>(p->dao.get(m.dao)) >=
                                        static_cast<int>(m.requiredDao);
                    const bool essOk  = p && p->essence >= m.baseEssence;
                    const bool canCast = rankOk && daoOk && essOk;

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(canCast ? ImVec4(0.55f,0.95f,0.6f,1.0f)
                                               : ImVec4(0.55f,0.55f,0.58f,1.0f),
                                       "%s%s", m.name.c_str(),
                                       m.canon ? "" : "（原创）");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(to_string(m.dao));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s / %s", to_string(m.requiredRank),
                                to_string(m.requiredDao));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.0f", m.baseEssence);
                    ImGui::TableSetColumnIndex(4);
                    if (!canCast) {
                        ImGui::BeginDisabled();
                        ImGui::Button("催动", ImVec2(-1, 0));
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            std::string why;
                            if (!rankOk) why += "修为不足 ";
                            if (!daoOk)  why += "道境不足 ";
                            if (!essOk)  why += (p ? p->essenceName() : "真元") + std::string("不足");
                            ImGui::SetTooltip("%s", why.c_str());
                        }
                    } else if (ImGui::Button("选目标", ImVec2(-1, 0))) {
                        // 先选目标再催动 —— 特效才看得出「从玩家飞向目标」
                        targeting_      = true;
                        targetMoveId_   = m.id;
                        targetMoveName_ = m.name;
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

// ---------------------------------------------------------------------------
//  自创炼蛊
//
//  原著基调：蛊非凭空而生，炼制前所未有之蛊极难 —— 看炼道造诣，
//  失败则炸炉、材料损毁，甚至反噬其身。故此处把「难」摆在明面上，
//  而不是让玩家一键造蛊。
//
//  数值是工程占位（原著未给公式），只保证方向正确。
// ---------------------------------------------------------------------------
void App::drawInnovatePanel() {
    const Cultivator* p = session_->world().player();
    if (!p) { ImGui::TextWrapped("尚未创建角色。"); return; }

    ImGui::TextWrapped(
        "以手中之蛊为基，推演前所未有之蛊。成败系于炼道造诣，"
        "炉毁蛊崩、反噬伤身都是常事 —— 纵然成了，也常出残次品。");
    ImGui::Spacing();

    const auto& carried = p->carriedGu;
    if (carried.size() < 2) {
        ImGui::TextDisabled("囊中蛊虫不足两只，无从推演（自创炼蛊至少需两蛊为基）");
        return;
    }

    if (innovateSel_.size() != carried.size())
        innovateSel_.assign(carried.size(), false);

    ImGui::TextDisabled("选取参与炼制的蛊虫（至少两只）：");
    auto templates = session_->world().refinery().templates();
    if (ImGui::BeginTable("innovateSel", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY, ImVec2(-1, 180))) {
        ImGui::TableSetupColumn("用", ImGuiTableColumnFlags_WidthFixed, 32);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("转数", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("流派", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < carried.size(); ++i) {
            const GuInstance& g = carried[i];
            const GuTemplate* t = nullptr;
            for (const auto& tp : templates)
                if (tp.id == g.templateId) { t = &tp; break; }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(i));
            bool sel = innovateSel_[i] != 0;
            if (ImGui::Checkbox("##sel", &sel)) innovateSel_[i] = sel ? 1 : 0;
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t ? t->name.c_str() : "？");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(t ? to_string(t->rank) : "—");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(t ? to_string(t->dao) : "—");
        }
        ImGui::EndTable();
    }

    std::size_t n = 0;
    for (char b : innovateSel_) if (b) ++n;

    ImGui::Spacing();
    ImGui::BeginDisabled(n < 2);
    if (ImGui::Button("开炉推演", ImVec2(120, 0))) {
        Command c; c.kind = CmdKind::Innovate;
        for (std::size_t i = 0; i < innovateSel_.size(); ++i)
            if (innovateSel_[i]) c.components.push_back(i);
        queue(c);
        innovateSel_.assign(innovateSel_.size(), false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("已选 %zu 只%s", n, n < 2 ? "（至少两只）" : "");

    // ---- 炼成待命名 ----
    const auto& pend = session_->pendingInnovation();
    if (pend.pending && pend.success) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "炉中新蛊初成，请为之命名");
        ImGui::TextWrapped("%s", pend.attemptDetail.c_str());
        ImGui::TextDisabled("流派：%s；推演转数：%s",
                            to_string(pend.proto.dao), to_string(pend.proto.rank));
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("蛊名", guNameBuf_, sizeof(guNameBuf_));
        ImGui::SameLine();
        if (ImGui::Button("命名归档", ImVec2(100, 0))) {
            Command c; c.kind = CmdKind::NameGu;
            c.newName = std::string(guNameBuf_);
            queue(c);
            guNameBuf_[0] = '\0';
        }
        ImGui::TextDisabled("不得与世间已有之蛊同名；命名后即存入存档");
    }

    // ---- 已自创的蛊 ----
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("自创蛊名录（%zu 只）", session_->customGu().size());
    if (session_->customGu().empty()) {
        ImGui::TextDisabled("尚无自创之蛊");
    } else {
        for (const auto& g : session_->customGu()) {
            ImGui::BulletText("%s（%s·%s）", g.name.c_str(),
                              to_string(g.rank), to_string(g.dao));
        }
        ImGui::TextDisabled("已存于 %s/custom_gu.txt", session_->saveDir().c_str());
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "须知：原著对炼制前所未有之蛊【没有】给出成功率公式。"
        "此处的数值是为让机制可运行而设的工程占位，只保证方向正确 —— "
        "难、看造诣、失败有代价，不可当作原著设定引用。");
}

// ---------------------------------------------------------------------------
//  道境与数值口径
// ---------------------------------------------------------------------------
void App::drawDaoTab() {
    using namespace canon;

    ImGui::Text("道境六阶 × 流派境界十阶");
    ImGui::TextDisabled("两套体系不可混用：道境绑定战力，流派境界绑定成尊与吞窍");
    ImGui::Separator();

    if (ImGui::BeginTable("daoFlow", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("道境（战力）", ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("流派境界（成尊）", ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("用途", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        struct Row { DaoLevel d; FlowLevel f; const char* use; };
        const Row rows[] = {
            {DaoLevel::Entry,      FlowLevel::QuasiMaster, "威力×1.0 / 消耗×1.0 —— 初入门径"},
            {DaoLevel::Small,      FlowLevel::Master,      "六转吞窍要求：大师"},
            {DaoLevel::Great,      FlowLevel::Grandmaster, "七转吞窍要求：宗师"},
            {DaoLevel::Perfection, FlowLevel::QuasiGreat,  "威力×3.1 / 反噬×0.35"},
            {DaoLevel::Foundation, FlowLevel::Great,       "八转吞窍要求：大宗师"},
            {DaoLevel::MarkFusion, FlowLevel::Supreme,     "九转成尊要求：无上大宗师"},
        };
        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(to_string(r.d));
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(to_string(r.f));
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.use);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("灾劫道痕收益（数值口径库）");
    if (ImGui::BeginTable("trib", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("灾劫", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("道痕/场", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("八转周期", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableHeadersRow();

        struct T { const char* n; TribulationKind k; const char* cyc; };
        const T ts[] = {
            {"地灾", TribulationKind::EarthCalamity,  "—"},
            {"天劫", TribulationKind::HeavenCalamity, "每 10 年"},
            {"浩劫", TribulationKind::GreatCalamity,  "每 50 年"},
            {"万劫", TribulationKind::MyriadCalamity, "每 100 年"},
        };
        for (const auto& t : ts) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(t.n);
            ImGui::TableSetColumnIndex(1);
            double g = tribulation_mark_gain(t.k);
            if (g > 0) ImGui::Text("%.0f", g);
            else       ImGui::TextDisabled("资料未给");
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(t.cyc);
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("三次万劫 ≈ 30 万道痕，正是成尊门槛的经济基础");
}

// ---------------------------------------------------------------------------
//  说明
// ---------------------------------------------------------------------------

// ===========================================================================
//  人物（NPC 互动）
//
//  界面只读规则层算好的态度与可做之事，不自行判断 ——
//  避免出现「界面显示可交易、规则却拒绝」的脱节。
//  见不到的人不列出互动按钮，但仍列出其人，并说明为何见不到。
// ===========================================================================
void App::drawNpcTab() {
    const auto cs = session_->npcContacts();
    if (cs.empty()) {
        ImGui::TextDisabled("此世空无一人。");
        return;
    }

    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                       "仙凡之别如天堑 —— 尊者行踪莫测，蛊仙视凡人如蝼蚁。");
    ImGui::TextDisabled("能否互动，先问能否见到。");

    //
    //  分两组：闻名之人（尊者蛊仙）与左近之人（凡俗众生）。
    //  凡俗之人须走到其居处方得相见 —— 既然隔空就能搭话，何须翻山越海。
    //
    std::size_t nHere = 0;
    for (const auto& cv : cs)
        if (cv.isCommoner && cv.here) ++nHere;
    ImGui::Spacing();
    ImGui::Text("左近之人：%zu 位", nHere);
    if (nHere == 0)
        ImGui::TextDisabled("此地空无一人 —— 往聚落去，方能遇上活人。");
    ImGui::Separator();

    for (const auto& cv : cs) {
        if (!cv.isCommoner || !cv.here) continue;   // 先列左近之人
        drawNpcRow(cv);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("闻名之人");
    ImGui::TextDisabled("尊者与蛊仙，名动五域；能否得见，各凭机缘。");
    ImGui::Separator();

    for (const auto& cv : cs) {
        if (cv.isCommoner) continue;
        drawNpcRow(cv);
    }
}


//  人物页单行：规则层已算好态度与可做之事，界面只照着画
void App::drawNpcRow(const gr::NpcContactView& cv) {
    const bool dim = !cv.visible;
    if (dim) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.5f, 1.0f));

    ImGui::PushID(cv.npcId.c_str());
    std::string head = cv.name + "  【" + cv.rankName + "】";
    if (cv.isCommoner && !cv.occupation.empty()) head += "  " + cv.occupation;
    head += "  " + cv.attitudeName;

    const bool open = ImGui::CollapsingHeader(head.c_str());
    if (open) {
        ImGui::Indent(12.0f);
        if (cv.isCommoner)
            ImGui::TextDisabled("居所：%s", cv.whereName.c_str());
        else
            ImGui::TextDisabled("所属：%s    此刻：%s",
                                cv.factionName.c_str(), cv.intentName.c_str());
        ImGui::TextDisabled("情谊：%d %s", cv.affinity,
                            cv.met ? "（已见过面）" : "（素未谋面）");
        ImGui::Spacing();
        ImGui::TextWrapped("「%s」", cv.line.c_str());
        ImGui::TextDisabled("%s", cv.reason.c_str());

        if (!cv.actions.empty()) {
            ImGui::Separator();
            for (NpcAction a : cv.actions) {
                if (ImGui::Button(to_string(a))) {
                    Command c;
                    c.npcId = cv.npcId;
                    switch (a) {
                        case NpcAction::Talk:   c.kind = CmdKind::NpcTalk;   break;
                        case NpcAction::Trade:  c.kind = CmdKind::NpcTrade;  break;
                        case NpcAction::Learn:  c.kind = CmdKind::NpcLearn;  break;
                        case NpcAction::Borrow: c.kind = CmdKind::NpcBorrow; break;
                        case NpcAction::Duel:   c.kind = CmdKind::NpcDuel;   break;
                        default:                c.kind = CmdKind::Idle;      break;
                    }
                    queue(c);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        if (cv.canon) ImGui::TextDisabled("溯源：%s", cv.source.c_str());
        else if (!cv.source.empty()) ImGui::TextDisabled("%s", cv.source.c_str());
        ImGui::Unindent(12.0f);
    }
    ImGui::PopID();
    if (dim) ImGui::PopStyleColor();
}

// ============================================================================
//  异兽与荒兽
//
//  原著分级：野兽 → 百兽王 → 千兽王 → 万兽王 → 兽皇
//            → 荒兽 → 上古荒兽 → 太古荒兽 → 太古传奇荒兽
//
//  「万兽王」及以下称异兽（凡人可敌）；「荒兽」属蛊仙之境，凡人不可敌。
// ============================================================================
void App::drawBeastTab() {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.6f, 1.0f), "异兽与荒兽");
    ImGui::TextDisabled("兽分九品：野兽、百兽王、千兽王、万兽王、兽皇，"
                        "荒兽、上古荒兽、太古荒兽、太古传奇。");
    ImGui::TextDisabled("万兽王及以下为异兽，凡人可敌；"
                        "荒兽已入蛊仙之境，遇之唯有退避。");
    ImGui::Separator();

    // —— 当前所在格 ——
    const gr::BeastEncounter here = session_->beastHere();
    ImGui::Text("此地兽情");
    if (here.speciesId.empty()) {
        ImGui::TextDisabled("  并无野兽出没。");
    } else {
        ImGui::Text("  %s  【%s】%s", here.name.c_str(),
                    to_string(here.rank),
                    here.count > 1 ? ("  ×" + std::to_string(here.count)).c_str() : "");
        if (here.canon) ImGui::TextDisabled("  溯源：%s", here.source.c_str());
        if (!here.fightable) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "  %s",
                               here.warning.c_str());
        } else if (ImGui::Button("狩猎")) {
            gr::Command c; c.kind = gr::CmdKind::Hunt; queue(c);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // —— 已遇名录 ——
    ImGui::Text("已遇之兽");
    const auto& seen = session_->beastsSeen();
    if (seen.empty()) {
        ImGui::TextDisabled("  尚未遭遇任何兽类。走到山野之间去罢。");
    } else {
        for (const auto& id : seen) {
            const gr::BeastSpecies* sp = gr::beast_species_by_id(id);
            if (!sp) continue;
            ImGui::TextDisabled("  %s  【%s】", sp->name.c_str(), to_string(sp->rank));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // —— 全表（按品阶分组）——
    ImGui::Text("兽谱");
    ImGui::TextDisabled("原著具名者标为可核验；凡兽多不具名，物种为工程填充。");
    const gr::BeastRank order[] = {
        gr::BeastRank::LegendaryHuang, gr::BeastRank::PrimordialHuang,
        gr::BeastRank::AncientHuang,   gr::BeastRank::Huang,
        gr::BeastRank::BeastKing,      gr::BeastRank::MyriadKing,
        gr::BeastRank::Wild,
    };
    for (gr::BeastRank rk : order) {
        std::vector<const gr::BeastSpecies*> list;
        for (const auto& s : gr::all_beast_species())
            if (s.rank == rk) list.push_back(&s);
        if (list.empty()) continue;
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s（%zu）",
                           to_string(rk), list.size());
        for (const auto* sp : list) {
            ImGui::PushID(sp->id.c_str());
            const bool open = ImGui::CollapsingHeader(sp->name.c_str());
            if (open) {
                ImGui::Indent(12.0f);
                ImGui::TextWrapped("%s", sp->desc.c_str());
                if (sp->isDaoBeast) ImGui::TextDisabled("道兽：道痕凝聚，无器官要害。");
                std::string drops;
                for (const auto& d : sp->drops) drops += (drops.empty() ? "" : "、") + d;
                if (!drops.empty()) ImGui::TextDisabled("所出：%s", drops.c_str());
                ImGui::TextDisabled("%s", sp->canon
                                    ? ("溯源：" + sp->source).c_str()
                                    : sp->source.c_str());
                ImGui::Unindent(12.0f);
            }
            ImGui::PopID();
        }
    }
}

void App::drawHelpTab() {
    ImGui::TextWrapped(
        "本界面为《蛊真人》开放世界工程的 ImGui 前端。\n\n"
        "左侧为角色状态，中间为世界舆图与各类列表，下方可下达指令，"
        "右侧见闻录记录每一次操作的结果。\n\n"
        "所有规则判定都由后台原子系统完成，界面只负责展示与下达指令，"
        "因此界面上的数字与规则实现永远一致，不会出现「界面显示可行动、"
        "实际被规则拒绝」的情况。");

    // ------------------------------------------------------------------
    //  数据溯源：蛊虫页签不再逐行罗列，整体收在此处
    // ------------------------------------------------------------------
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f), "数据溯源");
    ImGui::TextWrapped(
        "蛊虫与蛊方页签只列「是什么」，不列「从哪来」—— "
        "溯源是考据注记，摆在表格里既占地方也干扰查阅，故整体收于此处。");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("蛊方溯源")) {
        auto recipes = session_->world().refinery().recipes();
        if (ImGui::BeginTable("recipeSrc", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY, ImVec2(-1, 220))) {
            ImGui::TableSetupColumn("蛊方", ImGuiTableColumnFlags_WidthFixed, 130);
            ImGui::TableSetupColumn("溯源", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const GuRecipe* r : recipes) {
                if (!r) continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", r->source.c_str());
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("蛊虫溯源")) {
        auto templates = session_->world().refinery().templates();
        if (ImGui::BeginTable("guSrc", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY, ImVec2(-1, 260))) {
            ImGui::TableSetupColumn("蛊虫", ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("性质", ImGuiTableColumnFlags_WidthFixed, 78);
            ImGui::TableSetupColumn("溯源", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const auto& g : templates) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(g.name.c_str());
                ImGui::TableSetColumnIndex(1);
                if (g.original)
                    ImGui::TextColored(ImVec4(0.6f,0.7f,1.0f,1.0f), "工程原创");
                else if (g.rankConfirmed)
                    ImGui::TextColored(ImVec4(0.4f,0.9f,0.5f,1.0f), "原著确认");
                else
                    ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1.0f), "待考(占位)");
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", g.source.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("工程原创蛊虫的设计依据见 docs/原创蛊虫设计.md");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("操作说明");
    ImGui::BulletText("在「洞天」页签选中一处地点，再在下方指令栏点击 移动 / 定仙游 / 观察。");
    ImGui::BulletText("定仙游只能跳往已知坐标；未知地点须先「观察」记入坐标库。");
    ImGui::BulletText("六转后需定期闭关调和三气；闭关期间移动、战斗、外出均被锁死。");
    ImGui::BulletText("「推进世界」会让 NPC 自主行动，可观察世界演化。");

    ImGui::Spacing();
    ImGui::Text("键盘与鼠标");
    ImGui::BulletText("WASD 或方向键：逐格移动（按住不放连续走；洞天内同样可用）。");
    ImGui::BulletText("鼠标拖拽：平移视野。滚轮：以光标所在格为锚点缩放。");
    ImGui::BulletText("左键点地图：前往该格。右键：取消杀招的目标点选。");
    ImGui::TextDisabled("走一步视野自动跟随，不必手动「回到玩家」。");
    ImGui::TextDisabled("正在输入文字（改名、开局取名）时不响应 WASD，"
                        "按键照常打进输入框。");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), "关于渲染后端");
    ImGui::TextWrapped(
        "当前若运行在无显示器环境，界面经软件光栅化后端离屏渲染为图片；"
        "若环境具备 GLFW 与 OpenGL3，则直接打开可交互窗口。"
        "两种后端共用同一套界面代码，布局完全一致。");
}

// ---------------------------------------------------------------------------
//  指令栏
// ---------------------------------------------------------------------------
void App::drawCommandBar() {
    PlayerView v = session_->playerView();

    auto sites = session_->siteViews();
    const std::string targetId =
        (selectedCave_ >= 0 && selectedCave_ < static_cast<int>(sites.size()))
            ? sites[selectedCave_].siteId : std::string{};
    const std::string targetName =
        (selectedCave_ >= 0 && selectedCave_ < static_cast<int>(sites.size()))
            ? sites[selectedCave_].name : std::string{"（未选中）"};

    ImGui::Text("目标：%s", targetName.c_str());

    // 闭关时锁死移动类指令 —— 与规则保持一致，避免界面与判定脱节
    const bool locked = v.inSeclusion;

    ImGui::BeginDisabled(locked || targetId.empty());
    if (ImGui::Button("移动", ImVec2(90, 0))) queue(CmdKind::MoveTo, targetId);
    ImGui::SameLine();
    if (ImGui::Button("定仙游", ImVec2(90, 0))) queue(CmdKind::Jump, targetId);
    ImGui::SameLine();
    if (ImGui::Button("观察", ImVec2(90, 0))) queue(CmdKind::Observe, targetId);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(locked);
    if (ImGui::Button("搜集三气", ImVec2(110, 0))) queue(CmdKind::GatherQi, {}, 1);
    ImGui::SameLine();
    if (ImGui::Button("闭关", ImVec2(80, 0))) queue(CmdKind::EnterSeclusion);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!locked);
    if (ImGui::Button("调和×5", ImVec2(90, 0))) queue(CmdKind::Cultivate, {}, 5);
    ImGui::SameLine();
    if (ImGui::Button("出关", ImVec2(80, 0))) queue(CmdKind::LeaveSeclusion);
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("突破", ImVec2(80, 0))) queue(CmdKind::Breakthrough);

    if (locked) ImGui::TextDisabled("闭关中：移动 / 战斗 / 外出已锁死");
}

// ---------------------------------------------------------------------------
//  日志
// ---------------------------------------------------------------------------
void App::drawLogPanel() {
    const auto& log = session_->log();
    ImGui::TextDisabled("见闻录（%zu 条）", log.size());
    ImGui::Separator();

    ImGui::BeginChild("logScroll", ImVec2(-1, -1), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    // 由新到旧
    for (auto it = log.rbegin(); it != log.rend(); ++it) {
        ImVec4 col = it->success ? ImVec4(0.78f, 0.82f, 0.88f, 1.0f)
                                 : ImVec4(1.0f, 0.55f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(it->text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}


// ---------------------------------------------------------------------------
//  秘境：洞天 / 福地内部地图
// ---------------------------------------------------------------------------
void App::drawRealmTab() {
    CaveRealm* realm = session_->currentRealm();
    if (!realm) {
        ImGui::TextWrapped("尚未进入任何洞天。");
        ImGui::TextDisabled("在「洞天」页签选中一处，进入后此处即为其内部地图。");
        return;
    }

    CaveFloorMap& f = realm->floor(realm->level());

    ImGui::Text("%s", realm->name().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("已探明 %.1f%%", f.exploredRatio() * 100.0);

    // 多层结构：层数切换
    if (realm->levelCount() > 1) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 260);
        for (int lv = 1; lv <= realm->levelCount(); ++lv) {
            if (lv > 1) ImGui::SameLine();
            const bool cur = (lv == realm->level());
            if (cur) ImGui::BeginDisabled();
            if (ImGui::Button((std::to_string(lv)).c_str(), ImVec2(24, 0)))
                session_->gotoCaveLevel(lv);
            if (cur) ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("第 %d / %d 层", realm->level(), realm->levelCount());
    }
    ImGui::TextDisabled("%s", f.name.c_str());
    ImGui::TextDisabled("WASD 逐格移动 · 点击前往 · 出口返回外界");
    ImGui::Separator();

    // ---------------- 地图画布 ----------------
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 104.0f;
    if (avail.y < 100.0f) avail.y = 100.0f;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y),
                      IM_COL32(12, 13, 16, 255));

    // 秘境尺寸小（48×36 或 96×72），整层全画也不吃力，无需分块
    const float cell = std::min(avail.x / f.w, avail.y / f.h);
    const float mapW = cell * f.w, mapH = cell * f.h;
    const float offX = origin.x + (avail.x - mapW) * 0.5f;
    const float offY = origin.y + (avail.y - mapH) * 0.5f;

    for (int y = 0; y < f.h; ++y) {
        for (int x = 0; x < f.w; ++x) {
            const CaveTile& t = f.at(x, y);
            dl->AddRectFilled(ImVec2(offX + x * cell, offY + y * cell),
                              ImVec2(offX + (x + 1) * cell + 0.5f,
                                     offY + (y + 1) * cell + 0.5f),
                              caveTerrainColor(t.terrain, t.explored));
        }
    }

    // 洞天内地点
    for (const auto& s : f.sites) {
        if (!f.inBounds(s.x, s.y)) continue;
        if (!f.at(s.x, s.y).explored) continue;
        const float cx = offX + (s.x + 0.5f) * cell;
        const float cy = offY + (s.y + 0.5f) * cell;
        dl->AddCircleFilled(ImVec2(cx, cy), std::max(3.0f, cell * 0.32f),
                            IM_COL32(250, 215, 90, 255));
        if (cell >= 7.0f)
            dl->AddText(ImVec2(cx + 5.0f, cy - 6.0f),
                        IM_COL32(250, 230, 160, 255), s.name.c_str());
    }

    // 玩家位置
    {
        const float cx = offX + (realm->x() + 0.5f) * cell;
        const float cy = offY + (realm->y() + 0.5f) * cell;
        dl->AddCircleFilled(ImVec2(cx, cy), std::max(3.5f, cell * 0.4f),
                            IM_COL32(255, 90, 90, 255));
        dl->AddCircle(ImVec2(cx, cy), std::max(6.0f, cell * 0.7f),
                      IM_COL32(255, 255, 255, 220), 0, 2.0f);
    }

    // ---------------- WASD 逐格移动（秘境内部同用）----------------
    //  洞天内若只能点击走，和地表操作不一致，切来切去容易乱。
    {
        int dx = 0, dy = 0;
        if (pollWASD(dx, dy)) {
            const int nx = realm->x() + dx, ny = realm->y() + dy;
            if (f.inBounds(nx, ny) && f.at(nx, ny).passable())
                session_->moveInCave(nx, ny);
        }
    }

    // 交互
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("realmCanvas", avail);
    if (ImGui::IsItemHovered()) {
        const ImVec2 mp = ImGui::GetMousePos();
        const int gx = static_cast<int>((mp.x - offX) / cell);
        const int gy = static_cast<int>((mp.y - offY) / cell);
        if (f.inBounds(gx, gy)) {
            dl->AddRect(ImVec2(offX + gx * cell, offY + gy * cell),
                        ImVec2(offX + (gx + 1) * cell, offY + (gy + 1) * cell),
                        IM_COL32(255, 255, 255, 200), 0, 0, 2.0f);
            const CaveTile& t = f.at(gx, gy);
            ImGui::BeginTooltip();
            if (t.explored) {
                ImGui::Text("(%d, %d) %s", gx, gy, to_string(t.terrain));
                if (t.landmarkId >= 0 &&
                    static_cast<std::size_t>(t.landmarkId) < f.sites.size()) {
                    const CaveSite& s = f.sites[t.landmarkId];
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", s.name.c_str());
                    ImGui::TextWrapped("%s", s.desc.c_str());
                    ImGui::TextDisabled(s.canon ? "原著定点" : "工程构造");
                }
                ImGui::TextDisabled("点击前往");
            } else {
                ImGui::TextDisabled("未探明之处");
            }
            ImGui::EndTooltip();
            if (ImGui::IsItemClicked() && t.passable())
                session_->moveInCave(gx, gy);
        }
    }

    ImGui::Separator();

    // 图例
    ImGui::TextDisabled("图例：");
    struct CLeg { const char* n; CaveTerrain t; };
    const CLeg legs[] = {
        {"地面", CaveTerrain::Floor}, {"灵田", CaveTerrain::SpiritField},
        {"灵泉", CaveTerrain::SpiritSpring}, {"山岳", CaveTerrain::Mountain},
        {"聚落", CaveTerrain::Settlement}, {"矿脉", CaveTerrain::MineVein},
        {"兽巢", CaveTerrain::BeastNest}, {"混沌裂隙", CaveTerrain::ChaoticRift},
        {"遗迹", CaveTerrain::Ruins}, {"出口", CaveTerrain::CaveExit},
    };
    for (const auto& l : legs) {
        ImGui::SameLine();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + 11, p.y + 11), caveTerrainColor(l.t, true));
        ImGui::Dummy(ImVec2(13, 11));
        ImGui::SameLine();
        ImGui::TextUnformatted(l.n);
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "洞天自有一方天地，进来即入秘境 —— 内有山川灵脉、荒兽仙材，"
        "出口在地图下方。原著对多数洞天内部只给出少量定点，未给完整布局，"
        "故此图为【结构示意】：原著提及的地点按原著定点放置，其余地形为工程构造。");

    if (ImGui::Button("离开秘境", ImVec2(120, 0))) session_->leaveCave();
}

// ---------------------------------------------------------------------------
//  支线
// ---------------------------------------------------------------------------
void App::drawQuestPanel() {
    ImGui::Text("支线");
    ImGui::TextDisabled(
        "定仙游为仙蛊、世间唯一，归方源所有。玩家无法直接获得，"
        "只能通过支线向他求借 —— 借到的只是使用权，所有权始终在方源。");
    ImGui::Separator();

    // 炼蛊入口：需求 8 的起点 —— 点击后人物原地候炉 2 秒再切炼蛊台
    {
        auto recipes = session_->world().refinery().recipes();
        ImGui::TextDisabled("炼蛊：选定蛊方后开炉（人物原地候炉 2 秒）");
        if (!recipes.empty()) {
            static int pickIdx = 0;
            if (pickIdx >= static_cast<int>(recipes.size())) pickIdx = 0;
            ImGui::SetNextItemWidth(220);
            if (ImGui::BeginCombo("##refineRecipe",
                    recipes[pickIdx] ? recipes[pickIdx]->name.c_str() : "？")) {
                for (int i = 0; i < static_cast<int>(recipes.size()); ++i) {
                    if (!recipes[i]) continue;
                    const bool sel = (i == pickIdx);
                    if (ImGui::Selectable(recipes[i]->name.c_str(), sel)) pickIdx = i;
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("炼蛊", ImVec2(90, 0))) {
                Command c; c.kind = CmdKind::RefineOpen;
                c.refineRecipe = recipes[pickIdx]->id;
                queue(c);
            }
        }
    }
    ImGui::Separator();

    auto quests = session_->questViews();
    if (quests.empty()) {
        ImGui::TextWrapped("暂无可接支线。");
        return;
    }

    for (const auto& q : quests) {
        ImGui::PushID(static_cast<int>(q.id));

        // 状态色
        ImVec4 sc;
        switch (q.state) {
            case QuestState::Completed: sc = ImVec4(0.4f, 0.9f, 0.5f, 1.0f); break;
            case QuestState::Active:    sc = ImVec4(1.0f, 0.85f, 0.4f, 1.0f); break;
            case QuestState::Available: sc = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); break;
            default:                    sc = ImVec4(0.6f, 0.6f, 0.65f, 1.0f); break;
        }

        ImGui::TextColored(sc, "【%s】%s", to_string(q.state), q.name.c_str());
        ImGui::TextDisabled("发布者：%s", q.giver.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("%s", q.desc.c_str());
        ImGui::Spacing();
        ImGui::Text("目标：%s", q.objective.c_str());
        ImGui::Text("报酬：%s", q.reward.c_str());

        if (q.state == QuestState::Available) {
            ImGui::Spacing();
            if (ImGui::Button("接取", ImVec2(90, 0)))
                queue(CmdKind::AcceptQuest);
        } else if (q.state == QuestState::Locked) {
            ImGui::Spacing();
            // 未闻其事则无从求借：先打探情报
            ImGui::BeginDisabled(session_->playerView().rankName.find("一转") != std::string::npos ||
                                 session_->playerView().rankName.find("二转") != std::string::npos ||
                                 session_->playerView().rankName.find("三转") != std::string::npos ||
                                 session_->playerView().rankName.find("四转") != std::string::npos ||
                                 session_->playerView().rankName.find("五转") != std::string::npos);
            if (ImGui::Button("打探情报", ImVec2(110, 0))) queue(CmdKind::Inquire);
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
//  开发者面板 —— 仅展示后台信息，不修改任何状态
// ---------------------------------------------------------------------------
void App::drawDevPanel() {
    auto d = session_->devInfo();

    ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("开发者选项", &devMode_, ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.7f, 1.0f), "后台信息");
    ImGui::TextDisabled("本面板只读，不修改任何状态");
    ImGui::Separator();

    ImGui::Text("世界刻度：%llu", static_cast<unsigned long long>(d.now));
    ImGui::Separator();

    ImGui::Text("地图：%d × %d（%zu 格）", d.mapW, d.mapH, d.tiles);
    ImGui::Text("生成器：%s", d.mapGenerator.c_str());
    ImGui::Text("地标：%zu 处", d.landmarks);
    ImGui::Text("已探明：%.1f%%", d.exploredRatio * 100.0);

    ImGui::Separator();
    ImGui::Text("洞天：%zu", d.caves);
    ImGui::Text("NPC：%zu（存活 %zu）", d.npcs, d.aliveNpcs);
    ImGui::Text("仙蛊注册表：%zu", d.guRegistered);

    ImGui::Separator();
    ImGui::TextDisabled("设定口径自检");
    ImGui::Text("道痕显形极点：%.0f", gr::kDaoMarksRevealThreshold);
    ImGui::Text("真元→仙元折算：%.2f，上限 ×%.0f",
                gr::kEssenceConversionRate, gr::kEssenceMaxGrowth);

    ImGui::Separator();
    if (ImGui::Button("全图揭示", ImVec2(100, 0))) tmRevealAll_ = true;
    ImGui::SameLine();
    if (ImGui::Button("推进 10 刻", ImVec2(100, 0)))
        queue(CmdKind::Advance, {}, 10);

    ImGui::End();
}



// ---------------------------------------------------------------------------
//  相机边界钳制：地图放大后须防止视口移出世界之外
// ---------------------------------------------------------------------------
void App::queue(const Command& c) {
    // 与 queue(CmdKind...) 同一套机制：界面只负责下达，
    // 判定一律交回会话层，避免出现「界面可点、规则拒绝」的脱节
    pending_.has = true;
    pending_.cmd = c;
}

// ---------------------------------------------------------------------------
//  WASD 移动
//
//  为何要做：地图放大到 1024×768 后只画视口内的局部，
//  「点击前往」要先把视野拖到目标处才点得到，走一步得点一次，很累。
//  WASD 逐格走是这类探索地图的基本操作。
//
//  两个必须处理的细节：
//    · 正在输入文字时（改名、取名）不得抢按键 —— 否则打不出「wasd」四个字
//    · 按住不放要节流，否则一帧一格，一秒几十格，玩家看不清自己走到哪
// ---------------------------------------------------------------------------
bool App::pollWASD(int& dx, int& dy) {
    dx = 0; dy = 0;

    // 文本输入中：一律不响应，让按键正常打进输入框
    if (ImGui::GetIO().WantTextInput) { wasdRepeat_ = 0.0f; return false; }
    // 有模态弹窗时也不响应
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) { wasdRepeat_ = 0.0f; return false; }

    const bool up    = ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow);
    const bool down  = ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow);
    const bool left  = ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow);
    const bool right = ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow);
    if (!up && !down && !left && !right) { wasdRepeat_ = 0.0f; return false; }

    if (up)    dy = -1;
    if (down)  dy = +1;
    if (left)  dx = -1;
    if (right) dx = +1;
    // 斜向：一次只走一格，取先按下的主轴，避免对角穿墙
    if (dx != 0 && dy != 0) {
        if (ImGui::IsKeyPressed(ImGuiKey_W) || ImGui::IsKeyPressed(ImGuiKey_S)) dx = 0;
        else                                                                   dy = 0;
    }

    // 节流：首次立即响应，之后按住每 0.11 秒一步
    const bool firstPress = ImGui::IsKeyPressed(ImGuiKey_W, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_S, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_A, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_D, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
    if (firstPress) { wasdRepeat_ = 0.0f; return true; }

    wasdRepeat_ += ImGui::GetIO().DeltaTime;
    if (wasdRepeat_ >= 0.11f) { wasdRepeat_ = 0.0f; return true; }
    return false;
}

void App::clampCamera(const TileMap& tm) {
    camX_ = std::clamp(camX_, 0.0f, static_cast<float>(tm.width()  - 1));
    camY_ = std::clamp(camY_, 0.0f, static_cast<float>(tm.height() - 1));
}

} // namespace gui
} // namespace gr
