#pragma once
#include <windows.h>
#include <filesystem>
// 2は従来の戦闘試作、3は兵種確認へ切り替える要求。
int runHistoricalDemo(HINSTANCE instance, int show, bool smoke, bool warp,
    const std::filesystem::path& capture, const std::filesystem::path& directory, float yawDegrees);
