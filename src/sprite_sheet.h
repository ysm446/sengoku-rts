#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

// WICでPNGを読み、期待するサイズとRGBA形式を検証する。
std::vector<std::uint32_t> loadSpriteSheet(const std::filesystem::path& path, unsigned width, unsigned height);
