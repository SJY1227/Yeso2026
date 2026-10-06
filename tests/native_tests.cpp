#include "LoadAssets.h"
#include "../firmware/RoutineDevice/src/input/Button.h"
#include "../firmware/RoutineDevice/src/presentation/HomeRenderer.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>

using routine::input::Button;
using routine::input::ButtonEvent;
void testButton() {
  Button b;
  b.begin(0, false);
  assert(b.update(10, true) == ButtonEvent::None);
  assert(b.update(20, false) == ButtonEvent::None);
  assert(b.update(25, true) == ButtonEvent::None);
  assert(b.update(54, true) == ButtonEvent::None);
  assert(b.update(55, true) == ButtonEvent::None);
  assert(b.update(100, false) == ButtonEvent::None);
  assert(b.update(110, true) == ButtonEvent::None);
  assert(b.update(120, false) == ButtonEvent::None);
  assert(b.update(150, false) == ButtonEvent::ShortPress);
  assert(b.update(200, false) == ButtonEvent::None);

  b.begin(0, false);
  b.update(10, true); b.update(40, true);
  assert(b.update(839, true) == ButtonEvent::None);
  assert(b.update(840, true) == ButtonEvent::LongPress);
  assert(b.update(2000, true) == ButtonEvent::None);
  b.update(2010, false);
  assert(b.update(2040, false) == ButtonEvent::None);

  b.begin(0, true);
  assert(b.update(1000, true) == ButtonEvent::None);
  b.update(1100, false);
  assert(b.update(1130, false) == ButtonEvent::None);
  b.update(1200, true); b.update(1230, true); b.update(1300, false);
  assert(b.update(1330, false) == ButtonEvent::ShortPress);

  constexpr uint32_t start = UINT32_MAX - 100;
  b.begin(start, false);
  b.update(start + 10, true); b.update(start + 40, true);
  assert(b.update(start + 839, true) == ButtonEvent::None);
  assert(b.update(start + 840, true) == ButtonEvent::LongPress);
}
void testBuzzer() {
  routine::input::BuzzerPulse p;
  assert(!p.active());
  p.start(UINT32_MAX - 30, 80);
  assert(p.update(48));
  assert(!p.update(49));
  p.start(100, 0);
  assert(!p.update(100));
}
void testRenderer() {
  constexpr size_t count = routine::ui::kWidth * routine::ui::kHeight;
  std::vector<uint16_t> guarded(count + 2, 0xdead), expected(count);
  auto* pixels = guarded.data() + 1;
  routine::ui::HomeViewModel state;
  state.clockSet = true;
  for (auto theme : {routine::ui::Theme::Pink, routine::ui::Theme::Dark}) {
    state.theme = theme; state.progress = 100;
    routine::ui::renderHome(pixels, state);
    const auto accent = pixels[294 * 240 + 100];
    assert(accent != 0xffff);
    state.progress = 255;
    routine::ui::renderHome(expected.data(), state);
    assert(std::equal(expected.begin(), expected.end(), pixels));
    state.progress = 0;
    routine::ui::renderHome(pixels, state);
    assert(pixels[294 * 240 + 100] == 0xffff);
    routine::ui::renderHome(expected.data(), state);
    assert(std::equal(expected.begin(), expected.end(), pixels));
    state.minutesOfDay = 0; routine::ui::renderHome(pixels, state);
    state.minutesOfDay = 12 * 60; routine::ui::renderHome(expected.data(), state);
    assert(!std::equal(expected.begin(), expected.end(), pixels));
    state.clockSet = false; routine::ui::renderHome(pixels, state);
    assert(!std::equal(expected.begin(), expected.end(), pixels));
    state.clockSet = true;
    assert(guarded.front() == 0xdead && guarded.back() == 0xdead);
  }
}
int main() {
  loadTestAssets();
  testButton(); testBuzzer(); testRenderer();
  std::puts("PASS: debounce, short/long press, startup-held, millis rollover, buzzer cutoff, renderer boundaries and redraw");
}
