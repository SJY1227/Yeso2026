#include "../firmware/RoutineDevice/src/presentation/Motion.h"
#include "../firmware/RoutineDevice/src/generated/MotionAssets.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace routine;
int main(){
  auto state=std::make_unique<application::ProductState>();application::ProductView view;ui::MotionPlayer player;
  state->count=1;auto& record=state->routines[0];record.run.spec.id=9;record.run.spec.stepCount=1;record.food[0]=1;
  view.routine=0;view.screen=application::Screen::Letter;player.observe(*state,view,UINT32_MAX-100);
  assert(player.sample(0).kind==ui::MotionKind::None);
  view.screen=application::Screen::Routine;player.observe(*state,view,UINT32_MAX-50);
  assert(player.sample(49).kind==ui::MotionKind::Letter&&player.sample(49).frame==1);
  assert(player.sample(2049).kind==ui::MotionKind::None);
  view.screen=application::Screen::Feed;player.observe(*state,view,3000);
  view.screen=application::Screen::Growth;view.routine=-1;player.observe(*state,view,3001);
  assert(player.sample(3001).kind==ui::MotionKind::None); // No meal committed.
  view.screen=application::Screen::Feed;view.routine=0;player.observe(*state,view,4000);
  record.consumedRewards=1;view.screen=application::Screen::Growth;view.routine=-1;player.observe(*state,view,4001);
  assert(player.sample(4001).kind==ui::MotionKind::Eating&&player.transient(4001));
  player.skip();assert(!player.transient(4002));assert(record.consumedRewards==1&&state->companion.experience[0]==0);
  view.screen=application::Screen::AbandonConfirm;view.routine=0;player.observe(*state,view,5000);
  assert(player.sample(5000).kind==ui::MotionKind::Crying&&player.sample(5720).frame==4&&player.sample(5900).frame==0);
  view.selected^=1;player.observe(*state,view,5810);assert(player.sample(5810).frame==4); // Cursor doesn't reset the clip.
  player.enable(false);assert(player.sample(5811).kind==ui::MotionKind::None);
  player.enable(true);player.observe(*state,view,5812);assert(player.sample(5812).kind==ui::MotionKind::Crying);
  player.enable(false);
  view.screen=application::Screen::Routine;player.observe(*state,view,6000);view.screen=application::Screen::AbandonConfirm;player.observe(*state,view,6001);assert(player.sample(6001).kind==ui::MotionKind::None);
  player.enable(true);
  view.screen=application::Screen::SummaryIntro;view.summary.day=77;view.item=0;view.routine=-1;player.observe(*state,view,7000);
  view.screen=application::Screen::Summary;view.routine=0;player.observe(*state,view,7001);
  assert(player.sample(7001).kind==ui::MotionKind::SummaryLetter&&player.transient(7001));
  assert(player.sample(7901).frame==5&&player.sample(8081).kind==ui::MotionKind::None);
  view.screen=application::Screen::SummaryIntro;view.routine=-1;player.observe(*state,view,9000);
  view.screen=application::Screen::Summary;view.routine=0;player.observe(*state,view,9001);
  view.item=1;player.observe(*state,view,9002);assert(!player.transient(9002)); // New sheet cancels old sequence.
  view.screen=application::Screen::SummaryIntro;view.item=0;player.observe(*state,view,10000);
  ++view.summary.day;view.screen=application::Screen::Summary;player.observe(*state,view,10001);
  assert(player.sample(10001).kind==ui::MotionKind::None);
  player.enable(false);view.screen=application::Screen::SummaryIntro;player.observe(*state,view,11000);
  view.screen=application::Screen::Summary;player.observe(*state,view,11001);assert(!player.transient(11001));
  unsigned crying=0,bites=0;for(const auto& clip:assets::cryClips)if(clip.body)++crying;
  for(const auto& clip:assets::biteClips){assert(clip.count<=3);for(unsigned i=0;i<clip.count;++i)assert(clip.frames[i]);bites+=clip.count;}
  assert(crying==21&&bites==40);
  std::puts("PASS: cosmetic motion, committed-meal gating, rollover, skip, reduced motion, stage-specific clips and bounded partial bite inventory");
}
