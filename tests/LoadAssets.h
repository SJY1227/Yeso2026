#pragma once
#include "../firmware/RoutineDevice/src/presentation/ImageArchive.h"
#include "../firmware/RoutineDevice/src/generated/AssetPack.h"
#include <fstream>
#include <vector>
#include <cassert>
inline void loadTestAssets() {
  static std::vector<uint8_t> bytes;
  std::ifstream file("assets/packed/ui.pak",std::ios::binary);
  bytes.assign(std::istreambuf_iterator<char>(file),{});
  assert(routine::ui::bindImageArchive(bytes.data(),bytes.size(),assets::kAssetPayloadBytes,assets::kAssetCrc));
}
