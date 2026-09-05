#include "historical_scenario.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace history {
Scenario sekigahara() {
    using enum Action;
    // xは東、zは北。座標・経路・時刻は通説を説明するための模式値。
    // 出典と省略事項はdocs/reference/sekigahara_demo.mdに記録する。
    return {L"関ヶ原", 480, 870,
        {{L"笹尾山", -30, 25, 7, 12, 10}, {L"松尾山", -26, -31, 15, 17, 15},
         {L"南宮山", 45, -27, 19, 23, 20}, {L"桃配山", 44, 3, 7, 12, 11},
         {L"天満山", -37, -2, 9, 12, 14}, {L"岡山", 12, 27, 7, 15, 9}},
        {
         {L"石田三成", Side::West, 10000, L"笹尾山に布陣。終盤に西軍の戦線が崩れる。", {{480,-27,22,Waiting},{500,-24,19,Fighting},{785,-24,19,Retreating},{850,-48,37,Dispersed}}},
         {L"島津義弘", Side::West, 10000, L"終盤の敵中突破を南東への経路で模式化。実際の退路は省略。", {{480,-20,12,Waiting},{800,-20,12,Marching},{825,-2,6,Retreating},{870,10,-31,Dispersed}}},
         {L"小西行長", Side::West, 10000, L"中央の西軍。小早川転進後の崩れを段階的に再生。", {{480,-24,4,Waiting},{510,-17,5,Fighting},{770,-17,5,Retreating},{835,-50,10,Dispersed}}},
         {L"宇喜多秀家", Side::West, 10000, L"西軍の主力。福島隊などとの交戦を表す。", {{480,-22,-5,Waiting},{505,-13,-3,Fighting},{755,-13,-3,Retreating},{835,-49,-13,Dispersed}}},
         {L"大谷吉継", Side::West, 10000, L"松尾山方面からの転進で側面を脅かされる局面。", {{480,-30,-15,Waiting},{540,-26,-12,Fighting},{750,-26,-12,Retreating},{780,-37,-17,Dispersed}}},
         {L"小早川秀秋", Side::West, 720, L"正午ごろの転進という通説を採用。時刻・動機には異説がある。", {{480,-25,-30,Waiting},{720,-25,-30,Marching},{748,-24,-17,Fighting},{790,-17,-9,Marching},{840,-10,-4,Waiting}}},
         {L"脇坂安治ら", Side::West, 732, L"脇坂・朽木・小川・赤座を説明用に一つの軍勢へ集約。", {{480,-12,-18,Waiting},{732,-12,-18,Marching},{752,-22,-13,Fighting},{805,-23,-7,Waiting}}},
         {L"毛利秀元ら", Side::West, 10000, L"南宮山方面を集約。吉川広家らも含め、本戦へ進出しない表現。", {{480,43,-26,Waiting},{870,43,-26,Waiting}}},
         {L"徳川家康", Side::East, 10000, L"桃配山から陣場野方面への本陣前進。時刻は概略。", {{480,43,3,Waiting},{630,43,3,Marching},{670,6,9,Waiting},{870,6,9,Waiting}}},
         {L"井伊・松平", Side::East, 10000, L"井伊直政・松平忠吉の先陣を集約。終盤は島津隊の方向へ。", {{480,3,-3,Marching},{490,-7,-3,Fighting},{805,-7,-3,Marching},{835,5,-13,Waiting}}},
         {L"福島正則", Side::East, 10000, L"宇喜多隊に対する東軍の前線を表す。", {{480,5,-10,Marching},{510,-7,-8,Fighting},{765,-7,-8,Marching},{820,-26,-6,Waiting}}},
         {L"黒田・竹中", Side::East, 10000, L"北側から石田隊に対する攻勢を模式化。", {{480,0,20,Marching},{515,-18,21,Fighting},{795,-18,21,Marching},{845,-29,23,Waiting}}},
         {L"細川忠興", Side::East, 10000, L"石田隊方面の戦闘を表す。", {{480,3,12,Marching},{520,-16,15,Fighting},{800,-16,15,Marching},{850,-26,17,Waiting}}},
         {L"加藤・田中", Side::East, 10000, L"中央の東軍を集約。全武将・全備の再現ではない。", {{480,5,3,Marching},{515,-10,6,Fighting},{780,-10,6,Marching},{835,-28,7,Waiting}}}
        },
        {{480,L"布陣と開戦",L"午前8時ごろ、井伊・松平隊の先行から開戦する通説を再生。東軍は西へ進む。"},
         {510,L"中央の戦線が接触",L"宇喜多・小西・石田の各方面で交戦。軍勢の帯と向きで接する位置を示す。"},
         {630,L"家康の本陣前進",L"桃配山から陣場野方面へ。本陣前進の時刻には資料間の差がある。"},
         {720,L"小早川隊の転進",L"松尾山から大谷隊方面へ。正午ごろとする通説を採用し、転進部隊を金色で示す。"},
         {750,L"西軍南側の崩れ",L"大谷隊方面から宇喜多・小西隊へ、崩れを時間差で表現。損害計算による結果ではない。"},
         {795,L"西軍の敗走と島津の突破",L"石田隊の撤退、島津隊の敵中突破を別々の経路で示す。矢印は模式的な移動履歴。"},
         {850,L"戦いの帰結",L"東軍優勢で再生を終える。次段階では、この配置から自律計算と介入を比較する。"}}
    };
}
Pose sample(const Army& army, double minute) {
    if (army.route.empty()) return {};
    const Keyframe* a = &army.route.front();
    const Keyframe* b = a;
    for (const auto& key : army.route) {
        if (key.minute > minute) { b = &key; break; }
        a = b = &key;
    }
    const double duration = b->minute - a->minute;
    const float t = duration > 0 ? static_cast<float>(std::clamp((minute - a->minute) / duration, 0.0, 1.0)) : 0;
    const float dx = b->x - a->x, dz = b->z - a->z;
    const bool moving = dx * dx + dz * dz > 0.001f && duration > 0;
    const Side side = minute >= army.changeMinute ? Side::Changed : army.side;
    return {a->x + dx*t, a->z + dz*t, moving ? std::atan2(dz,dx) : side == Side::West ? 0.0f : 3.14159265f,
        moving && a->action == Action::Waiting ? Action::Marching : a->action, side, moving};
}
float height(const Scenario& scenario, float x, float z) {
    float value = 0;
    for (const auto& h : scenario.hills) {
        const float dx = (x-h.x)/h.radiusX, dz=(z-h.z)/h.radiusZ;
        value += h.height * std::exp(-2*(dx*dx+dz*dz));
    }
    return value;
}
std::size_t eventIndex(const Scenario& scenario, double minute) {
    std::size_t index=0;
    for (std::size_t i=0; i<scenario.events.size(); ++i) if (minute>=scenario.events[i].minute) index=i;
    return index;
}
std::wstring clockText(double minute) {
    const int total = static_cast<int>(minute);
    std::wostringstream text; text << std::setfill(L'0') << std::setw(2) << total/60 << L":" << std::setw(2) << total%60;
    return text.str();
}
const wchar_t* actionText(Action action) {
    switch(action) {
    case Action::Waiting:return L"布陣・待機"; case Action::Marching:return L"進軍";
    case Action::Fighting:return L"交戦"; case Action::Retreating:return L"撤退・突破";
    default:return L"離脱";
    }
}
void Player::seek(const Scenario& scenario, double target) {
    if (!std::isfinite(target)) return;
    minute=std::clamp(target,scenario.startMinute,scenario.endMinute);
    if (minute>=scenario.endMinute) playing=false;
}
void Player::update(const Scenario& scenario, double seconds) {
    if (playing && std::isfinite(seconds) && seconds>0 && std::isfinite(speed) && speed>0) seek(scenario,minute+seconds*speed);
}
}
