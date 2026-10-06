#include "../firmware/RoutineDevice/src/input/Intent.h"
#include "../firmware/RoutineDevice/src/input/ContextualGestures.h"
#include "../firmware/RoutineDevice/src/application/InputSession.h"
#include "../firmware/RoutineDevice/src/application/ClockSync.h"
#include "../firmware/RoutineDevice/src/diagnostics/DeviceCommand.h"
#include "../firmware/RoutineDevice/src/diagnostics/LineBuffer.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace routine;
using input::Gesture;
void tap(input::Gestures& input, uint32_t start) {
  assert(input.update(start, true) == Gesture::None);
  assert(input.update(start + 30, true) == Gesture::None);
  assert(input.update(start + 80, false) == Gesture::None);
}
void testInput() {
  input::ContextualGestures tagged;tagged.begin(0,false,1);
  tagged.update(10,true,1);tagged.update(40,true,1);
  tagged.update(100,true,2); // Deadline replaces the screen during a hold.
  assert(tagged.update(1000,true,2).intent==input::Intent::None);
  tagged.update(1010,false,2);tagged.update(1040,false,2);
  assert(tagged.update(1400,false,2).intent==input::Intent::None);
  tagged.update(1500,true,2);tagged.update(1530,true,2);tagged.update(1580,false,2);tagged.update(1610,false,2);
  const auto captured=tagged.update(1910,false,2);assert(captured.intent==input::Intent::Next&&captured.context==2);
  tagged.update(2000,true,2);tagged.update(2030,true,2);tagged.update(2080,false,2);tagged.update(2110,false,2);
  tagged.update(2200,false,0);assert(tagged.update(2500,false,3).intent==input::Intent::None); // Pending tap cancelled during presentation.
  application::ProductState state;application::ProductView view;application::InputSession session;
  session.publish(state,view,false);const auto old=session.context();assert(session.accepts(old,state,view));
  session.publish(state,view,false);assert(session.context()==old); // Clock/frame redraws do not cancel input.
  view.screen=application::Screen::CompleteConfirm;assert(!session.accepts(old,state,view));
  session.publish(state,view,false);assert(session.context()!=old&&!session.accepts(old,state,view));
  const auto choice=session.context();view.selected^=1;assert(!session.accepts(choice,state,view));
  session.publish(state,view,true);const auto animated=session.context();assert(session.transient());
  session.publish(state,view,false);assert(!session.accepts(animated,state,view)); // A late skip cannot confirm.
  session.invalidate();assert(!session.accepts(session.context(),state,view));
  session.publish(state,view,false);state.count=1;state.routines[0].run.spec.id=42;view.routine=0;
  session.publish(state,view,false);const auto originalRun=session.context();state.routines[0].run.spec.id=43;
  assert(!session.accepts(originalRun,state,view)); // Same array slot, different dated occurrence.
  input::Gestures input;
  input.begin(0, false);
  tap(input, 10);
  assert(input.update(120, false) == Gesture::None);
  assert(input.update(419, false) == Gesture::None);
  assert(input.update(420, false) == Gesture::Tap);
  assert(!input.busy());
  input.begin(0, false);
  tap(input, 10); input.update(120, false);
  tap(input, 200);
  assert(input.update(310, false) == Gesture::DoubleTap);
  assert(input.update(2000, false) == Gesture::None);  // No delayed single after double.
  input.begin(0, false);
  tap(input, 10); input.update(120, false);
  input.update(200, true); input.update(230, true);
  assert(input.update(1029, true) == Gesture::None);
  assert(input.update(1030, true) == Gesture::Hold);  // Tap + hold is Confirm only.
  assert(input.update(3000, true) == Gesture::None);
  input.update(3010, false);
  assert(input.update(3040, false) == Gesture::None);
  assert(input.update(4000, false) == Gesture::None);
  input.begin(0, true);  // Wake while held, then release: no product operation.
  assert(input.update(3000, true) == Gesture::None);
  input.update(3010, false); assert(input.update(3040, false) == Gesture::None);
  assert(input.update(4000, false) == Gesture::None);
  tap(input, 4010); input.update(4120, false);
  assert(input.update(4420, false) == Gesture::Tap);
  input.begin(0, false);  // Second press bounce must not strand a pending tap.
  tap(input, 10); input.update(120, false);
  input.update(200, true); input.update(205, false);
  assert(input.update(420, false) == Gesture::Tap);
  input.begin(0, false);
  tap(input, 10); input.update(120, false);
  assert(input.update(420, true) == Gesture::Tap);  // Exactly at deadline: new sequence.
  input.update(450, true); input.update(500, false); input.update(530, false);
  assert(input.update(830, false) == Gesture::Tap);
  constexpr uint32_t wrap = UINT32_MAX - 100;
  input.begin(wrap, false); tap(input, wrap + 10); input.update(wrap + 120, false);
  assert(input.update(wrap + 420, false) == Gesture::Tap);
  using input::Control; using input::Intent;
  assert(input::mapSingleButton(Control::Primary, Gesture::Tap) == Intent::Next);
  assert(input::mapSingleButton(Control::Primary, Gesture::DoubleTap) == Intent::Previous);
  assert(input::mapSingleButton(Control::Primary, Gesture::Hold) == Intent::Confirm);
  assert(input::mapThreeButtons(Control::Primary, Gesture::Tap) == Intent::Previous);
  assert(input::mapThreeButtons(Control::Secondary, Gesture::Tap) == Intent::Confirm);
  assert(input::mapThreeButtons(Control::Tertiary, Gesture::Tap) == Intent::Next);
  assert(input::mapThreeButtons(Control::Primary, Gesture::Hold) == Intent::None);
}
void testClock() {
  using application::ClockSync; using application::SyncState;
  ClockSync clock;
  auto actions = clock.request(0, false, false);
  assert(!actions.connect && !actions.requestTime && clock.state() == SyncState::NeedsSetup);
  assert(!clock.known());
  actions = clock.request(1, true, false);
  assert(actions.connect && !actions.requestTime && clock.busy());
  actions = clock.tick(100, true, false);
  assert(actions.requestTime && clock.state() == SyncState::AwaitingTime);
  clock.tick(200, true, false);
  assert(!clock.known());  // A plausible existing clock is not a fresh NTP response.
  actions = clock.tick(300, true, true);
  assert(actions.stopTime && clock.known() && clock.fresh());
  for (unsigned wake = 0; wake < 3; ++wake) {
    assert(clock.suspend().stopTime);
    clock.tick(400, false, true);  // Ignore late response while suspended.
    assert(!clock.fresh() && clock.known());
    actions = clock.request(500, true, true);  // Already connected still requests NTP.
    assert(actions.requestTime && actions.stopTime && !clock.fresh());
    clock.tick(600, true, true);
    assert(clock.fresh());
  }
  actions = clock.tick(700, false, false);  // Router interruption while awake.
  assert(actions.connect && clock.known() && !clock.fresh());
  actions = clock.tick(15700, false, false);
  assert(actions.stopTime && clock.state() == SyncState::RetryWait);
  assert(!clock.tick(45699, false, false).connect);
  assert(clock.tick(45700, false, false).connect);
  assert(clock.tick(45800, true, false).requestTime);
  clock.tick(60800, true, false);  // NTP unavailable even though Wi-Fi connected.
  assert(clock.state() == SyncState::RetryWait && clock.known());
  assert(clock.tick(90800, true, false).requestTime);
  clock.tick(90900, true, true);
  assert(clock.fresh());
  ClockSync cold;
  constexpr uint32_t start = UINT32_MAX - 100;
  cold.request(start, true, false);
  cold.tick(start + ClockSync::kConnectTimeoutMs, false, false);
  assert(cold.state() == SyncState::RetryWait && !cold.known());
  assert(cold.tick(start + ClockSync::kConnectTimeoutMs + ClockSync::kRetryMs, false, false).connect);
}
std::string hex(const std::string& text) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : text) { out += digits[c >> 4]; out += digits[c & 15]; }
  return out;
}
void testDeviceCommands() {
  using namespace diagnostics;
  using Kind = DeviceCommandKind;
  assert(parseDeviceCommand("sleep 30").seconds == 30);
  assert(parseDeviceCommand("sleep 86400").kind == Kind::Sleep);
  for (const char* bad : {"sleep 0", "sleep -1", "sleep 86401", "sleep 999999999999999", "idle 3601", "idle ", "wifi-set 00 ", "wifi-set a ", "wifi-set 61 31"})
    assert(parseDeviceCommand(bad).kind == Kind::Invalid);
  assert(parseDeviceCommand("idle off").seconds == 0);
  assert(parseDeviceCommand("time auto").kind == Kind::AutoClock);
  assert(parseDeviceCommand("theme dark").kind == Kind::Other);
  const std::string ssid = "test network";
  const std::string command = "wifi-set " + hex(ssid) + " " + hex("some pass123");
  auto parsed = parseDeviceCommand(command.c_str());
  assert(parsed.kind == Kind::WifiSetup && ssid == parsed.ssid && std::string(parsed.password) == "some pass123");
  parsed.clearSecrets(); assert(!*parsed.ssid && !*parsed.password);
  assert(parseDeviceCommand("wifi-set 61 ").kind == Kind::WifiSetup);  // Open AP.
  const auto longest = "wifi-set " + hex(std::string(32, 's')) + " " + hex(std::string(63, 'p'));
  BasicLineBuffer<224> buffer;
  for (char c : longest) buffer.push(c);
  assert(buffer.push('\n') == LineResult::Ready);
  assert(parseDeviceCommand(buffer.line()).kind == Kind::WifiSetup);
  buffer.clear(); assert(!*buffer.line());
}
int main() {
  testInput(); testClock(); testDeviceCommands();
  std::puts("PASS: single/double/hold intent mapping, wake suppression, retry/fresh NTP after every wake, rollover, credential framing");
}
