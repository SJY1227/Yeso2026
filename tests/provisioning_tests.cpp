#include "../firmware/RoutineDevice/src/application/WifiProvisioning.h"
#include "../firmware/RoutineDevice/src/input/SystemGestures.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
using namespace routine;

std::string payload(const char* ssid="우리집 WiFi",const char* password="pw 12345",const char* code="0012345678",const char* url="http://192.168.0.10:8080"){
  return std::string("{\"ssid\":\"")+ssid+"\",\"password\":\""+password+"\",\"pairingCode\":\""+code+"\",\"serverUrl\":\""+url+"\"}";
}
void testProtocol(){
  using namespace provisioning;
  Config config;
  const auto json=payload();
  assert(parseConfig(json.data(),json.size(),config,true));
  assert(std::string(config.ssid)=="우리집 WiFi"&&std::string(config.pairingCode)=="0012345678");
  assert(!parseConfig(json.data(),json.size(),config,false));assert(!config.ssid[0]);
  for(unsigned chunk=1;chunk<=20;++chunk){
    Frame frame;const auto input=json+"\n";
    for(size_t at=0;at<input.size();at+=chunk){
      const size_t count=std::min<size_t>(chunk,input.size()-at);
      const auto result=frame.push(reinterpret_cast<const uint8_t*>(input.data()+at),count,unsigned(at));
      assert(result==(at+count==input.size()?FrameResult::Complete:FrameResult::Waiting));
    }
    assert(parseConfig(frame.bytes(),frame.size(),config,true));
    assert(std::string(config.ssid)=="우리집 WiFi"); // UTF-8 may span three writes.
    frame.clear();assert(frame.size()==0&&!frame.bytes()[0]);
  }
  unsigned badCase=0;
  for(const auto& bad:{payload("","password"),payload("test","short"),payload("test","password","123"),payload("test","password","012345678x"),json+"{}",payload("bad\\u0000ssid"),payload("bad\\ud800"),payload(std::string(33,'a').c_str()),payload("test","password","1234567890","http://example.com"),payload("test","password","1234567890","https://example.com/path")}){
    const bool parsed=parseConfig(bad.data(),bad.size(),config,true);if(parsed)std::fprintf(stderr,"Unexpected accepted invalid case %u\n",badCase);assert(!parsed);++badCase;
  }
  std::string invalidUtf8=payload("\xc0\xaf");assert(!parseConfig(invalidUtf8.data(),invalidUtf8.size(),config,true));
  auto numeric=json;const auto position=numeric.find("\"0012345678\"");numeric.replace(position,12,"1234567890");assert(!parseConfig(numeric.data(),numeric.size(),config,true));
  const auto maximum=payload(std::string(32,'s').c_str(),std::string(63,'p').c_str());assert(parseConfig(maximum.data(),maximum.size(),config,true));
  const auto open=payload("test","");assert(parseConfig(open.data(),open.size(),config,true));
  for(const char* url:{"http://10.0.0.1","http://172.16.0.1:80","http://172.31.255.254/","http://192.168.1.1:65535","https://example.org:443/"})assert(validServerUrl(url,true));
  for(const char* url:{"http://8.8.8.8","http://127.0.0.1","http://169.254.1.1","http://172.15.0.1","http://172.32.0.1","http://192.168.1.999","http://192.168.01.1","http://192.168.0.1:0","http://192.168.0.1:65536","http://192.168.0.1@evil.org","http://192.168.0.1\\evil","https://host?query","https://host#fragment","https://host:abc","https:///","https://host//"})assert(!validServerUrl(url,true));
  assert(!validServerUrl("http://192.168.0.1",false));assert(validServerUrl("https://api.example.com",false));
  Frame frame;
  const uint8_t partial[]={'{','"','s'};assert(frame.push(partial,3,UINT32_MAX-100)==FrameResult::Waiting);
  assert(!frame.expire(UINT32_MAX-100+9999));assert(frame.expire(UINT32_MAX-100+10000));assert(frame.size()==0&&!frame.bytes()[0]);
  const uint8_t tooMany[21]{};assert(frame.push(tooMany,21,0)==FrameResult::Invalid);
  const uint8_t badNul[]={'{',0,'\n'};assert(frame.push(badNul,3,0)==FrameResult::Invalid);
  const uint8_t two[]={'{','}','\n','{','}','\n'};assert(frame.push(two,sizeof(two),1)==FrameResult::Invalid);
  frame.clear();uint8_t full[20];std::fill_n(full,20,'x');
  for(unsigned i=0;i<51;++i)assert(frame.push(full,20,i)==FrameResult::Waiting);
  assert(frame.push(full,20,52)==FrameResult::Invalid);const uint8_t nl='\n';assert(frame.push(&nl,1,53)==FrameResult::Invalid);
  assert(frame.size()==0);frame.clear();
  // No partial prefix can leak from one BLE connection into another.
  frame.push(partial,3,100);frame.clear();const uint8_t valid[]={'{','}','\n'};
  assert(frame.push(valid,3,101)==FrameResult::Complete&&std::string(frame.bytes())=="{}");
}
struct FakeWifi:provisioning::WifiPort {
  bool startOk=true,ip=false,saveOk=true;unsigned tries=0,saves=0,restores=0;
  bool tryCandidate(const char*,const char*)override{++tries;return startOk;}
  bool hasCandidateIp()const override{return ip;}
  bool persistCandidate()override{++saves;return saveOk;}
  void restorePrevious()override{++restores;}
};
void testTransaction(){
  using namespace provisioning;Config config;Join join;FakeWifi port;
  assert(join.start(port,config,100));assert(!join.start(port,config,200));
  join.tick(port,1000);assert(port.saves==0&&join.state()==JoinState::Connecting);
  port.ip=true;join.tick(port,2000);assert(port.saves==1&&join.state()==JoinState::Committed&&port.restores==0);
  join.tick(port,90000);join.cancel(port);assert(port.saves==1&&port.restores==0); // Late cancel cannot undo a committed network.
  port={};assert(join.start(port,config,UINT32_MAX-100));join.tick(port,UINT32_MAX-100+30000);
  assert(join.state()==JoinState::Failed&&port.saves==0&&port.restores==1);
  port={};port.ip=true;port.saveOk=false;assert(join.start(port,config,0));join.tick(port,1);
  assert(join.state()==JoinState::Failed&&port.restores==1);
  port={};port.startOk=false;assert(!join.start(port,config,0));assert(port.restores==1);
  port={};assert(join.start(port,config,0));join.cancel(port);assert(port.restores==1&&join.state()==JoinState::Idle);
}
void testSystemHold(){
  using namespace input;SystemGestures input;input.begin(0,false,1);
  assert(input.update(10,true,1).intent==Intent::None);input.update(40,true,1);
  assert(input.update(840,true,1).intent==Intent::None); // A pending Confirm must not execute on the way to 5s.
  input.update(4000,false,1);input.update(4005,true,1); // Short bounce cannot restart the long hold.
  assert(input.update(5040,true,1).intent==Intent::None&&input.takeSetup());
  assert(!input.takeSetup());assert(input.update(6000,true,1).intent==Intent::None);
  input.update(6010,false,1);assert(input.update(6040,false,1).intent==Intent::None);
  assert(input.update(6500,false,1).intent==Intent::None);
  input.update(7000,true,1);input.update(7030,true,1);input.update(7830,true,1);
  input.update(7900,false,1);assert(input.update(7930,false,1).intent==Intent::Confirm);
  assert(input.update(8000,false,1).intent==Intent::None&&!input.takeSetup());
  input.begin(0,true,1);input.update(6000,true,1);assert(!input.takeSetup()); // Held wake/boot input is not a reset.
  input.update(6010,false,1);input.update(6040,false,1);
  input.update(7000,true,1);input.update(7030,true,1);input.update(7830,true,1);input.update(7850,true,2);
  input.update(7900,false,2);assert(input.update(7930,false,2).intent==Intent::None); // Deadline invalidates pending Confirm.
  input.begin(0,false,0);input.update(10,true,0);input.update(40,true,0);input.update(5040,true,0);assert(input.takeSetup()); // Close settings despite disabled product input.
}
int main(){testProtocol();testTransaction();testSystemHold();std::puts("PASS: BLE framing/UTF-8/limits, URL policy, Wi-Fi commit/rollback, exclusive setup hold");}
