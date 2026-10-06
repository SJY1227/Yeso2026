#include "LoadAssets.h"
#include <cstdio>
#include <limits>
int main(){
  loadTestAssets();
  using namespace routine::ui;
  std::ifstream f("assets/packed/ui.pak",std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
  const auto* original=archivedImage(0,assets::kAssetPayloadBytes);
  assert(original);
  uint32_t crc=0xffffffff;
  for(size_t at=16;at<bytes.size();at+=4096)crc=updateImageCrc(crc,bytes.data()+at,std::min<size_t>(4096,bytes.size()-at));
  assert(~crc==assets::kAssetCrc);
  for(size_t cut : {size_t(0),size_t(8),size_t(15),bytes.size()-1})
    assert(!bindImageArchive(bytes.data(),cut,assets::kAssetPayloadBytes,assets::kAssetCrc));
  for(size_t pos : {size_t(0),size_t(8),size_t(12),size_t(100),bytes.size()-1}){
    bytes[pos]^=1;assert(!bindImageArchive(bytes.data(),bytes.size(),assets::kAssetPayloadBytes,assets::kAssetCrc));bytes[pos]^=1;
    assert(archivedImage(0,assets::kAssetPayloadBytes)==original);
  }
  assert(!bindImageArchive(bytes.data(),bytes.size(),assets::kAssetPayloadBytes,assets::kAssetCrc^1));
  assert(!archivedImage(assets::kAssetPayloadBytes,1));
  assert(!archivedImage(std::numeric_limits<size_t>::max(),2));
  assert(!archivedImage(1,std::numeric_limits<size_t>::max()));
  struct Source { const uint8_t* bytes;unsigned reads=0; } source{bytes.data()+16};
  bindImageProvider(&source,[](void* p,size_t offset,size_t)->const uint8_t*{auto& s=*static_cast<Source*>(p);++s.reads;return s.bytes+offset;},assets::kAssetPayloadBytes);
  assert(archivedImage(20,30)==bytes.data()+36&&source.reads==1);
  assert(!archivedImage(assets::kAssetPayloadBytes,1)&&source.reads==1);
  std::puts("PASS: pack version, CRC, truncation, size/offset overflow and rejected replacement preserves current pack");
}
