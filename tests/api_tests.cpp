#include "../firmware/RoutineDevice/src/application/DeviceApi.h"
#include "../firmware/RoutineDevice/src/application/ProductController.h"
#include "../firmware/RoutineDevice/src/application/StateCodec.h"
#include "../firmware/RoutineDevice/src/application/DailySummary.h"
#include "../firmware/RoutineDevice/src/vendor/ArduinoJson.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#include <memory>
using namespace routine;
using namespace routine::application;
namespace {
const char sample[]=R"({"success":true,"data":{"serverTime":"2026-10-03T22:32:00Z","accepted":0,"routines":[{"date":"2026-10-04","bigRoutines":[{"bigRoutineId":300,"seriesId":"repeat-family","title":"아침 준비","startTime":"07:30","endTime":"08:30","sortOrder":1,"smallRoutines":[{"smallRoutineId":903,"title":"나가기","sortOrder":3,"status":"PENDING","completedAt":null},{"smallRoutineId":901,"title":"세수하기","sortOrder":1,"status":"PENDING","completedAt":null},{"smallRoutineId":902,"title":"양치하기","sortOrder":2,"status":"DONE","completedAt":"2026-10-03T22:31:00Z"}]}]}]},"error":null})";
struct Store:ProductStore {
  ProductState saved{};bool exists=false,fail=false,archiveFail=false;unsigned archives=0;
  LoadResult load(ProductState& s)override{if(exists)s=saved;return exists?LoadResult::Loaded:LoadResult::Empty;}
  bool save(const ProductState& s)override{if(fail)return false;assert(validProduct(s));saved=s;exists=true;return true;}
  bool archive(const ProductState& s)override{if(archiveFail)return false;assert(validProduct(s));++archives;return true;}
};
int64_t timestamp(const char* s){int64_t v=0;assert(api::parseTimestamp(s,v));return v;}
api::SyncRequest dates(){api::SyncRequest r;r.battery=70;r.dateCount=1;assert(api::parseDate("2026-10-04",r.dates[0]));return r;}
void parsing(){
  int64_t value=0;assert(api::parseDate("2024-02-29",value));assert(api::formatDate(value)=="2024-02-29");assert(!api::parseDate("2025-02-29",value));assert(!api::parseDate("2026-13-01",value));
  assert(api::parseTimestamp("2026-10-03T22:31:00.000Z",value));assert(api::formatTimestamp(value)=="2026-10-03T22:31:00Z");assert(!api::parseTimestamp("2026-10-03T24:31:00Z",value));
  api::SyncResponse response;assert(api::parseSync(sample,sizeof(sample)-1,response)==api::Result::Ok);
  const auto& r=response.routines[0].record;assert(response.count==1&&r.run.spec.id==300&&r.run.spec.steps[0]==901&&r.run.spec.steps[2]==903);
  assert(r.run.remoteCompleted==2&&r.run.earnedRewards==0&&r.run.completedSteps==1&&domain::nextStep(r.run)==0);
  assert(r.run.spec.startsAt==timestamp("2026-10-03T22:30:00Z"));
  const char* bads[]={"{","{\"success\":false,\"data\":{}}","[]"};for(auto s:bads)assert(api::parseSync(s,std::strlen(s),response)!=api::Result::Ok);
  JsonDocument doc;assert(!deserializeJson(doc,sample));auto small=doc["data"]["routines"][0]["bigRoutines"][0]["smallRoutines"][0];
  small["smallRoutineId"]=uint64_t(UINT32_MAX)+1;std::string json;serializeJson(doc,json);assert(api::parseSync(json.data(),json.size(),response)==api::Result::InvalidField);
  small["smallRoutineId"]=901;json.clear();serializeJson(doc,json);assert(api::parseSync(json.data(),json.size(),response)==api::Result::InvalidField);
  small["smallRoutineId"]=903;small["title"]=std::string(100,'x');json.clear();serializeJson(doc,json);assert(api::parseSync(json.data(),json.size(),response)==api::Result::InvalidField);
  assert(api::parseSync(sample,api::kMaxJsonBytes+1,response)==api::Result::Capacity);
  api::ClaimResponse claim;const char* c=R"({"success":true,"data":{"deviceId":55,"deviceAccessUuid":"7c9e4d21-8b3a-4f60-a1d5-2e8c0b7f3a94"},"error":null})";
  assert(api::parseClaim(c,std::strlen(c),claim)==api::Result::Ok&&claim.deviceId==55);
  std::string body;assert(api::makeClaim("0482913057","YESO-001","0.7.0",body));assert(body.find("\"0482913057\"")!=std::string::npos);assert(!api::makeClaim("482913057","x","x",body));
  auto request=dates();request.battery=-1;assert(!api::makeSync(request,"x",body));request.battery=70;assert(api::makeSync(request,"0.7.0",body));assert(body.find("\"completions\":[]")!=std::string::npos);
}
void workflow(){
  auto memory=std::make_unique<Store>();auto controller=std::make_unique<ProductController>(*memory);auto& c=*controller;assert(c.begin()==LoadResult::Empty);
  auto response=std::make_unique<api::SyncResponse>();assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);auto request=dates();const int64_t now=response->serverTime;
  c.tick({now,true});assert(c.applySync(*response,request)==api::Result::Ok);const auto initial=c.state().generation;
  assert(c.applySync(*response,request)==api::Result::Ok&&c.state().generation==initial);
  c.tick({now,true});assert(c.view().screen==Screen::Letter);c.input(input::Intent::Confirm,{now,true});assert(c.view().screen==Screen::Routine);
  c.input(input::Intent::Confirm,{now+1,true});c.input(input::Intent::Confirm,{now+2,true});assert(c.state().routines[0].run.earnedRewards==1&&c.state().routines[0].run.completedSteps==2);
  assert(api::prepareSync(c.state(),now+3,71,request));assert(request.completionCount==1&&request.completions[0].step==901&&request.completions[0].at==now+2);
  response->accepted=1;response->serverTime=now+3;
  assert(c.applySync(*response,request)==api::Result::Ok&&c.state().routines[0].syncedRewards==0); // Count alone never acknowledges.
  auto& remote=response->routines[0];remote.record.run.remoteCompleted=3;remote.record.run.completedSteps=2;remote.completedAt[0]=now+1;
  assert(c.applySync(*response,request)==api::Result::Ok&&c.state().routines[0].syncedRewards==0); // Wrong timestamp.
  remote.completedAt[0]=now+2;assert(c.applySync(*response,request)==api::Result::Ok&&c.state().routines[0].syncedRewards==1);
  const auto food=c.state().routines[0].food[0];assert(food&&c.state().routines[0].food[1]==0);
  c.input(input::Intent::Confirm,{now+4,true});c.input(input::Intent::Confirm,{now+5,true});c.input(input::Intent::Confirm,{now+6,true});
  assert(c.view().screen==Screen::Routine&&c.view().expectedStep==903&&c.state().companion.experience[0]==1);
  c.input(input::Intent::Confirm,{now+7,true});c.input(input::Intent::Confirm,{now+8,true});
  assert(c.state().routines[0].run.earnedRewards==5&&c.state().routines[0].run.phase==domain::Phase::Completed);
  c.input(input::Intent::Confirm,{now+9,true});c.input(input::Intent::Confirm,{now+10,true});c.input(input::Intent::Confirm,{now+11,true});c.input(input::Intent::Confirm,{now+12,true});
  assert(c.state().routines[0].resultSeen&&c.state().companion.experience[0]==2);
  assert(api::prepareSync(c.state(),now+13,71,request)&&request.completionCount==1&&request.completions[0].step==903);
  remote.record.run.remoteCompleted=7;remote.record.run.completedSteps=3;remote.record.run.phase=domain::Phase::Completed;remote.record.run.version=1;remote.record.run.lastTransitionAt=now+13;remote.record.resultSeen=true;remote.completedAt[2]=now+8;response->serverTime=now+13;
  assert(c.applySync(*response,request)==api::Result::Ok&&c.state().routines[0].syncedRewards==5);
  std::vector<uint8_t> bytes(kSnapshotBytes);const auto length=encodeState(c.state(),bytes.data(),bytes.size());assert(length&&bytes[4]==4);
  auto decoded=std::make_unique<ProductState>();assert(decodeState(bytes.data(),length,*decoded)==DecodeResult::Ok&&decoded->routines[0].run.remoteCompleted==2&&decoded->routines[0].syncedRewards==5);
  const auto before=foodInventory(c.state(),food).acquired;
  memory->archiveFail=true;assert(!c.retireAcknowledgedDays(request.dates[0]+3)&&c.state().count==1);
  memory->archiveFail=false;assert(c.retireAcknowledgedDays(request.dates[0]+3)&&c.state().count==0&&memory->archives==1);
  assert(foodInventory(c.state(),food).acquired==before&&c.state().companion.experience[0]==2);
  assert(c.applySync(*response,request)==api::Result::Ok&&c.state().count==0); // Retired day cannot resurrect food.
  ProductController reboot(*memory);assert(reboot.begin()==LoadResult::Loaded&&reboot.state().count==0&&foodInventory(reboot.state(),food).acquired==before);
}
void changesAndFailures(){
  auto store=std::make_unique<Store>();auto c=std::make_unique<ProductController>(*store);c->begin();auto response=std::make_unique<api::SyncResponse>();assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);
  auto request=dates();const int64_t now=response->serverTime;c->tick({now,true});assert(c->applySync(*response,request)==api::Result::Ok);c->tick({now,true});
  auto old=c->state().generation;response->routines[0].record.run.spec.steps[0]=999;
  assert(c->applySync(*response,request)==api::Result::Conflict&&c->state().generation==old);
  assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);response->count=0;
  assert(c->applySync(*response,request)==api::Result::Ok&&c->state().count==1); // Omission is not a tombstone.
  assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);auto& r=response->routines[0].record;r.run.spec.id=400;for(auto& id:r.run.spec.steps)if(id)id+=100;r.run.spec.startsAt+=86400;r.run.spec.endsAt+=86400;
  response->dates[0]++;request.dates[0]++;store->fail=true;
  assert(c->applySync(*response,request)==api::Result::StorageError&&c->state().generation==old&&c->state().count==1);
}
void strictBodies(){
  auto response=std::make_unique<api::SyncResponse>();
  for(const auto& suffix:{std::string("{}"),std::string("garbage"),std::string(1,'\0')}){
    const auto json=std::string(sample)+suffix;
    assert(api::parseSync(json.data(),json.size(),*response)==api::Result::InvalidJson);
  }
  const auto padded=std::string(sample)+" \t\r\n";
  assert(api::parseSync(padded.data(),padded.size(),*response)==api::Result::Ok);
  for(const auto& prefix:{std::string("PENDING"),std::string("2026-10-04"),std::string("07:30"),std::string("2026-10-03T22:32:00Z")}){
    std::string json=sample;const auto at=json.find('"'+prefix+'"');assert(at!=std::string::npos);
    json.insert(at+prefix.size()+1,"\\u0000tail");
    assert(api::parseSync(json.data(),json.size(),*response)==api::Result::InvalidField);
  }
  api::ClaimResponse claim;claim.deviceId=99;
  const char badUuid[]=R"({"success":true,"data":{"deviceId":55,"deviceAccessUuid":"7c9e4d21-8b3a-4f60-a1d5-2e8c0b7f3a94\u0000tail"},"error":null})";
  assert(api::parseClaim(badUuid,sizeof(badUuid)-1,claim)==api::Result::InvalidField&&claim.deviceId==99);
}
void navigationDuringSync(){
  auto store=std::make_unique<Store>();auto c=std::make_unique<ProductController>(*store);c->begin();
  auto response=std::make_unique<api::SyncResponse>();assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);
  auto& remote=response->routines[0];remote.record.run.remoteCompleted=0;remote.record.run.completedSteps=0;
  const int64_t now=remote.record.run.spec.startsAt-3600;response->serverTime=now;
  auto request=dates();c->tick({now,true});assert(c->applySync(*response,request)==api::Result::Ok);
  c->input(input::Intent::Next,{now,true});c->input(input::Intent::Next,{now,true});assert(c->view().screen==Screen::Catalog&&c->view().selected==1);
  remote.record.run.spec.id++;for(auto& step:remote.record.run.spec.steps)if(step)step+=100;
  remote.record.run.spec.startsAt+=86400;remote.record.run.spec.endsAt+=86400;response->dates[0]++;request.dates[0]++;
  assert(c->applySync(*response,request)==api::Result::Ok);
  assert(c->view().screen==Screen::Catalog&&c->view().selected==1); // Tomorrow arriving cannot close a chooser.
}
void acknowledgementDuringConfirmation(){
  auto store=std::make_unique<Store>();auto c=std::make_unique<ProductController>(*store);c->begin();
  auto response=std::make_unique<api::SyncResponse>();assert(api::parseSync(sample,sizeof(sample)-1,*response)==api::Result::Ok);
  auto request=dates();const auto now=response->serverTime;c->tick({now,true});assert(c->applySync(*response,request)==api::Result::Ok);c->tick({now,true});
  for(int seconds=0;seconds<=5;++seconds)c->input(input::Intent::Confirm,{now+seconds,true});
  assert(c->view().screen==Screen::Routine&&c->view().expectedStep==903);
  c->input(input::Intent::Confirm,{now+6,true});assert(c->view().screen==Screen::CompleteConfirm);
  assert(api::prepareSync(c->state(),now+7,50,request));
  auto& remote=response->routines[0];remote.record.run.remoteCompleted=3;remote.record.run.completedSteps=2;remote.completedAt[0]=now+2;response->serverTime=now+7;
  assert(c->applySync(*response,request)==api::Result::Ok&&c->state().routines[0].syncedRewards==1);
  assert(c->view().screen==Screen::CompleteConfirm&&c->view().expectedStep==903);
  // A genuine new remote completion must invalidate that confirmation.
  remote.record.run.remoteCompleted=7;remote.record.run.completedSteps=3;remote.record.run.phase=domain::Phase::Completed;remote.record.run.version=1;remote.record.run.lastTransitionAt=now+8;remote.record.resultSeen=true;remote.completedAt[2]=now+8;response->serverTime=now+8;
  assert(c->applySync(*response,request)==api::Result::Ok&&c->view().screen!=Screen::CompleteConfirm);
}
void archiveRemapsDisplayedRun(){
  auto store=std::make_unique<Store>();store->exists=true;auto& state=store->saved;state.generation=1;state.scheduleRevision=1;state.count=3;
  int64_t day;assert(api::parseDate("2026-10-05",day));const auto now=day*86400-9*3600+7*3600;
  for(size_t i=0;i<3;++i){auto& record=state.routines[i];record.apiManaged=true;
    domain::RunSpec spec;spec.id=100+i;spec.startsAt=now-(i==2?0:3-i)*86400;spec.endsAt=spec.startsAt+3600;spec.stepCount=1;spec.steps[0]=uint32_t(500+i);
    assert(domain::createRun(spec,{},record.run)==domain::SpecError::None);std::strcpy(record.title.data(),"루틴");std::strcpy(record.stepNames[0].data(),"할 일");
    if(i<2){record.run.remoteCompleted=1;record.run.completedSteps=1;record.run.phase=domain::Phase::Completed;record.run.version=1;record.run.lastTransitionAt=spec.startsAt+1;record.resultSeen=true;}
  }
  auto controller=std::make_unique<ProductController>(*store);assert(controller->begin()==LoadResult::Loaded);controller->tick({now,true});
  assert(controller->view().screen==Screen::Letter&&controller->view().routine==2);const auto version=controller->view().expectedVersion;
  assert(controller->retireAcknowledgedDays(day));
  assert(controller->state().count==1&&controller->view().screen==Screen::Letter&&controller->view().routine==0);
  assert(controller->state().routines[0].run.spec.id==102&&controller->view().expectedVersion==version&&controller->view().expectedStep==502);
}
void longRunningDevice(){
  auto store=std::make_unique<Store>();auto c=std::make_unique<ProductController>(*store);assert(c->begin()==LoadResult::Empty);
  auto response=std::make_unique<api::SyncResponse>();int64_t day;assert(api::parseDate("2026-10-04",day));
  for(unsigned n=0;n<160;++n){
    const auto today=day+n;const auto now=today*86400-9*3600+7*3600;
    c->tick({now,true});c->retireAcknowledgedDays(today);
    *response={};response->serverTime=now;response->dates[0]=today;response->dateCount=1;response->count=1;
    auto& remote=response->routines[0];auto& r=remote.record;r.apiManaged=true;
    domain::RunSpec spec;spec.id=1000+n;spec.startsAt=now;spec.endsAt=now+3600;spec.stepCount=1;spec.steps[0]=2000+n;
    assert(domain::createRun(spec,{},r.run)==domain::SpecError::None);std::strcpy(r.title.data(),"일일 루틴");std::strcpy(r.stepNames[0].data(),"완료할 항목");
    api::SyncRequest request;request.battery=50;request.dateCount=1;request.dates[0]=today;
    assert(c->applySync(*response,request)==api::Result::Ok);c->tick({now,true});assert(c->view().screen==Screen::Letter);
    for(int seconds=0;seconds<=4;++seconds)c->input(input::Intent::Confirm,{now+seconds,true});
    for(unsigned tries=0;tries<12&&c->view().screen!=Screen::Home;++tries){
      if(c->view().screen==Screen::NewCharacter)c->input(input::Intent::Next,{now+5+tries,true});
      c->input(input::Intent::Confirm,{now+5+tries,true});
    }
    assert(c->view().screen==Screen::Home);
    assert(api::prepareSync(c->state(),now+30,50,request)&&request.completionCount==1);
    remote.completedAt[0]=now+2;r.run.remoteCompleted=1;r.run.completedSteps=1;r.run.phase=domain::Phase::Completed;r.run.version=1;r.run.lastTransitionAt=now+30;r.resultSeen=true;response->serverTime=now+30;
    assert(c->applySync(*response,request)==api::Result::Ok);
    if(n%10==0){c=std::make_unique<ProductController>(*store);assert(c->begin()==LoadResult::Loaded);}
    assert(c->state().count<=3&&!c->faulted());
  }
  uint32_t foods=0;for(uint8_t i=1;i<=31;++i)foods+=foodInventory(c->state(),i).acquired;
  assert(foods==160&&c->state().companion.experience[0]==150&&c->state().companion.experience[1]==10);
  assert(c->state().companion.highestStage[0]==3&&store->archives==158);
}
}
int main(){parsing();strictBodies();workflow();changesAndFailures();navigationDuringSync();acknowledgementDuringConfirmation();archiveRemapsDisplayedRun();longRunningDevice();std::puts("PASS: strict API bodies, UTC/KST, claim, sparse DONE, local rewards, read-back ACK, navigation preservation, archive remapping and 160-day evolution/reboot/retirement");}
