#pragma once
#include <algorithm>
#include <cmath>

// 戦場のXZ平面で共有する距離と簡易コリジョン。描画人数には依存しない。
struct BattlePoint { float x, z; };
inline float battleDistance(BattlePoint a, BattlePoint b) { return std::hypot(b.x - a.x, b.z - a.z); }
inline float segmentDistance(BattlePoint a, BattlePoint b, BattlePoint p) {
    const float dx = b.x - a.x, dz = b.z - a.z;
    const float lengthSquared = dx * dx + dz * dz;
    const float t = lengthSquared > 0 ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / lengthSquared, 0.0f, 1.0f) : 0;
    return std::hypot(p.x - a.x - t * dx, p.z - a.z - t * dz);
}
inline float turnToward(float heading, float target, float radians) {
    const float delta = std::atan2(std::sin(target - heading), std::cos(target - heading));
    const float turned = heading + std::clamp(delta, -radians, radians);
    return std::atan2(std::sin(turned), std::cos(turned));
}
