#include "LoadAssets.h"
#include "../firmware/RoutineDevice/src/application/ProductController.h"
#include "../firmware/RoutineDevice/src/application/InputSession.h"
#include "../firmware/RoutineDevice/src/presentation/ProductRenderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
using namespace routine;
using namespace application;
using input::Intent;
constexpr int64_t dayStart=1767193200; // 2026-01-01 00:00 KST
struct Store : ProductStore {
  ProductState saved{};
  unsigned writes=0;
  LoadResult load(ProductState& out) override {out=saved;return LoadResult::Loaded;}
  bool save(const ProductState& in) override {saved=in;++writes;return true;}
};
void fixture(ProductState& state) {
  state={};state.count=3;state.scheduleRevision=1;
  const unsigned hours[]={11,9,24};
  const uint8_t steps[]={5,3,1};
  for(size_t i=0;i<3;++i) {
    auto& r=state.routines[i];domain::RunSpec spec{};
    spec.id=i+1;spec.startsAt=dayStart+hours[i]*3600;spec.endsAt=spec.startsAt+1800;spec.stepCount=steps[i];
    for(uint8_t s=0;s<steps[i];++s){spec.steps[s]=uint32_t(i*20+s+1);std::strcpy(r.stepNames[s].data(),"준비하기");}
    assert(domain::createRun(spec,{},r.run)==domain::SpecError::None);
    std::strcpy(r.title.data(),i?"오전 루틴":"병원가기");r.apiManaged=true;
    if(i<2) {
      r.run=domain::advanceTime(r.run,{spec.startsAt,true}).next;
      r.run.remoteCompleted=i?7:5;
      r.run.completedSteps=domain::countSteps(r.run.remoteCompleted);
      r.run=domain::advanceTime(r.run,{spec.endsAt+1,true}).next;
      if(i){r.run.phase=domain::Phase::Completed;++r.run.version;}
      r.resultSeen=true;
    }
  }
  for(unsigned i=0;i<3;++i) {
    std::strcpy(state.routines[1].stepNames[i].data(),std::array<const char*,3>{"씻기","아침먹기","옷갈아입기"}[i]);
    std::strcpy(state.routines[0].stepNames[i].data(),std::array<const char*,3>{"7번 버스타기","병원도착","약국가기"}[i]);
  }
  assert(validProduct(state));
}
int main(int argc,char** argv) {
  auto store=std::make_unique<Store>();fixture(store->saved);
  auto controller=std::make_unique<ProductController>(*store);
  auto& c=*controller;assert(c.begin()==LoadResult::Loaded);
  const domain::TimeSample now{dayStart+12*3600,true};
  c.tick(now);assert(c.view().screen==Screen::SummaryIntro);
  const auto day=c.view().summary.day;
  assert(c.view().summary.runs==2&&c.view().summary.steps==8&&c.view().summary.completedSteps==5);
  assert(summaryPage(c.state(),day,0).routine==1);
  const auto page=summaryPage(c.state(),day,2);
  assert(page.pages==3&&page.routine==0&&page.firstStep==3&&page.stepCount==2);
  assert(!summaryPage(c.state(),day,3).valid()&&!summaryPage(c.state(),-1,0).valid());
  c.input(Intent::Confirm,now);assert(c.view().screen==Screen::Summary&&c.view().routine==1&&c.view().item==0);
  InputSession session;session.publish(c.state(),c.view(),false);const auto context=session.context();
  c.input(Intent::Next,now);assert(c.view().routine==0&&c.view().item==1);
  assert(!session.accepts(context,c.state(),c.view()));
  c.input(Intent::Previous,now);c.input(Intent::Previous,now);assert(c.view().item==2);
  c.input(Intent::Confirm,now);assert(c.view().screen==Screen::SummaryEnd);
  c.input(Intent::Previous,now);assert(c.view().screen==Screen::Summary&&c.view().item==2);
  assert(store->writes==0&&c.state().eventCount==0&&c.state().companion.experience[0]==0);
  c.input(Intent::Confirm,now);c.input(Intent::Confirm,now);
  assert(c.view().screen==Screen::Home&&c.state().summarySeenDay==day&&store->writes==1);
  assert(c.begin()==LoadResult::Loaded);c.tick(now);assert(c.view().screen==Screen::Home);
  c.input(Intent::Confirm,now);c.input(Intent::Confirm,now);assert(c.view().item==0);
  c.input(Intent::Next,now);assert(c.view().item==1);
  c.input(Intent::Confirm,{dayStart+86400,true}); // Old summary input cannot open tomorrow's letter.
  assert(c.view().screen==Screen::Letter&&c.state().routines[2].run.phase==domain::Phase::LetterPending);

  auto state=std::make_unique<ProductState>();fixture(*state);
  state->count=kMaxRuns;
  for(size_t i=0;i<kMaxRuns;++i){state->routines[i]=state->routines[0];state->routines[i].run.spec.id=i+1;state->routines[i].run.spec.stepCount=domain::kMaxSteps;}
  assert(summaryPage(*state,day,0).pages==96);
  const auto last=summaryPage(*state,day,95);assert(last.valid()&&last.firstStep==15&&last.stepCount==1);
  assert(!summaryPage(*state,day,96).valid());

  loadTestAssets();fixture(*state);state->companion.experience[0]=100;
  ProductView view;view.summary=summarizeDay(*state,day);
  std::vector<uint16_t> pixels(240*320+2,0x1234);
  const auto save=[&](const std::string& name,ui::MotionFrame motion=ui::MotionFrame{}) {
    ui::renderProduct(pixels.data()+1,*state,view,true,550,motion);
    assert(pixels.front()==0x1234&&pixels.back()==0x1234);
    if(argc<2)return;
    std::ofstream f(std::string(argv[1])+"/"+name+".ppm",std::ios::binary);f<<"P6\n240 320\n255\n";
    for(size_t i=1;i<=240*320;++i){auto p=pixels[i];char rgb[]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(rgb,3);}assert(f.good());
  };
  for(unsigned theme=0;theme<10;++theme) {
    state->companion.selected=uint8_t(theme);
    view.screen=Screen::SummaryIntro;save("intro-"+std::to_string(theme));
    view.screen=Screen::SummaryEnd;save("end-"+std::to_string(theme));
    view.screen=Screen::Summary;
    for(uint8_t p=0;p<3;++p){view.item=p;save("sheet-"+std::to_string(theme)+"-"+std::to_string(p));}
  }
  state->companion.selected=0;view.item=0;
  for(uint8_t frame=0;frame<6;++frame)save("motion-"+std::to_string(frame),{ui::MotionKind::SummaryLetter,frame,0,3,0});
  std::strcpy(state->routines[1].title.data(),"준비물을 챙기고 가족과 함께 외출 준비를 끝내기");
  std::strcpy(state->routines[1].stepNames[0].data(),"마지막 준비물까지 모두 가방에 넣고 잘 확인하기");
  save("long-title");
  std::puts("PASS: summary date/order, 3-row pagination up to 96 sheets, outcome masks, navigation/exit, read-only review, seen-day persistence, stale input/day rollover and 10-theme renderer bounds");
}
