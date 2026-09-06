#pragma once
#include "unit_type.h"
#include <stdexcept>

// 既存の小組戦闘で比較するための仮値。距離は描画座標系の単位。
struct MeleeProfile {
    float groupRange, stopDistance, individualRange, moveSpeed, turnRate, damagePerSecond;
};
inline MeleeProfile meleeProfile(UnitType unit) {
    switch(unit) {
    case UnitType::Spearman:return {7.5f,7.2f,3.5f,1.8f,3.141592654f,1.2f};
    case UnitType::Samurai:return {5.2f,4.9f,1.5f,2.0f,3.5f,1.2f};
    case UnitType::Cavalry:return {5.6f,5.3f,2.2f,4.0f,.7f,1.2f};
    default:throw std::invalid_argument("This unit has no melee simulation profile yet");
    }
}
