#pragma once
#include <array>
#include <string_view>

enum class UnitType : unsigned { Spearman, Samurai, Archer, Cavalry };
struct UnitVisualDefinition {
    const wchar_t* name;
    const wchar_t* assetPrefix;
    float idleSize, attackSize, idlePivot;
};
inline constexpr std::array<UnitVisualDefinition,4> unitVisuals{{
    {L"槍足軽",L"ashigaru",3.4f,5,.88f},
    {L"刀武士",L"samurai",3.4f,5,.88f},
    {L"弓足軽",L"archer",3.4f,5,.88f},
    {L"騎馬武者",L"cavalry",5,6,.75f}
}};
inline const UnitVisualDefinition& unitVisual(UnitType type) {return unitVisuals.at(static_cast<unsigned>(type));}
