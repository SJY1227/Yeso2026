#include "LoadAssets.h"
#include "../firmware/RoutineDevice/src/presentation/ProductRenderer.h"
#include "../firmware/RoutineDevice/src/presentation/ImageRenderer.h"
#include "../firmware/RoutineDevice/src/generated/CatalogAssets.h"
#include "../firmware/RoutineDevice/src/generated/ProductAssets.h"
#include "../firmware/RoutineDevice/src/generated/ThemeAssets.h"
#include "../firmware/RoutineDevice/src/content/Catalog.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
using namespace routine;
using application::Screen;
constexpr size_t pixels=240*320;
void save(const uint16_t* data,const char* dir,const std::string& name,bool binary=false) {
  if (!dir) return;
  std::ofstream f(std::string(dir)+"/"+name+(binary?".bin":".ppm"),std::ios::binary);
  if (!binary) f<<"P6\n240 320\n255\n";
  for(size_t i=0;i<pixels;++i) {
    const auto v=data[i];
    if(binary) { const char b[]={char(v),char(v>>8)}; f.write(b,2); }
    else { const char b[]={char((v>>11)*255/31),char(((v>>5)&63)*255/63),char((v&31)*255/31)};f.write(b,3); }
  }
  assert(f.good());
}
void asset(const assets::CompressedImage& source,const char* dir,const std::string& name) {
  std::vector<uint16_t> buffer(pixels+2,0xffff);
  assert(ui::drawImage(buffer.data()+1,source));
  assert(buffer.front()==0xffff&&buffer.back()==0xffff);
  save(buffer.data()+1,dir,"asset-"+name,true);
}
void malformed() {
  std::vector<uint16_t> buffer(pixels+2,0x1234);
  // Literal RGB565 red/opaque, then an overlapping match across pixels.
  const uint8_t valid[]={8,0,248,255,2,48};
  assets::CompressedImage source{valid,sizeof(valid),239,317,1,3};
  assert(ui::drawImage(buffer.data()+1,source));
  for(unsigned y=317;y<320;++y) assert(buffer[1+y*240+239]==0xf800);
  for(size_t size=0;size<sizeof(valid);++size) { source.size=size;assert(!ui::drawImage(buffer.data()+1,source)); }
  source.size=sizeof(valid);source.width=2;assert(!ui::drawImage(buffer.data()+1,source));source.width=1;
  source.height=2;assert(!ui::drawImage(buffer.data()+1,source)); // Overlong match.
  const uint8_t badDistance[]={1,0,0};source={badDistance,sizeof(badDistance),0,0,1,1};
  assert(!ui::drawImage(buffer.data()+1,source));
  const uint8_t extra[]={0,0,248,255,0};source={extra,sizeof(extra),0,0,1,1};
  assert(!ui::drawImage(buffer.data()+1,source));
  source.bytes=nullptr;assert(!ui::drawImage(buffer.data()+1,source));
  source={valid,sizeof(valid),239,317,1,3};
  assert(ui::drawImageAt(buffer.data()+1,source,-1,319,4,6));
  for(unsigned x=0;x<3;++x)assert(buffer[1+319*240+x]==0xf800);
  assert(!ui::drawImageAt(buffer.data()+1,source,0,0,0,6));
  assert(buffer.front()==0x1234&&buffer.back()==0x1234);
}
int main(int argc,char** argv) {
  loadTestAssets();
  const char* dir=argc>1?argv[1]:nullptr;
  malformed();
  asset(assets::pinkBase,dir,"pink-base");asset(assets::darkBase,dir,"dark-base");
  asset(assets::letterImage,dir,"product-letter");asset(assets::routineImage,dir,"product-routine");
  asset(assets::catalogImage,dir,"product-catalog");asset(assets::cryImage,dir,"product-cry");asset(assets::blueberryImage,dir,"product-blueberry");
  for(unsigned i=0;i<30;++i) {
    asset(*assets::characterArt[i].image,dir,"characters-"+std::to_string(i));
    if(assets::lockedCharacterArt[i].image) asset(*assets::lockedCharacterArt[i].image,dir,"locked-"+std::to_string(i));
  }
  for(unsigned i=0;i<31;++i) asset(*assets::foodArt[i].image,dir,"food-"+std::to_string(i+1));
  asset(assets::catalogArrows,dir,"arrows");asset(assets::catalogPinkBar,dir,"topbar-pink");
  asset(assets::catalogDarkBar,dir,"topbar-dark");asset(assets::catalogLock,dir,"lock");
  for(unsigned i=0;i<10;++i){
    const auto& theme=assets::characterTheme(uint8_t(i));
    asset(*theme.background,dir,"home-"+std::to_string(i));asset(*theme.topbar,dir,"bar-"+std::to_string(i));asset(*theme.arrows,dir,"arrows-"+std::to_string(i));
    application::ProductState state;state.companion.selected=uint8_t(i);
    application::ProductView view;
    std::vector<uint16_t> buffer(pixels+2,0x1234);
    ui::renderProduct(buffer.data()+1,state,view,true,550);
    save(buffer.data()+1,dir,"home-"+std::to_string(i));
    assert(buffer.front()==0x1234&&buffer.back()==0x1234);
    // At zero growth the lower area remains source background, with only the
    // Figma gauge. No panel, stage/status labels or extra footer is painted.
    std::vector<uint16_t> base(pixels);assert(ui::drawImage(base.data(),*theme.background));assert(ui::drawImage(base.data(),*theme.homeBase));
    assert(ui::drawImage(base.data(),*assets::homeCharacterArt[i*3]));
    for(unsigned y=267;y<320;++y)for(unsigned x=0;x<240;++x)
      if(y<291||y>=299)assert(buffer[1+y*240+x]==base[y*240+x]);
    assert(buffer[1+294*240+100]==theme.track);
  }
  unsigned screens=0;
  for(uint8_t theme=0;theme<10;++theme) for(bool acquired : {false,true}) {
    application::ProductState state;
    state.companion.selected=theme;state.companion.owned=acquired?1023:0;
    if(acquired)state.companion.highestStage.fill(3);
    // These are PC-only visual fixtures, not persisted domain states.
    for(auto screen : {Screen::CharacterCatalog,Screen::CharacterConfirm,Screen::RewardCatalog,Screen::FoodDetail}) {
      const unsigned count=(screen==Screen::CharacterCatalog||screen==Screen::CharacterConfirm)?30:31;
      for(unsigned i=0;i<=count;++i) {
        application::ProductView view;view.screen=screen;view.item=uint8_t(i);
        state.count=acquired?1:0;state.routines[0].run.completedSteps=1;state.routines[0].run.spec.stepCount=1;state.routines[0].run.earnedRewards=1;state.routines[0].food[0]=uint8_t(i+1);
        std::vector<uint16_t> buffer(pixels+2,0x1234);
        ui::renderProduct(buffer.data()+1,state,view,true,9*60+10);
        assert(buffer.front()==0x1234&&buffer.back()==0x1234);++screens;
        if(screen==Screen::CharacterCatalog||screen==Screen::RewardCatalog)
          save(buffer.data()+1,dir,(count==30?"character-":"food-")+std::to_string(theme)+"-"+(acquired?"open-":"locked-")+std::to_string(i));
      }
    }
  }
  std::printf("PASS: packed assets, scaled/clipped/truncated streams, 10 home layouts, %u catalog renders with bounds guards\n",screens);
}
