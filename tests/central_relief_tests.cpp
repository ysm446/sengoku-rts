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
        for(bool axis:{false,true})for(float sign:{-1.0f,1.0f})for(unsigned team:{0u,1u}) {
            auto battle=std::make_unique<BattleSimulation>(fixture(axis,sign,team,2,false));
            auto& f=battle->formations[team];const unsigned front=slot(axis,sign>0?4:0,2),reserve=slot(axis,2,2);
            for(unsigned id=0;id<25;++id)if(id!=front && id!=reserve) {
                f.organization.smallGroups[id].offsetZ=1000;f.organization.smallGroups[id].fatigue=5;
            }
            auto& a=f.organization.smallGroups[front];auto& b=f.organization.smallGroups[reserve];
            const auto head=f.groupPosition(front),rear=f.groupPosition(reserve);
            auto archer=std::make_unique<BattleSimulation>(*battle);
            archer->formations[team].organization.smallGroups[reserve].unit=UnitType::Archer;archer->update(1.0f/60);
            require(archer->formations[team].organization.frontReliefs[3].front<0,"Archer was taken as an alternative melee reserve");
            auto blocked=std::make_unique<BattleSimulation>(*battle);
            const unsigned obstacle=slot(axis,sign>0?3:1,2);
            auto& blocker=blocked->formations[team].organization.smallGroups[obstacle];
            blocker.offsetZ=0;blocker.fatigue=5;
            blocked->update(1.0f/60);
            require(blocked->formations[team].organization.frontReliefs[3].front<0,"Alternative exchange ignored an occupied withdrawal path");
            const auto company=a.company;bool completed=false;
            for(unsigned tick=0;tick<2400;++tick) {
                const auto p=f.groupPosition(front),q=f.groupPosition(reserve);
                for(auto& g:battle->formations[1-team].organization.smallGroups)g.faceDeployment={};
                battle->update(1.0f/60);
                if(tick==0) {
                    require(f.organization.frontReliefs[3].reserve==static_cast<int>(reserve),"Alternative reserve was not selected");
                    auto held=std::make_unique<BattleSimulation>(*battle);held->hold(team);held->update(.1f);
                    require(battleDistance(held->formations[team].groupPosition(reserve),f.groupPosition(reserve))==0,"Hold moved alternative reserve");
                    auto paused=std::make_unique<BattleSimulation>(*battle);paused->running=false;paused->update(1);
                    require(battleDistance(paused->formations[team].groupPosition(front),f.groupPosition(front))==0,"Pause moved alternative front");
                    auto interrupted=std::make_unique<BattleSimulation>(*battle);
                    interrupted->formations[team].organization.smallGroups[reserve].morale=10;interrupted->update(1.0f/60);
                    require(interrupted->formations[team].organization.frontReliefs[3].front<0,"Alternative reserve rout retained reservation");
                }
                require(battleDistance(p,f.groupPosition(front))<=f.speed/60+.001f && battleDistance(q,f.groupPosition(reserve))<=f.speed/60+.001f,
                    "Alternative exchange teleported");
                for(unsigned id:{front,reserve})for(unsigned t=0;t<2;++t)for(unsigned other=0;other<25;++other) {
                    if(t==team && id==other)continue;
                    require(battleDistance(f.groupPosition(id),battle->formations[t].groupPosition(other))>=4.5f,"Alternative exchange crossed occupancy");
                }
                if(a.slot==reserve && b.slot==front) {
                    require(a.resting && a.company==company && battleDistance(f.groupPosition(front),rear)<.001f &&
                        battleDistance(f.groupPosition(reserve),head)<.001f,"Alternative exchange lost position, identity or rest");
                    completed=true;break;
                }
            }
            require(completed,"Alternative reserve exchange did not complete");
        }
        for(bool axis:{false,true})for(float sign:{-1.0f,1.0f})for(unsigned team:{0u,1u})for(bool gap:{true,false}) {
            auto moved=std::make_unique<BattleSimulation>(fixture(axis,sign,team,2,gap));
            const unsigned front=slot(axis,sign>0?4:0,2),rear=slot(axis,sign>0?3:1,2);
            auto& f=moved->formations[team];auto& a=f.organization.smallGroups[front];auto& b=f.organization.smallGroups[rear];
            if(axis){a.approachX=sign*1.5f;b.approachX=sign*.5f;}else{a.approachZ=sign*1.5f;b.approachZ=sign*.5f;}
            if(!gap)for(unsigned id=0;id<25;++id)if(id!=front && id!=rear) {
                auto& h=f.organization.smallGroups[id];
                if(axis){h.approachX=sign*.3f;h.approachZ=.15f;}else{h.approachZ=sign*.3f;h.approachX=.15f;}
            }
            std::array<BattlePoint,25> original{};
            for(unsigned id=0;id<25;++id)original[id]=f.groupPosition(id);
            const auto head=f.groupPosition(front),tail=f.groupPosition(rear);
            bool started=false,completed=false,exchanged=false;unsigned helpers=0;
            for(unsigned tick=0;tick<3600;++tick) {
                std::array<BattlePoint,25> previous{};
                for(unsigned id=0;id<25;++id)previous[id]=f.groupPosition(id);
                const auto previousHead=f.groupPosition(front),previousRear=f.groupPosition(rear);
                for(auto& enemy:moved->formations[1-team].organization.smallGroups)enemy.faceDeployment={};
                moved->update(1.0f/60);
                started|=f.organization.frontReliefs[3].displaced;
                if(tick==0) {
                    require(started,"Displaced relief did not reserve its open path immediately");
                    helpers=f.organization.frontReliefs[3].corridorGroups;
                    if(!gap)require(helpers!=0 && f.organization.frontReliefs[3].phase==4,"Displaced helpers did not prepare corridor");
                    auto paused=std::make_unique<BattleSimulation>(*moved);paused->running=false;paused->update(1);
                    require(battleDistance(paused->formations[team].groupPosition(front),f.groupPosition(front))==0,"Pause moved displaced front");
                    auto held=std::make_unique<BattleSimulation>(*moved);held->hold(team);held->update(.2f);
                    require(battleDistance(held->formations[team].groupPosition(rear),f.groupPosition(rear))==0,"Hold moved displaced reserve");
                    auto interrupted=std::make_unique<BattleSimulation>(*moved);
                    interrupted->formations[team].organization.smallGroups[rear].morale=10;interrupted->update(1.0f/60);
                    require(interrupted->formations[team].organization.frontReliefs[3].front<0 &&
                        interrupted->formations[team].organization.smallGroups[rear].routed,"Reserve rout retained displaced reservation");
                    auto whole=std::make_unique<BattleSimulation>(*moved),split=std::make_unique<BattleSimulation>(*moved);
                    whole->update(1);for(unsigned sub=0;sub<60;++sub)split->update(1.0f/60);
                    require(battleDistance(whole->formations[team].groupPosition(front),split->formations[team].groupPosition(front))<.001f &&
                        battleDistance(whole->formations[team].groupPosition(rear),split->formations[team].groupPosition(rear))<.001f &&
                        whole->formations[team].organization.frontReliefs[3].phase==split->formations[team].organization.frontReliefs[3].phase,
                        "Displaced relief depends on update cadence");
                }
                for(unsigned id=0;id<25;++id)if(helpers&(1u<<id)) {
                    require(battleDistance(previous[id],f.groupPosition(id))<=f.speed/60+.001f,"Corridor helper teleported");
                    for(unsigned t=0;t<2;++t)for(unsigned other=0;other<25;++other) {
                        if((t==team && id==other) || moved->formations[t].organization.smallGroups[other].strength<=0)continue;
                        require(battleDistance(f.groupPosition(id),moved->formations[t].groupPosition(other))>=4.5f,"Displaced helper crossed occupancy");
                    }
                }
                require(battleDistance(previousHead,f.groupPosition(front))<=f.speed/60+.001f &&
                    battleDistance(previousRear,f.groupPosition(rear))<=f.speed/60+.001f,"Displaced relief teleported a group");
                for(unsigned id:{front,rear})for(unsigned t=0;t<2;++t)for(unsigned other=0;other<25;++other) {
                    if((t==team && other==id) || moved->formations[t].organization.smallGroups[other].strength<=0)continue;
                    require(battleDistance(f.groupPosition(id),moved->formations[t].groupPosition(other))>=4.5f,"Displaced relief crossed occupancy");
                }
                if(!exchanged && a.slot==rear && b.slot==front) {
                    require(a.resting && battleDistance(f.groupPosition(front),tail)<.001f && battleDistance(f.groupPosition(rear),head)<.001f,
                        "Displaced relief lost actual positions or rest");
                    exchanged=true;
                }
                if(exchanged && f.organization.frontReliefs[3].front<0) {
                    for(unsigned id=0;id<25;++id)if(helpers&(1u<<id))
                        require(battleDistance(f.groupPosition(id),original[id])<.04f,"Helper lost its original displaced position");
                    completed=true;break;
                }
            }
            if(!completed)std::cerr<<"displaced axis="<<axis<<" sign="<<sign<<" team="<<team<<" started="<<started<<" phase="<<f.organization.frontReliefs[3].phase
                <<" route="<<static_cast<unsigned>(a.route)<<','<<static_cast<unsigned>(b.route)<<" offsets="<<a.offsetX<<','<<a.offsetZ<<'/'<<b.offsetX<<','<<b.offsetZ<<'\n';
            require(started && completed,"Displaced front failed to exchange with its reserve");
        }
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
