#include "../tests/LoadAssets.h"
#include "../firmware/RoutineDevice/src/presentation/ProductRenderer.h"
#include <fstream>
#include <memory>
#include <string>
#include <cstring>
#include <vector>
using namespace routine;
int main(int argc,char** argv){
  if(argc!=2)return 1;
  loadTestAssets();auto state=std::make_unique<application::ProductState>();application::ProductView view;
  std::vector<uint16_t> pixels(240*320);
  const auto save=[&](std::string name,ui::MotionFrame motion={}){ui::renderProduct(pixels.data(),*state,view,true,550,motion);ui::renderDeviceStatus(pixels.data(),false,-1);std::ofstream f(std::string(argv[1])+"/"+name+".ppm",std::ios::binary);f<<"P6\n240 320\n255\n";for(auto v:pixels){char rgb[]={char((v>>11)*255/31),char(((v>>5)&63)*255/63),char((v&31)*255/31)};f.write(rgb,3);}return f.good();};
  state->count=1;auto& r=state->routines[0];r.run.spec.stepCount=5;r.run.spec.startsAt=1791068400;r.run.spec.endsAt=r.run.spec.startsAt+3600;
  std::strcpy(r.title.data(),"병원 가기");std::strcpy(r.stepNames[0].data(),"7번 버스 타기");
  for(unsigned c=0;c<10;++c){state->companion.selected=uint8_t(c);
    for(unsigned stage=0;stage<3;++stage){state->companion.experience[c]=stage==0?0:stage==1?40:90;view.screen=application::Screen::Home;view.routine=-1;if(!save("home-"+std::to_string(c)+"-"+std::to_string(stage+1)))return 2;}
    view.routine=0;
    for(auto screen:{application::Screen::Letter,application::Screen::Routine,application::Screen::AbandonConfirm}){view.screen=screen;if(!save("flow-"+std::to_string(c)+"-"+std::to_string(int(screen))))return 2;}
  }
  state->companion.selected=0;state->companion.experience[0]=0;view.routine=0;view.rewardStep=0;view.screen=application::Screen::Feed;
  for(unsigned food=1;food<=31;++food){r.food[0]=uint8_t(food);if(!save("food-"+std::to_string(food)))return 2;}
  view.screen=application::Screen::Routine;
  for(uint8_t frame=0;frame<21;++frame)if(!save("letter-motion-"+std::to_string(frame),{ui::MotionKind::Letter,frame,0,1,0}))return 2;
  for(uint8_t c:{uint8_t(0),uint8_t(6),uint8_t(9)})for(uint8_t stage=1;stage<=3;++stage){
    state->companion.selected=c;state->companion.experience[c]=stage==1?0:stage==2?40:90;view.screen=application::Screen::AbandonConfirm;
    for(uint8_t frame=0;frame<5;++frame)if(!save("cry-motion-"+std::to_string(c)+"-"+std::to_string(stage)+"-"+std::to_string(frame),{ui::MotionKind::Crying,frame,c,stage,0}))return 2;
  }
  state->companion.selected=0;view.screen=application::Screen::Growth;
  for(uint8_t food=1;food<=31;++food)for(uint8_t frame=0;frame<4;++frame)
    if(!save("eat-motion-"+std::to_string(food)+"-"+std::to_string(frame),{ui::MotionKind::Eating,frame,0,1,food}))return 2;
}
