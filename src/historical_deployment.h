#pragma once
#include "formation_world.h"
#include "historical_scenario.h"

// 模式地図の1単位を戦闘座標6単位へ変換。実距離や史実の兵数の縮尺ではない。
constexpr float deploymentScale=6;
inline FormationWorld makeDeployment(const history::Scenario& scenario) {
    std::vector<WorldFormation> entries;
    for(unsigned id=0;id<scenario.armies.size();++id) {
        const auto& army=scenario.armies[id];const auto pose=history::sample(army,scenario.startMinute);
        WorldFormation entry;entry.id=id;entry.side=pose.side==history::Side::West?1:0;entry.name=army.name;
        entry.formation.x=pose.x*deploymentScale;entry.formation.z=pose.z*deploymentScale;entry.formation.heading=pose.heading;
        for(auto& group:entry.formation.organization.smallGroups)group.heading=pose.heading;
        entries.push_back(std::move(entry));
    }
    FormationWorld world;world.reset(std::move(entries));return world;
}
inline std::vector<history::Pose> deploymentPoses(const FormationWorld& world) {
    std::vector<history::Pose> poses;
    for(const auto& entry:world.formations) {
        const auto& f=entry.formation;
        const auto action=f.strength<=0?history::Action::Dispersed:entry.order==WorldOrder::Withdraw?history::Action::Retreating:f.state==FormationState::Engaged?history::Action::Fighting:
            f.moving?history::Action::Marching:history::Action::Waiting;
        poses.push_back({f.x/deploymentScale,f.z/deploymentScale,f.heading,action,
            entry.side==1?history::Side::West:history::Side::East,f.moving});
    }
    return poses;
}
