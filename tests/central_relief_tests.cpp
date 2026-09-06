#include "simulation.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string_view>
#include <iomanip>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
unsigned slot(bool alongX, unsigned rank, unsigned lane) { return alongX ? lane * 5 + rank : rank * 5 + lane; }
BattleSimulation fixture(bool alongX, float sign, unsigned team, unsigned lane, bool gap) {
    BattleSimulation battle;
    for (unsigned t = 0; t < 2; ++t) {
        auto& f = battle.formations[t];
        const float center = (t == team ? -14.0f : 14.0f) * sign;
        f.x = f.targetX = alongX ? center : 0;
        f.z = f.targetZ = alongX ? 0 : center;
        f.morale = 100;
        for (auto& g : f.organization.smallGroups) g.morale = 100;
    }
    auto& f = battle.formations[team];
    const unsigned rank = sign > 0 ? 4 : 0, rear = sign > 0 ? 3 : 1;
    auto& front = f.organization.smallGroups[slot(alongX, rank, lane)];
    front.fatigue = 9; front.strength = 19; f.strength -= 1;
    if (gap) for (unsigned row : {rank, rear}) {
        auto& g = f.organization.smallGroups[slot(alongX, row, lane <= 2 ? lane - 1 : lane + 1)];
        f.strength -= g.strength; g.strength = 0; g.routed = true; g.routShock = 0;
    }
    battle.hold(1 - team);
    battle.running = true;
    return battle;
}
// 幾何テストと分け、攻撃・接近を有効にした最初の交代をCSVへ記録する。
// 成功率を合格条件にせず、調整前後の条件付きの観測値として扱う。
void combatReport() {
    std::cout << "along_x,sign,team,lane,gap,enemy_hold,status,start_s,ready_s,exchange_s,end_s,front_loss,formation_loss,front_routed,reserve_routed,last_phase,battle_ended\n";
    std::cout << std::fixed << std::setprecision(3);
    for (bool alongX : {false, true}) for (float sign : {-1.0f, 1.0f}) for (unsigned team : {0u, 1u})
        for (unsigned lane : {1u, 2u, 3u}) for (bool gap : {true, false}) for (bool enemyHold : {true, false}) {
            auto battle = std::make_unique<BattleSimulation>(fixture(alongX, sign, team, lane, gap));
            battle->formations[1 - team].maneuverEnabled = !enemyHold;
            auto& f = battle->formations[team];
            const unsigned front = slot(alongX, sign > 0 ? 4 : 0, lane);
            const unsigned rear = slot(alongX, sign > 0 ? 3 : 1, lane);
            const float initialFront = f.organization.smallGroups[front].strength, initialTotal = f.strength;
            double start = -1, ready = -1, exchange = -1;
            unsigned supporters = 0;
            unsigned lastPhase = 0;
            const char* status = "not_started";
            for (unsigned tick = 0; tick < 2400; ++tick) {
                lastPhase = f.organization.frontReliefs[lane + 1].phase;
                battle->update(1.0f / 60);
                const auto& r = f.organization.frontReliefs[lane + 1];
                if (start < 0 && r.front == static_cast<int>(front)) {
                    start = battle->time; supporters = r.corridorGroups; status = "pending";
                }
                if (start >= 0 && ready < 0 && r.front == static_cast<int>(front) && r.phase != 4) ready = battle->time;
                if (exchange < 0 && f.organization.smallGroups[front].slot == rear &&
                    f.organization.smallGroups[rear].slot == front) exchange = battle->time;
                if (start >= 0 && r.front != static_cast<int>(front)) {
                    status = exchange >= 0 ? "exchanged_released" : "aborted";
                    break;
                }
                if (battle->result != BattleResult::Ongoing) break;
            }
            // 予約解除は敗走でも起こるため、列が実際に戻ったかを別に判定する。
            if (exchange >= 0 && std::string_view(status) == "exchanged_released") {
                bool restored = true;
                for (unsigned id = 0; id < 25; ++id) if (supporters & (1u << id)) {
                    const auto& g = f.organization.smallGroups[id];
                    if (g.routed || g.offsetX != 0 || g.offsetZ != 0 || g.route != SmallGroupRoute::None) restored = false;
                }
                status = restored ? "restored" : "exchanged_interrupted";
            }
            std::cout << alongX << ',' << sign << ',' << team << ',' << lane << ',' << gap << ',' << enemyHold << ','
                << status << ',' << start << ',' << ready << ',' << exchange << ',' << battle->time << ','
                << initialFront - f.organization.smallGroups[front].strength << ',' << initialTotal - f.strength << ','
                << f.organization.smallGroups[front].routed << ',' << f.organization.smallGroups[rear].routed << ','
                << lastPhase << ',' << (battle->result != BattleResult::Ongoing) << '\n';
        }
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--combat-report") { combatReport(); return 0; }
        if (argc != 1) throw std::runtime_error("Usage: central_relief_tests [--combat-report]");
        for (bool alongX : {false, true}) for (float sign : {-1.0f, 1.0f}) for (unsigned team : {0u, 1u})
            for (unsigned lane : {1u, 2u, 3u}) for (bool gap : {true, false}) {
                auto battle = fixture(alongX, sign, team, lane, gap);
                const unsigned rank = sign > 0 ? 4 : 0, rearRank = sign > 0 ? 3 : 1;
                const auto front = slot(alongX, rank, lane), rear = slot(alongX, rearRank, lane);
                bool reserved = false, completed = false;
                bool exchanged = false;
                for (unsigned tick = 0; tick < 2400; ++tick) {
                    // 通路の往復を単独検証するため、敵の攻撃参加を毎刻み解除する。
                    // 敵の占有領域と衝突は維持する。被害による中断は別ケースで確認する。
                    for (auto& enemy : battle.formations[1 - team].organization.smallGroups) enemy.faceDeployment = {};
                    battle.update(1.0f / 60);
                    const auto& f = battle.formations[team];
                    reserved |= f.organization.frontReliefs[lane + 1].front == static_cast<int>(front);
                    std::array<bool, 25> slots{};
                    for (unsigned id = 0; id < 25; ++id) {
                        const auto& g = f.organization.smallGroups[id];
                        require(g.slot < 25 && !slots[g.slot] && g.company == id / 5, "Central relief changed membership or duplicated slots");
                        slots[g.slot] = true;
                        if (g.strength <= 0) continue;
                        for (unsigned other = 0; other < id; ++other) {
                            if (f.organization.smallGroups[other].strength <= 0) continue;
                            const auto a = f.groupPosition(id), b = f.groupPosition(other);
                            require(battleDistance(a, b) >= 4.5f, "Central relief crossed an occupied group");
                        }
                    }
                    if (f.organization.smallGroups[front].slot == rear && f.organization.smallGroups[rear].slot == front) {
                        if (!exchanged) require(f.organization.smallGroups[front].resting, "Relieved central front did not rest");
                        exchanged = true;
                        if (f.organization.frontReliefs[lane + 1].front < 0) {
                            for (const auto& group : f.organization.smallGroups)
                                require(group.route != SmallGroupRoute::ReliefCorridor, "Completed relief retained corridor participants");
                            completed = true; break;
                        }
                    }
                }
                if (!completed) {
                    const auto& r = battle.formations[team].organization.frontReliefs[lane + 1];
                    std::cerr << "axis=" << alongX << " sign=" << sign << " team=" << team << " lane=" << lane
                        << " gap=" << gap << " reserved=" << reserved << " phase=" << r.phase << " front=" << r.front << '\n';
                    const auto& g = battle.formations[team].organization.smallGroups[front];
                    std::cerr << "exchanged=" << exchanged << " routed=" << g.routed << " strength=" << g.strength << " morale=" << g.morale << '\n';
                }
                require(reserved && completed, "Central relief did not complete through an open neighboring lane");
            }
        auto blocked = fixture(false, 1, 0, 2, false);
        for (unsigned id = 0; id < 25; ++id) if (id % 5 != 2) blocked.formations[0].organization.smallGroups[id].resting = true;
        blocked.update(.1f);
        require(blocked.formations[0].organization.frontReliefs[3].front < 0 &&
            blocked.formations[0].organization.smallGroups[22].offsetZ == 0, "Blocked central front started withdrawing");
        for (unsigned id : {16u, 21u}) {
            auto& g = blocked.formations[0].organization.smallGroups[id];
            blocked.formations[0].strength -= g.strength;
            g.strength = 0; g.routed = true; g.routShock = 0;
        }
        blocked.update(1.0f / 60);
        require(blocked.formations[0].organization.frontReliefs[3].front == 22, "Central relief did not start when corridor opened");
        auto open = fixture(false, 1, 0, 2, true);
        open.update(.1f);
        const auto before = open.formations[0].groupPosition(17);
        open.hold(0); open.update(1);
        require(battleDistance(before, open.formations[0].groupPosition(17)) == 0, "Hold moved central reserve");
        open.reset();
        for (const auto& relief : open.formations[0].organization.frontReliefs) require(relief.front < 0, "Reset retained central relief");
        auto preparing = std::make_unique<BattleSimulation>(fixture(false, 1, 0, 2, false));
        preparing->update(.1f);
        auto& prepared = preparing->formations[0].organization;
        require(prepared.frontReliefs[3].phase == 4 && prepared.frontReliefs[3].corridorGroups != 0,
            "Dense formation did not reserve corridor preparation");
        require(prepared.smallGroups[22].offsetZ == 0 && prepared.smallGroups[17].offsetX == 0,
            "Central pair moved before corridor was ready");
        const auto supporters = prepared.frontReliefs[3].corridorGroups;
        const float heldOffset = prepared.smallGroups[0].offsetX;
        preparing->running = false; preparing->update(1);
        require(prepared.smallGroups[0].offsetX == heldOffset, "Pause moved corridor helpers");
        preparing->running = true; preparing->hold(0); preparing->update(.1f);
        require(prepared.smallGroups[0].offsetX == heldOffset, "Hold moved corridor helpers");
        preparing->move(0, 0, 0);
        prepared.smallGroups[22].morale = 10;
        preparing->update(1.0f / 60);
        require(prepared.frontReliefs[3].front < 0, "Rout retained a corridor reservation");
        for (unsigned id = 0; id < 25; ++id) if (supporters & (1u << id))
            require(prepared.smallGroups[id].route == SmallGroupRoute::Returning || prepared.smallGroups[id].routed ||
                (prepared.smallGroups[id].route == SmallGroupRoute::None && prepared.smallGroups[id].offsetX == 0 && prepared.smallGroups[id].offsetZ == 0),
                "Rout did not release corridor helpers");
        const auto slow = std::make_unique<BattleSimulation>(fixture(false, 1, 0, 2, false));
        const auto fast = std::make_unique<BattleSimulation>(*slow);
        slow->update(12);
        for (unsigned tick = 0; tick < 720; ++tick) fast->update(1.0f / 60);
        for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
            const auto& a = slow->formations[team].organization.smallGroups[id];
            const auto& b = fast->formations[team].organization.smallGroups[id];
            require(a.slot == b.slot && a.route == b.route && a.resting == b.resting && a.strength == b.strength &&
                a.morale == b.morale && a.offsetX == b.offsetX && a.offsetZ == b.offsetZ, "Central relief depends on update frequency");
        }
        std::cout << "Central relief paths, mirrored teams, collision, hold and reset passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
