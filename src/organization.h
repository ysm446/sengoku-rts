#pragma once
#include <array>

// ユーザー指定の4階層。人数は編成兵力であり描画Sprite数ではない。
enum class SmallGroupState { Waiting, Advancing, Engaged, Retreating };
enum class SmallGroupRoute { None, Outward, Forward, Returning };
struct SmallGroup {
    unsigned nominalStrength = 20;
    unsigned company = 0;
    SmallGroupState state = SmallGroupState::Waiting;
    // 備の標準配置からの変位。表示人数とは独立して固定刻みで更新する。
    float offsetX = 0, offsetZ = 0;
    float targetOffsetX = 0, targetOffsetZ = 0;
    float fatigue = 0;
    SmallGroupRoute route = SmallGroupRoute::None;
    bool routeAlongX = false;
    float routeForward = 0;
};
struct Company { unsigned firstGroup = 0, groupCount = 5, troop = 0; };
struct Troop { unsigned firstCompany = 0, companyCount = 0; };
struct Organization {
    std::array<SmallGroup, 25> smallGroups{};
    std::array<Company, 5> companies{};
    std::array<Troop, 2> troops{{{0, 3}, {3, 2}}};
    Organization() {
        for (unsigned i = 0; i < smallGroups.size(); ++i) smallGroups[i].company = i / 5;
        for (unsigned i = 0; i < companies.size(); ++i) companies[i] = {i * 5, 5, i < 3 ? 0u : 1u};
    }
    static unsigned groupForSoldier(unsigned id) {
        return ((id / 71) * 5 / 71) * 5 + ((id % 71) * 5 / 71);
    }
};
