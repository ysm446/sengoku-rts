#pragma once
#include "combat_geometry.h"
#include <array>

// 接触面の近似モデル。正面・右・背面・左の各面を16区間で測る。
struct ContactBody { BattlePoint position{}; float heading = 0; bool present = false; bool fighting = false; };
struct ContactFace {
    std::array<float, 25> enemyWidths{};
    float width() const { float total = 0; for (float value : enemyWidths) total += value; return total; }
};
struct ContactFronts {
    static constexpr float halfExtent = 2.25f;
    static constexpr float reach = 3.0f;
    static constexpr unsigned samples = 16;
    std::array<ContactFace, 4> faces{};
};
struct ContactAllocationFace {
    std::array<float, 25> enemyFighters{};
    float fighters() const { float total = 0; for (float value : enemyFighters) total += value; return total; }
};
struct ContactAllocation {
    // 一列、一人あたり幅0.9の仮値。実兵数ではなく連続量で配分する。
    static constexpr float widthPerFighter = 0.9f;
    std::array<ContactAllocationFace, 4> faces{};
    float reserve = 0;
    float fighters() const { float total = 0; for (const auto& face : faces) total += face.fighters(); return total; }
};
struct FaceDeployment {
    std::array<float, 4> deployed{};
    float accountedStrength = 0;
    float total() const { float sum = 0; for (float value : deployed) sum += value; return sum; }
    float reserve() const { return std::max(0.0f, accountedStrength - total()); }
};
inline void advanceFaceDeployment(FaceDeployment& current, const ContactAllocation& target, float strength, float seconds) {
    strength = std::max(0.0f, strength);
    if (current.accountedStrength > strength && current.accountedStrength > 0)
        for (auto& value : current.deployed) value *= strength / current.accountedStrength;
    current.accountedStrength = strength;
    const float availableReserve = current.reserve();
    std::array<float, 4> excess{}, shortage{};
    float totalExcess = 0, totalShortage = 0;
    for (unsigned face = 0; face < 4; ++face) {
        const float difference = target.faces[face].fighters() - current.deployed[face];
        excess[face] = std::max(0.0f, -difference); shortage[face] = std::max(0.0f, difference);
        totalExcess += excess[face]; totalShortage += shortage[face];
    }
    // 小組全体で毎秒2人。戻った人数を同じ刻みで別の面へ再投入しない。
    const float budget = 2.0f * seconds;
    const float returning = std::min(totalExcess, budget);
    const float entering = std::min({totalShortage, availableReserve, budget - returning});
    for (unsigned face = 0; face < 4; ++face) {
        if (totalExcess > 0) current.deployed[face] -= excess[face] / totalExcess * returning;
        if (totalShortage > 0) current.deployed[face] += shortage[face] / totalShortage * entering;
        current.deployed[face] = std::max(0.0f, current.deployed[face]);
    }
}
inline ContactAllocation allocateContactFronts(const ContactFronts& fronts, float strength) {
    ContactAllocation result;
    result.reserve = std::max(0.0f, strength);
    double capacity = 0;
    for (const auto& face : fronts.faces) capacity += face.width() / static_cast<double>(ContactAllocation::widthPerFighter);
    if (capacity <= 0 || strength <= 0) return result;
    const double ratio = std::min(1.0, strength / capacity);
    for (unsigned face = 0; face < 4; ++face) for (unsigned enemy = 0; enemy < 25; ++enemy) {
        const float allocated = std::min(result.reserve, static_cast<float>(fronts.faces[face].enemyWidths[enemy] /
            static_cast<double>(ContactAllocation::widthPerFighter) * ratio));
        result.faces[face].enemyFighters[enemy] = allocated;
        result.reserve = std::max(0.0f, result.reserve - allocated);
    }
    return result;
}
inline ContactAllocation participatingContactFronts(const ContactFronts& fronts, const FaceDeployment& deployment, float strength) {
    ContactAllocation result;
    result.reserve = std::max(0.0f, strength);
    const float retained = deployment.total() > strength && deployment.total() > 0 ? std::max(0.0f, strength) / deployment.total() : 1;
    for (unsigned face = 0; face < 4; ++face) {
        const float width = fronts.faces[face].width();
        if (width <= 0) continue;
        const float available = std::min(deployment.deployed[face] * retained, width / ContactAllocation::widthPerFighter);
        for (unsigned enemy = 0; enemy < 25; ++enemy) {
            const float fighters = std::min(result.reserve, available * fronts.faces[face].enemyWidths[enemy] / width);
            result.faces[face].enemyFighters[enemy] = fighters;
            result.reserve -= fighters;
        }
    }
    return result;
}
inline ContactFronts measureContactFronts(const std::array<std::array<ContactBody, 25>, 2>& bodies, unsigned team, unsigned id) {
    ContactFronts result;
    const auto& self = bodies.at(team).at(id);
    if (!self.fighting) return result;
    constexpr float half = ContactFronts::halfExtent;
    std::array<unsigned, 25> candidates{};
    unsigned candidateCount = 0;
    for (unsigned enemy = 0; enemy < 25; ++enemy) {
        const auto& target = bodies[1 - team][enemy];
        const float distance = battleDistance(self.position, target.position);
        if (target.fighting && distance >= 2 * half && distance <= 2 * half * 1.41421357f + ContactFronts::reach)
            candidates[candidateCount++] = enemy;
    }
    if (candidateCount == 0) return result;
    for (unsigned face = 0; face < 4; ++face) {
        const float angle = self.heading - face * 1.57079632679f;
        const BattlePoint normal{std::cos(angle), std::sin(angle)}, tangent{-normal.z, normal.x};
        for (unsigned sample = 0; sample < ContactFronts::samples; ++sample) {
            const float offset = -half + (sample + 0.5f) * (2 * half / ContactFronts::samples);
            const BattlePoint p{self.position.x + normal.x * half + tangent.x * offset,
                self.position.z + normal.z * half + tangent.z * offset};
            int nearest = -1;
            float best = ContactFronts::reach + 0.00001f;
            for (unsigned candidate = 0; candidate < candidateCount; ++candidate) {
                const unsigned enemy = candidates[candidate];
                const auto& target = bodies[1 - team][enemy];
                const float c = std::cos(target.heading), s = std::sin(target.heading);
                const float dx = p.x - target.position.x, dz = p.z - target.position.z;
                const float x = std::clamp(dx * c + dz * s, -half, half);
                const float z = std::clamp(-dx * s + dz * c, -half, half);
                const BattlePoint q{target.position.x + x * c - z * s, target.position.z + x * s + z * c};
                const float distance = battleDistance(p, q);
                if (distance >= best || (q.x - p.x) * normal.x + (q.z - p.z) * normal.z <= 0.00001f) continue;
                bool blocked = false;
                for (unsigned otherTeam = 0; otherTeam < 2 && !blocked; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                    if ((otherTeam == team && other == id) || (otherTeam != team && other == enemy) || !bodies[otherTeam][other].present) continue;
                    if (segmentDistance(p, q, bodies[otherTeam][other].position) < half) { blocked = true; break; }
                }
                if (!blocked) { best = distance; nearest = static_cast<int>(enemy); }
            }
            // 同じ面の同じ区間を複数の敵へ重複計上しない。
            if (nearest >= 0) result.faces[face].enemyWidths[nearest] += 2 * half / ContactFronts::samples;
        }
    }
    return result;
}
