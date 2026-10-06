#include "../firmware/RoutineDevice/src/diagnostics/HardwareCheck.h"
#include "../firmware/RoutineDevice/src/diagnostics/LineBuffer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace routine::diagnostics;
void testCommands() {
  HardwareCheck check;
  assert(check.onCommand("theme dark").redraw);
  assert(check.view().theme == routine::ui::Theme::Dark);
  assert(check.onCommand("progress 100").reply == Reply::Ok);
  assert(check.view().progress == 100);
  assert(check.onCommand("time 23:59").reply == Reply::Ok);
  assert(check.view().minutesOfDay == 1439);
  assert(check.onCommand("time off").reply == Reply::Ok);
  assert(!check.view().clockSet);
  for (const char* bad : {"progress -1", "progress 101", "progress 999999999999999999", "progress 3x", "progress ", "time 24:00", "time 09", "time 09:", "time 09:60", "time 9:10x", "theme red", ""}) {
    const auto result = check.onCommand(bad);
    assert(result.reply == Reply::InvalidCommand && !result.redraw && !result.buzzerMs);
    assert(check.view().progress == 100 && check.view().minutesOfDay == 1439 && !check.view().clockSet);
  }
  assert(check.onCommand("time 0:00").reply == Reply::Ok);
  assert(check.view().clockSet && check.view().minutesOfDay == 0);
  assert(check.onCommand("help").reply == Reply::Help);
  assert(check.onCommand("beep").buzzerMs == 80);
  assert(check.onIntent(routine::input::Intent::Next).redraw);
  assert(check.view().theme == routine::ui::Theme::Pink);
  assert(check.onIntent(routine::input::Intent::Confirm).buzzerMs == 40);
  const auto held = check.onIntent(routine::input::Intent::Previous);
  assert(!held.redraw && held.buzzerMs == 80);
}
void testFraming() {
  LineBuffer buffer;
  for (char c : std::string("theme dark\r")) assert(buffer.push(c) == LineResult::None);
  assert(buffer.push('\n') == LineResult::Ready);
  assert(std::strcmp(buffer.line(), "theme dark") == 0);
  assert(buffer.push('\n') == LineResult::None);
  for (size_t n = 0; n < LineBuffer::kCapacity; ++n) buffer.push('x');
  assert(buffer.push('\n') == LineResult::TooLong);
  for (char c : std::string("help")) buffer.push(c);
  assert(buffer.push('\n') == LineResult::Ready);
  assert(std::strcmp(buffer.line(), "help") == 0);
  for (char c : std::string("beep")) buffer.push(c);
  buffer.push('\0');
  assert(buffer.push('\n') == LineResult::TooLong);
}
int main() {
  testCommands(); testFraming();
  std::puts("PASS: diagnostic commands, numeric overflow rejection, unchanged state on errors, serial overflow recovery, button effects");
}
