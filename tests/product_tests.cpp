#include "LoadAssets.h"
#include "../firmware/RoutineDevice/src/application/ProductController.h"
#include "../firmware/RoutineDevice/src/application/SnapshotStore.h"
#include "../firmware/RoutineDevice/src/application/ScheduleTransfer.h"
#include "../firmware/RoutineDevice/src/presentation/ProductRenderer.h"
#include "../firmware/RoutineDevice/src/content/Catalog.h"
#include "../firmware/RoutineDevice/src/application/RewardLedger.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using namespace routine;
using namespace application;
using input::Intent;
struct Slots : SlotIo {
  std::vector<uint8_t> slots[2];
  bool fail=false,uncertain=false,readError=false;
  uint64_t sealed=0;
  bool sealFailure=false;
  bool floor(uint64_t& out) override { out=sealed;return !readError; }
  bool seal(uint64_t value) override { if(sealFailure)return false;sealed=value;return true; }
  SlotRead read(unsigned s,uint8_t* out,size_t capacity,size_t& size) override {
    if(readError||slots[s].size()>capacity)return SlotRead::Error;
    if(slots[s].empty())return SlotRead::Missing;
    size=slots[s].size();std::memcpy(out,slots[s].data(),size);return SlotRead::Ok;
  }
  bool write(unsigned s,const uint8_t* bytes,size_t size) override {
    slots[s].assign(bytes,bytes+(fail?size/2:size));return !fail&&!uncertain;
  }
};
Schedule schedule(uint8_t count=2) {
  Schedule s;s.revision=42;s.count=1;auto& r=s.routines[0];
  domain::RunSpec spec{1,1000,2000,count,{11,12}};
  assert(domain::createRun(spec,{domain::DeadlineRule::DeadlineWins,100},r.run)==domain::SpecError::None);
  std::strcpy(r.title.data(),"병원 가기");std::strcpy(r.stepNames[0].data(),"7번 버스 타기");std::strcpy(r.stepNames[1].data(),"접수하기");return s;
}
void active(ProductController& c,Schedule s=schedule()) {
  assert(c.begin()==LoadResult::Empty);assert(c.importSchedule(s)==ImportResult::Accepted);
  c.tick({1000,true});assert(c.view().screen==Screen::Letter);
  c.input(Intent::Next,{1001,true});assert(c.view().screen==Screen::Letter);
  c.input(Intent::Confirm,{1001,true});assert(c.view().screen==Screen::Routine);
}
void confirmStep(ProductController& c,int64_t now) {
  c.input(Intent::Next,{now,true});c.input(Intent::Confirm,{now,true});
  assert(c.view().screen==Screen::CompleteConfirm);
  c.input(Intent::Confirm,{now,true});assert(c.view().screen==Screen::StepSaved);
}
void feedAndContinue(ProductController& c,int64_t now) {
  c.input(Intent::Confirm,{now,true}); assert(c.view().screen==Screen::Feed);
  c.input(Intent::Confirm,{now,true}); assert(c.view().screen==Screen::Growth);
  c.input(Intent::Confirm,{now,true});
}
void closeSummary(ProductController& c,int64_t now) {
  assert(c.view().screen==Screen::SummaryIntro);
  c.input(Intent::Confirm,{now,true}); assert(c.view().screen==Screen::Summary);
  c.input(Intent::Confirm,{now,true}); assert(c.view().screen==Screen::SummaryEnd);
  c.input(Intent::Confirm,{now,true}); assert(c.view().screen==Screen::Home);
}
void workflow() {
  Slots io;SnapshotStore storage(io);ProductController c(storage);active(c);
  const auto generation=c.state().generation;
  c.input(Intent::Previous,{1002,true});assert(c.view().selected==0);
  c.input(Intent::Confirm,{1002,true});assert(c.view().screen==Screen::AbandonConfirm&&c.view().selected==0);
  c.input(Intent::Confirm,{1002,true});assert(c.view().screen==Screen::Routine&&c.state().generation==generation);
  c.input(Intent::Confirm,{1002,true});assert(c.view().screen==Screen::CompleteConfirm);
  c.input(Intent::Previous,{1002,true});assert(c.view().screen==Screen::Routine&&c.state().generation==generation);
  confirmStep(c,1003);assert(c.state().routines[0].run.earnedRewards==1);
  const auto events=c.state().eventCount;
  SnapshotStore restoredStore(io);ProductController restored(restoredStore);
  assert(restored.begin()==LoadResult::Loaded&&restored.view().screen==Screen::StepSaved);
  feedAndContinue(restored,1004);assert(restored.view().screen==Screen::Routine);
  assert(restored.state().companion.experience[0]==1&&restored.state().routines[0].consumedRewards==1);
  assert(restored.state().eventCount==events&&restored.state().routines[0].run.earnedRewards==1);
  assert(restored.importSchedule(schedule())==ImportResult::Duplicate);
  auto conflict=schedule();conflict.routines[0].stepNames[0][0]='X';assert(restored.importSchedule(conflict)==ImportResult::Conflict);
  confirmStep(restored,1005);feedAndContinue(restored,1006);assert(restored.view().screen==Screen::Result);
  restored.input(Intent::Confirm,{1006,true});closeSummary(restored,1006);
  const auto finalEvents=restored.state().eventCount;assert(restored.state().companion.experience[0]==2);
  restored.input(Intent::Next,{1008,true});assert(restored.view().screen==Screen::Catalog);
  restored.input(Intent::Next,{1008,true});restored.input(Intent::Confirm,{1008,true});assert(restored.view().screen==Screen::RewardCatalog);
  restored.input(Intent::Previous,{1008,true});assert(restored.view().item==content::kFoodCount);
  restored.input(Intent::Confirm,{1008,true});assert(restored.view().screen==Screen::Home);
  restored.input(Intent::Previous,{1008,true});restored.input(Intent::Confirm,{1008,true});assert(restored.view().screen==Screen::CharacterCatalog);
  restored.input(Intent::Next,{1008,true});assert(restored.view().item==1);
  restored.input(Intent::Confirm,{1008,true});assert(restored.view().screen==Screen::CharacterCatalog); // Locked.
  for (unsigned i=1;i<content::kCharacterEntryCount;++i) restored.input(Intent::Next,{1008,true});
  restored.input(Intent::Confirm,{1008,true});assert(restored.view().screen==Screen::Home);
  assert(restored.state().eventCount==finalEvents);
}
void boundaries() {
  Slots io;SnapshotStore storage(io);ProductController c(storage);active(c);
  c.input(Intent::Confirm,{1899,true});assert(c.view().screen==Screen::CompleteConfirm);
  c.input(Intent::Confirm,{1900,true});assert(c.view().screen==Screen::Routine); // Warning invalidates old confirmation.
  assert(c.state().routines[0].run.completedSteps==0);
  c.input(Intent::Confirm,{1999,true});c.input(Intent::Confirm,{2000,true});assert(c.view().screen==Screen::Result);
  assert(c.state().routines[0].run.phase==domain::Phase::Expired&&c.state().routines[0].run.earnedRewards==0);
  Slots io2;SnapshotStore st2(io2);ProductController d(st2);active(d);
  d.input(Intent::Confirm,{1002,false});assert(d.view().screen==Screen::ClockRequired);
  d.tick({1002,true});assert(d.view().screen==Screen::Routine);
  confirmStep(d,1003);feedAndContinue(d,1004);d.input(Intent::Previous,{1005,true});d.input(Intent::Confirm,{1005,true});
  d.input(Intent::Next,{1005,true});d.input(Intent::Confirm,{1005,true});
  assert(d.state().routines[0].run.phase==domain::Phase::Abandoned&&d.state().routines[0].run.earnedRewards==1);
  assert(d.sleepSeconds({1006,true})==300);
  d.tick({1000,true});assert(d.view().screen==Screen::ClockRequired);
  assert(c.sleepSeconds({0,false})==30);
  Slots io3;SnapshotStore st3(io3);ProductController e(st3);assert(e.begin()==LoadResult::Empty);
  assert(e.importSchedule(schedule())==ImportResult::Accepted);
  assert(e.sleepSeconds({990,true})==10);e.tick({1000,true});assert(e.sleepSeconds({1899,true})==1);
}
void failedStorage() {
  Slots io;SnapshotStore storage(io);ProductController c(storage);active(c);
  c.input(Intent::Confirm,{1002,true});const auto before=c.state().generation;
  io.fail=true;c.input(Intent::Confirm,{1002,true});
  assert(c.faulted()&&c.state().generation==before&&c.state().routines[0].run.completedSteps==0);
  io.fail=false;SnapshotStore reboot(io);ProductState state;
  assert(reboot.load(state)==LoadResult::Recovered);assert(state.generation==before);state.generation++;assert(!reboot.save(state));
  Slots io2;SnapshotStore st2(io2);ProductController d(st2);active(d);d.input(Intent::Confirm,{1002,true});
  io2.uncertain=true;d.input(Intent::Confirm,{1002,true});assert(d.faulted());
  io2.uncertain=false;SnapshotStore reboot2(io2);ProductController recovered(reboot2);assert(recovered.begin()==LoadResult::Loaded);
  assert(recovered.view().screen==Screen::StepSaved&&recovered.state().routines[0].run.completedSteps==1);
  recovered.input(Intent::Confirm,{1003,true});assert(recovered.state().routines[0].run.completedSteps==1);
  io2.slots[0].clear();io2.slots[1].clear();
  SnapshotStore lost(io2);assert(lost.load(state)==LoadResult::Corrupt); // No silent factory reset.
}
void codec() {
  ProductState s;s.generation=99;s.scheduleRevision=42;s.count=1;s.routines=schedule().routines;
  uint8_t data[kSnapshotBytes];const size_t n=encodeState(s,data,sizeof(data));assert(n);
  ProductState out;assert(decodeState(data,n,out)==DecodeResult::Ok);assert(out.generation==99&&out.routines[0].title==s.routines[0].title);
  for(size_t i=0;i<n;++i){assert(decodeState(data,i,out)==DecodeResult::Invalid);data[i]^=1;assert(decodeState(data,n,out)==DecodeResult::Invalid);data[i]^=1;}
  data[4]=5;const auto crc=crc32(data,n-4);for(int i=0;i<4;++i)data[n-4+i]=uint8_t(crc>>(8*i));
  assert(decodeState(data,n,out)==DecodeResult::Unsupported);
  s.routines[0].title.fill('x');assert(!encodeState(s,data,sizeof(data)));
  assert(!validText("\xf0\x80\x80\x80"));assert(!validText("\xed\xa0\x80"));assert(!validText("\xc0\x80"));
  ScheduleTransfer transfer;assert(transfer.receive("load begin 29",0)==TransferResult::Begun);
  assert(transfer.receive("load data 1 00",1)==TransferResult::Invalid);assert(!transfer.active());
  transfer.receive("load begin 29",0);assert(transfer.receive("load commit",1)==TransferResult::Invalid);
  transfer.receive("load begin 29",UINT32_MAX-100);transfer.tick(30000);assert(!transfer.active());
  transfer.receive("load begin 29",0);assert(transfer.receive("load data 0 zz",1)==TransferResult::Invalid);
  transfer.receive("load begin 29",0);std::string command="load data 0 "+std::string(58,'0');
  assert(transfer.receive(command.c_str(),1)==TransferResult::Chunk);assert(transfer.receive("load commit",2)==TransferResult::Complete);
  assert(transfer.size()==29);assert(decodeState(transfer.bytes(),transfer.size(),out)==DecodeResult::Invalid);
  // Exhaustive implementation capacity fits the persistent envelope.
  s={};s.generation=1;s.scheduleRevision=1;s.count=kMaxRuns;
  for(size_t i=0;i<kMaxRuns;++i){auto& r=s.routines[i];domain::RunSpec spec;spec.id=i+1;spec.startsAt=1000;spec.endsAt=2000;spec.stepCount=16;
    for(unsigned j=0;j<16;++j)spec.steps[j]=j+1;
    assert(domain::createRun(spec,{domain::DeadlineRule::DeadlineWins,0},r.run)==domain::SpecError::None);
    r.title.fill('A');r.title.back()=0;for(auto& t:r.stepNames){t.fill('B');t.back()=0;}
  }
  const auto scheduledBytes=encodeState(s,data,sizeof(data));assert(scheduledBytes&&scheduledBytes<kSnapshotBytes-256*21);
  for(size_t i=0;i<kMaxRuns;++i){auto& r=s.routines[i];
    const auto advance=[&](const domain::Transition& t){assert(t.changed());r.run=t.next;for(size_t e=0;e<t.eventCount;++e)s.events[s.eventCount++]=t.events[e];};
    advance(domain::advanceTime(r.run,{1000,true}));
    advance(domain::dispatch(r.run,{domain::Action::OpenLetter,r.run.spec.id,r.run.version,0},{1001,true}));
    for(uint8_t step=0;step<16;++step)advance(domain::dispatch(r.run,{domain::Action::CompleteStep,r.run.spec.id,r.run.version,r.run.spec.steps[step]},{1010+step,true}));
    r.food.fill(1);
  }
  const auto completedBytes=encodeState(s,data,sizeof(data));assert(completedBytes&&completedBytes<=kSnapshotBytes);
  assert(decodeState(data,completedBytes,out)==DecodeResult::Ok&&out.eventCount==s.eventCount&&out.count==kMaxRuns);
}
void dailyProgress() {
  ProductState state;
  state.count = 4;
  // Deliberately unsorted; the KST date changes at 15:00 UTC, not midnight UTC.
  const int64_t starts[] = {15*3600, 14*3600, 86400, 15*3600-1};
  for (size_t i=0;i<4;++i) {
    state.routines[i].run.spec.id=i+1;
    state.routines[i].run.spec.startsAt=starts[i];
  }
  auto p=dailyRoutineProgress(state,0); assert(p.ordinal==1&&p.total==2);
  p=dailyRoutineProgress(state,2); assert(p.ordinal==2&&p.total==2);
  p=dailyRoutineProgress(state,3); assert(p.ordinal==2&&p.total==2);
  p=dailyRoutineProgress(state,4); assert(p.ordinal==0&&p.total==0);
}
void seed(Slots& io, ProductState state) {
  state.generation=1;
  std::vector<uint8_t> bytes(kSnapshotBytes);
  const auto size=encodeState(state,bytes.data(),bytes.size()); assert(size);
  bytes.resize(size);io.slots[0]=bytes;io.slots[1].clear();io.sealed=1;
}
void feedingAndEvolution() {
  domain::Companion companion;
  for (unsigned feed=1;feed<=150;++feed) {
    assert(domain::feedCompanion(companion,content::kCharacterCount));
    const auto g=domain::growthView(companion.experience[0]);
    assert(domain::collectedStage(companion,0)==g.stage && companion.highestStage[0]==g.stage);
    if (feed==39) assert(g.stage==1&&g.progress==39&&g.target==40);
    if (feed==40) assert(g.stage==2&&g.progress==0&&g.target==50);
    if (feed==90) assert(g.stage==3&&g.progress==0&&g.target==60);
    if (feed==40||feed==90||feed==150) {
      assert(!domain::feedCompanion(companion,content::kCharacterCount));
      companion.notice=domain::GrowthNotice::None;
    }
  }
  assert(companion.experience[0]==150&&companion.offered==1);
  assert(!domain::feedCompanion(companion,content::kCharacterCount));
  for (unsigned before : {39u,89u,149u}) {
    Slots source;SnapshotStore store(source);ProductController c(store);active(c,schedule(1));confirmStep(c,1002);
    auto saved=c.state();saved.companion.experience[0]=uint16_t(before);
    const auto food=saved.routines[0].food[0];assert(food&&food<=content::kFoodCount);
    Slots io;seed(io,saved);SnapshotStore restoredStore(io);ProductController restored(restoredStore);
    assert(restored.begin()==LoadResult::Loaded&&restored.state().routines[0].food[0]==food);
    restored.input(Intent::Confirm,{1003,true});assert(restored.view().screen==Screen::Feed);
    restored.input(Intent::Confirm,{1003,true});assert(restored.view().screen==Screen::Evolution);
    assert(restored.state().companion.experience[0]==before+1);
    SnapshotStore rebootStore(io);ProductController reboot(rebootStore);
    assert(reboot.begin()==LoadResult::Loaded&&reboot.view().screen==Screen::Evolution);
    assert(foodInventory(reboot.state(),food).acquired==1&&foodInventory(reboot.state(),food).available==0);
    reboot.input(Intent::Confirm,{1004,true});
    if (before==149) {
      assert(reboot.view().screen==Screen::NewCharacter&&!domain::owns(reboot.state().companion,1));
      reboot.input(Intent::Next,{1004,true});reboot.input(Intent::Confirm,{1004,true});
      assert(domain::owns(reboot.state().companion,1)&&reboot.state().companion.selected==1);
      assert(reboot.state().companion.experience[0]==150&&reboot.state().companion.experience[1]==0);
    } else assert(reboot.view().screen==Screen::Growth);
    reboot.input(Intent::Confirm,{1005,true});assert(reboot.view().screen==Screen::Result);
    assert(reboot.state().companion.experience[0]==before+1); // No replay credit.
  }
  Slots io;SnapshotStore store(io);ProductController c(store);active(c);confirmStep(c,1002);
  c.input(Intent::Confirm,{1002,true});assert(c.view().screen==Screen::Feed);
  io.fail=true;c.input(Intent::Confirm,{1003,true});
  assert(c.faulted()&&c.state().companion.experience[0]==0&&c.state().routines[0].consumedRewards==0);
}
void characterSelection() {
  ProductState state;state.companion.owned=3;state.companion.experience[0]=150;state.companion.experience[1]=7;
  Slots io;seed(io,state);SnapshotStore store(io);ProductController c(store);assert(c.begin()==LoadResult::Loaded);
  c.input(Intent::Next,{1000,true});c.input(Intent::Confirm,{1000,true});
  for (unsigned i=0;i<3;++i) c.input(Intent::Next,{1000,true});
  c.input(Intent::Confirm,{1000,true});
  assert(c.view().screen==Screen::CharacterConfirm&&c.view().selected==0&&c.view().item==3);
  const auto generation=c.state().generation;
  c.input(Intent::Confirm,{1000,true}); // Cancel retains the card, no save.
  assert(c.view().screen==Screen::CharacterCatalog&&c.view().item==3&&c.state().generation==generation);
  c.input(Intent::Confirm,{1000,true});
  c.input(Intent::Next,{1000,true});c.input(Intent::Confirm,{1000,true});
  assert(c.state().companion.selected==1&&c.state().companion.experience[1]==7);
  c.input(Intent::Next,{1000,true});c.input(Intent::Confirm,{1000,true});
  for (unsigned i=0;i<content::kCharacterEntryCount-1;++i) c.input(Intent::Next,{1000,true});
  c.input(Intent::Confirm,{1000,true}); // Collected adult sheep, in other species group.
  c.input(Intent::Next,{1000,true});c.input(Intent::Confirm,{1000,true});
  assert(c.state().companion.selected==0&&c.state().companion.experience[0]==0&&c.state().companion.owned==3);
  assert(c.state().companion.highestStage[0]==3);
  SnapshotStore againStore(io);ProductController again(againStore);
  assert(again.begin()==LoadResult::Loaded&&again.state().companion.experience[0]==0);
  assert(domain::collected(again.state().companion,0,3));
  again.input(Intent::Next,{1000,true});again.input(Intent::Confirm,{1000,true});
  again.input(Intent::Next,{1000,true});again.input(Intent::Next,{1000,true});again.input(Intent::Confirm,{1000,true});
  assert(again.view().screen==Screen::CharacterConfirm); // Adult card remains available after restart.
  io.fail=true;again.input(Intent::Next,{1000,true});again.input(Intent::Confirm,{1000,true});
  assert(again.faulted()&&again.state().companion.highestStage[0]==3);
}
void collectionMigration() {
  std::ifstream f("tests/fixtures/state-v2-collection.bin",std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
  assert(bytes.size()>6 && bytes[4]==2);
  ProductState state;
  assert(decodeState(bytes.data(),bytes.size(),state)==DecodeResult::Ok);
  assert(state.generation==7&&state.companion.owned==3&&state.companion.highestStage[0]==3&&state.companion.highestStage[1]==1);
  assert(state.randomState==123456&&state.summarySeenDay==123&&state.companion.experience[1]==7);
  state.companion.experience[0]=0;
  std::vector<uint8_t> encoded(kSnapshotBytes);
  const auto size=encodeState(state,encoded.data(),encoded.size());assert(size&&encoded[4]==4);
  ProductState out;
  assert(decodeState(encoded.data(),size,out)==DecodeResult::Ok&&domain::collected(out.companion,0,3));
  Slots io;io.slots[0]=bytes;io.sealed=7;
  SnapshotStore oldStore(io);ProductState loaded;
  assert(oldStore.load(loaded)==LoadResult::Loaded&&loaded.generation==7);
  loaded.generation=8;loaded.companion.experience[0]=0;
  assert(oldStore.save(loaded)&&io.slots[0][4]==2&&io.slots[1][4]==4);
  SnapshotStore mixedStore(io);
  assert(mixedStore.load(out)==LoadResult::Loaded&&out.generation==8&&out.companion.experience[0]==0);
  assert(domain::collected(out.companion,0,3)&&out.randomState==123456&&out.summarySeenDay==123);
  state.companion.highestStage[0]=4;assert(!validProduct(state));
  state.companion.highestStage[0]=3;state.companion.highestStage[2]=1;assert(!validProduct(state));
  for (uint8_t selected=0; selected<content::kCharacterCount; ++selected) for (uint8_t index=0; index<content::kCharacterEntryCount; ++index) {
    const auto entry=content::characterEntry(index,selected);
    assert(entry.character==(selected+index/3)%content::kCharacterCount && entry.stage==index%3+1);
  }
  assert(!content::characterEntry(content::kCharacterEntryCount,0).stage);
  domain::Companion c;
  for (uint16_t xp : {0,39,40,89,90,150}) {
    c.experience[0]=xp;
    assert(domain::collectedStage(c,0)==(xp<40?1:xp<90?2:3));
    assert(!domain::collected(c,1,1));
  }
}
void summaryRollover() {
  auto s=schedule(1);s.count=2;s.routines[1]=s.routines[0];
  auto& tomorrow=s.routines[1].run;tomorrow.spec.id=2;tomorrow.spec.startsAt=54000;tomorrow.spec.endsAt=55000;
  Slots io;SnapshotStore store(io);ProductController c(store);active(c,s);confirmStep(c,1002);feedAndContinue(c,1003);
  c.input(Intent::Confirm,{1004,true});assert(c.view().screen==Screen::SummaryIntro);
  assert(c.view().summary.runs==1&&c.view().summary.steps==1&&c.view().summary.completedSteps==1);
  c.input(Intent::Confirm,{1004,true});c.input(Intent::Confirm,{1004,true});assert(c.view().screen==Screen::SummaryEnd);
  c.input(Intent::Confirm,{54000,true}); // Old summary press must not open tomorrow's letter.
  assert(c.view().screen==Screen::Letter&&c.state().routines[1].run.phase==domain::Phase::LetterPending);
  assert(c.view().summary.runs==1&&c.view().summary.completedSteps==0);
  assert(c.state().routines[0].run.completedSteps==1&&c.state().companion.experience[0]==1);
  auto state=c.state();state.routines[1].run.phase=domain::Phase::Abandoned;
  const auto day=summarizeDay(state,1);assert(day.finished()&&day.abandonedRuns==1&&day.completedSteps==0);
}
void legacyMigration() {
  std::ifstream f("tests/fixtures/state-v1-reward.bin",std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
  ProductState before;
  assert(decodeState(bytes.data(),bytes.size(),before)==DecodeResult::Ok);
  assert(before.generation==1&&before.routines[0].run.completedSteps==1&&before.routines[0].acknowledgedRewards==1);
  std::vector<uint8_t> current(kSnapshotBytes);
  assert(!encodeState(before,current.data(),current.size())); // v2 cannot defer a draw until a later boot.
  before.routines[0].food[0]=5;
  const auto size=encodeState(before,current.data(),current.size());assert(size);
  std::vector<uint8_t> other(kSnapshotBytes);before.routines[0].food[0]=6;
  assert(encodeState(before,other.data(),other.size())==size);
  size_t changes=0;
  for(size_t i=0;i<size-4;++i) if(current[i]!=other[i]) { current[i]=0;++changes; }
  assert(changes==1);
  const auto checksum=crc32(current.data(),size-4);
  for(unsigned i=0;i<4;++i) current[size-4+i]=uint8_t(checksum>>(8*i));
  ProductState invalid;
  assert(decodeState(current.data(),size,invalid)==DecodeResult::Invalid); // Valid CRC, missing v2 food.
  Slots io;io.slots[0]=bytes;io.sealed=1;SnapshotStore store(io);ProductController c(store);
  assert(c.begin()==LoadResult::Loaded&&c.view().screen==Screen::Feed);
  const auto food=c.state().routines[0].food[0];
  assert(food&&c.state().generation==2&&c.state().routines[0].consumedRewards==0);
  c.input(Intent::Confirm,{1004,true});assert(c.view().screen==Screen::Growth&&c.state().companion.experience[0]==1);
  SnapshotStore rebootStore(io);ProductController reboot(rebootStore);
  assert(reboot.begin()==LoadResult::Loaded&&reboot.view().screen==Screen::Routine);
  assert(reboot.state().routines[0].food[0]==food&&reboot.state().companion.experience[0]==1);
}
void rewardLedger() {
  ProductState state;
  state.count=2;
  state.routines=schedule().routines;
  state.routines[1]=state.routines[0];
  uint32_t random=123;
  for (auto index : {0,1}) {
    auto& record=state.routines[index];
    auto& run=record.run;
    run=domain::advanceTime(run,{1000,true}).next;
    run=domain::dispatch(run,{domain::Action::OpenLetter,run.spec.id,run.version,11},{1001,true}).next;
    for(auto step : {11u,12u})
      run=domain::dispatch(run,{domain::Action::CompleteStep,run.spec.id,run.version,step},{1002,true}).next;
    assert(domain::valid(run)&&run.completedSteps==2);
    assert(assignEarnedFood(record,random));
    const auto assigned=record.food;
    const auto afterDraw=random;
    assert(!assignEarnedFood(record,random)&&record.food==assigned&&random==afterDraw);
  }
  auto& first=state.routines[0];
  first.food[0]=5;first.food[1]=9;first.acknowledgedRewards=3;first.consumedRewards=1;
  state.routines[1].food[0]=5;state.routines[1].food[1]=9;
  assert(pendingRewardStep(first)==1); // Reading the receipt does not consume the second reward.
  assert(pendingRewardStep(first,5)==-1); // The consumed blueberry must be skipped.
  auto reward=findPendingFood(state,5);assert(reward.routine==1&&reward.step==0);
  reward=findPendingFood(state,9);assert(reward.routine==0&&reward.step==1);
  assert(!findPendingFood(state,31).found());
}
void render(const char* directory) {
  Slots io;SnapshotStore storage(io);ProductController c(storage);active(c);confirmStep(c,1002);
  std::vector<uint16_t> pixels(240*320+2,0x1234);
  const Screen screens[]={Screen::Letter,Screen::Routine,Screen::CompleteConfirm,Screen::AbandonConfirm,Screen::Catalog,Screen::StepSaved,Screen::Result,Screen::ClockRequired,Screen::StorageError,Screen::Feed,Screen::Growth,Screen::Evolution,Screen::NewCharacter,Screen::CharacterCatalog,Screen::CharacterConfirm,Screen::RewardCatalog,Screen::FoodDetail,Screen::SummaryIntro,Screen::Summary,Screen::SummaryEnd,Screen::Home};
  for(size_t i=0;i<sizeof(screens)/sizeof(*screens);++i){
    // Visual fixtures run only on the PC; never seed artificial progress on a device.
    auto state=c.state();state.routines[0].food[0]=5;
    auto view=c.view();view.screen=screens[i];view.rewardStep=0;
    view.summary=summarizeDay(state,0);
    if(view.screen==Screen::RewardCatalog || view.screen==Screen::FoodDetail) view.item=4;
    if(view.screen==Screen::Growth) state.companion.experience[0]=39;
    if(view.screen==Screen::Evolution) { state.companion.experience[0]=40;state.companion.notice=domain::GrowthNotice::Evolved; }
    if(view.screen==Screen::NewCharacter) { state.companion.experience[0]=150;state.companion.offered=1; }
    if(view.screen==Screen::CharacterConfirm) state.companion.experience[0]=150;
    if(view.screen==Screen::Home) { state={};view={}; }
    ui::renderProduct(pixels.data()+1,state,view,true,9*60+10);
    assert(pixels.front()==0x1234&&pixels.back()==0x1234);
    if(directory){std::ofstream f(std::string(directory)+"/product-"+std::to_string(i)+".ppm",std::ios::binary);f<<"P6\n240 320\n255\n";
      for(size_t j=1;j<=240*320;++j){const auto v=pixels[j];const char b[]={char((v>>11)*255/31),char(((v>>5)&63)*255/63),char((v&31)*255/31)};f.write(b,3);}
      assert(f.good());
    }
  }
}
void tenCharacterCollection() {
  ProductState state;
  // Unlock/finish every supplied species without inventing a species eleven.
  for(uint8_t id=0;id<content::kCharacterCount;++id){
    state.companion.selected=id;
    for(unsigned xp=0;xp<150;++xp){
      assert(domain::feedCompanion(state.companion,content::kCharacterCount));
      state.companion.notice=domain::GrowthNotice::None;
    }
    assert(domain::collected(state.companion,id,3));
    if(id+1<content::kCharacterCount){
      assert(state.companion.offered==id+1);
      state.companion.owned|=uint64_t(1)<<(id+1);
      state.companion.highestStage[id+1]=1;state.companion.offered=domain::kNoCharacter;
    }else assert(state.companion.offered==domain::kNoCharacter);
  }
  std::vector<uint8_t> bytes(kSnapshotBytes);
  const auto size=encodeState(state,bytes.data(),bytes.size());assert(size);
  ProductState decoded;assert(decodeState(bytes.data(),size,decoded)==DecodeResult::Ok);
  assert(decoded.companion.owned==1023);
  for(uint8_t id=0;id<10;++id)assert(domain::collected(decoded.companion,id,3));
  // A complete round trip through all 30 cards includes the explicit Home entry.
  Slots io;SnapshotStore store(io);ProductController c(store);assert(c.begin()==LoadResult::Empty);
  const domain::TimeSample time{1000,true};
  c.input(Intent::Confirm,time);assert(c.view().screen==Screen::Home); // User kept this behavior.
  c.input(Intent::Next,time);c.input(Intent::Confirm,time);
  for(unsigned i=0;i<30;++i){assert(c.view().screen==Screen::CharacterCatalog&&c.view().item==i);c.input(Intent::Next,time);}
  assert(c.view().item==30);c.input(Intent::Confirm,time);assert(c.view().screen==Screen::Home);
  c.input(Intent::Previous,time);c.input(Intent::Confirm,time);c.input(Intent::Previous,time);
  assert(c.view().item==30);c.input(Intent::Confirm,time);assert(c.view().screen==Screen::Home);
}
int main(int argc,char** argv){
  loadTestAssets();
  if(argc==3&&!std::strcmp(argv[1],"--decode")){
    std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),{});ProductState state;
    assert(decodeState(data.data(),data.size(),state)==DecodeResult::Ok&&state.count&&state.generation==0&&state.eventCount==0);
    Slots io;SnapshotStore store(io);ProductController c(store);assert(c.begin()==LoadResult::Empty);
    Schedule s;s.revision=state.scheduleRevision;s.count=state.count;s.routines=state.routines;
    assert(c.importSchedule(s)==ImportResult::Accepted);assert(c.importSchedule(s)==ImportResult::Duplicate);
    std::puts("Host JSON -> binary -> firmware codec -> schedule import passed");return 0;
  }
  workflow();boundaries();failedStorage();codec();dailyProgress();feedingAndEvolution();characterSelection();collectionMigration();summaryRollover();legacyMigration();rewardLedger();tenCharacterCollection();
  render(argc>1?argv[1]:nullptr);std::puts("Product workflow, feeding/evolution, day rollover, persistence, codec, transfer and renderer tests passed");
}
