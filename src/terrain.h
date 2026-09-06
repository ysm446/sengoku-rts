#pragma once
#include <cmath>

// シミュレーションの射線と描画で共用する試作地形。
inline float terrainHeight(float x, float z) {
    return 1.2f * std::sin(x * 0.055f) * std::cos(z * 0.07f) +
        3.6f * std::exp(-((x + 37) * (x + 37) + (z - 25) * (z - 25)) / 330.0f);
}
