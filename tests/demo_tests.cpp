#include "../firmware/RoutineDevice/src/diagnostics/DemoSession.h"
#include "../firmware/RoutineDevice/src/application/ProductController.h"
#include <cassert>
#include <cstdio>
using namespace routine;
using application::Screen;
using input::Intent;
struct LiveStore:application::ProductStore {
  application::ProductState saved{};
  unsigned loads=0,saves=0,archives=0;
  application::LoadResult load(application::ProductState& s) override{++loads;s=saved;return application::LoadResult::Loaded;}
  bool save(const application::ProductState& s) override{++saves;saved=s;return true;}
  bool archive(const application::ProductState&) override{++archives;return true;}
};
int main(){
  LiveStore live;live.saved.generation=987;live.saved.companion.experience[0]=17;
  diagnostics::DemoSession session(live);application::ProductController controller(session);application::Schedule schedule;
  assert(controller.begin()==application::LoadResult::Loaded&&controller.state().companion.experience[0]==17);
  const auto liveLoads=live.loads;
  session.start(100);assert(controller.begin()==application::LoadResult::Empty);
  session.schedule(schedule);assert(controller.importSchedule(schedule)==application::ImportResult::Accepted);
  controller.tick(session.time(100));assert(controller.view().screen==Screen::Home);
  assert(session.advance(3,100));controller.tick(session.time(100));assert(controller.view().screen==Screen::Letter);
  auto press=[&](Intent intent){controller.input(intent,session.time(100));};
  press(Intent::Confirm);assert(controller.view().screen==Screen::Routine);
  // Cancellation never awards completion or food.
  press(Intent::Previous);press(Intent::Confirm);assert(controller.view().screen==Screen::AbandonConfirm);
  press(Intent::Confirm);assert(controller.view().screen==Screen::Routine&&controller.state().eventCount==2);
  for(unsigned step=0;step<3;++step){
    press(Intent::Next);press(Intent::Confirm);assert(controller.view().screen==Screen::CompleteConfirm);
    press(Intent::Confirm);assert(controller.view().screen==Screen::StepSaved);
    if(step==0){assert(controller.begin()==application::LoadResult::Loaded);controller.tick(session.time(100));assert(controller.view().screen==Screen::StepSaved);}
    press(Intent::Confirm);assert(controller.view().screen==Screen::Feed);
    press(Intent::Confirm);assert(controller.view().screen==Screen::Growth);
    if(step==0){assert(controller.begin()==application::LoadResult::Loaded);controller.tick(session.time(100));assert(controller.view().screen==Screen::Routine);}
    else press(Intent::Confirm);
  }
  assert(controller.view().screen==Screen::Result&&controller.state().companion.experience[0]==3);
  press(Intent::Confirm);assert(controller.view().screen==Screen::SummaryIntro);
  press(Intent::Confirm);assert(controller.view().screen==Screen::Summary&&controller.view().summary.completedSteps==3);
  press(Intent::Confirm);press(Intent::Confirm);assert(controller.view().screen==Screen::Home);
  assert(!session.archive(controller.state())&&live.archives==0);
  assert(live.loads==liveLoads&&live.saves==0&&live.saved.generation==987&&live.saved.companion.experience[0]==17);
  session.stop();assert(controller.begin()==application::LoadResult::Loaded);
  assert(controller.state().generation==987&&controller.state().companion.experience[0]==17);
  // Demo reset discards only its RAM snapshot; deadline and warning use same engine.
  session.start(100);controller.begin();session.schedule(schedule);controller.importSchedule(schedule);
  assert(session.advance(7140,100));controller.tick(session.time(100));
  assert(controller.view().warning&&controller.state().routines[0].run.warningEmitted);
  assert(session.advance(60,100));controller.tick(session.time(100));
  assert(controller.state().routines[0].run.phase==domain::Phase::Expired&&controller.state().routines[0].run.earnedRewards==0);
  assert(!session.advance(86400,100)&&!session.advance(0,100));
  session.start(UINT32_MAX-500);assert(session.time(499).unixSeconds==diagnostics::DemoSession::kEpoch+1);
  assert(!session.archive(controller.state())&&live.saves==0&&live.archives==0);
  session.stop();assert(controller.begin()==application::LoadResult::Loaded&&controller.state().generation==987);
  using diagnostics::DemoAction;
  assert(diagnostics::parseDemoCommand("demo start").action==DemoAction::Start);
  assert(diagnostics::parseDemoCommand("demo advance 86400").seconds==86400);
  for(const char* bad:{"demo","demo start x","demo advance 0","demo advance -1","demo advance 9999999999999999999","demo advance 1 x"})
    assert(diagnostics::parseDemoCommand(bad).action==DemoAction::Invalid);
  for(const char* blocked:{"storage snapshot","storage read 3 0 16","api sync","api pair 123456","load begin 10","assets begin 10 aa","sleep 2","wifi setup 00 00","sync"})
    assert(!diagnostics::demoAllowsCommand(blocked));
  for(const char* allowed:{"status","help","reboot","input confirm","motion off"})assert(diagnostics::demoAllowsCommand(allowed));
  std::puts("PASS: RAM-only routine demo, 3 steps/food/summary, reload without duplicate growth, cancel/deadline, live-store isolation/restoration, command boundary and clock rollover");
}
