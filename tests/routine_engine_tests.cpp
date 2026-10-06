#include "../firmware/RoutineDevice/src/domain/RoutineEngine.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <set>
#include <tuple>

using namespace routine::domain;
RunState scheduled(uint8_t steps = 2, DeadlineRule rule = DeadlineRule::DeadlineWins, uint32_t warning = 10) {
  RunSpec spec;
  spec.id = 7001; spec.startsAt = 100; spec.endsAt = 200; spec.stepCount = steps;
  for (uint32_t i = 0; i < steps; ++i) spec.steps[i] = 1000 + i;
  RunState state;
  assert(createRun(spec, {rule, warning}, state) == SpecError::None);
  assert(valid(state));
  return state;
}
RunState apply(Transition transition) {
  assert(transition.changed());
  assert(valid(transition.next));
  return transition.next;
}
Command command(const RunState& state, Action action, StepId step = 0) {
  return {action, state.spec.id, state.version, step};
}
RunState active(uint8_t count = 2, DeadlineRule rule = DeadlineRule::DeadlineWins) {
  const auto state = scheduled(count, rule);
  return apply(dispatch(state, command(state, Action::OpenLetter), {100, true}));
}
void testValidation() {
  auto good = scheduled();
  auto spec = good.spec;
  spec.id = 0;
  assert(createRun(spec, good.policy, good) == SpecError::MissingRunId);
  assert(good.spec.id == 7001);  // Failed preparation never damages live state.
  spec = good.spec; spec.steps[1] = spec.steps[0];
  assert(createRun(spec, good.policy, good) == SpecError::DuplicateStepId);
  spec = good.spec; spec.stepCount = kMaxSteps + 1;
  assert(createRun(spec, good.policy, good) == SpecError::InvalidStepCount);
  spec = good.spec; spec.endsAt = spec.startsAt;
  assert(createRun(spec, good.policy, good) == SpecError::InvalidWindow);
  good.earnedRewards = 1;
  assert(advanceTime(good, {100, true}).result == Result::InvalidState);
}
void testScheduleAndLetter() {
  auto state = scheduled();
  assert(!advanceTime(state, {99, true}).changed());
  assert(advanceTime(state, {150, false}).result == Result::ClockUnknown);
  assert(!advanceTime(state, {150, false}).changed());
  auto start = advanceTime(state, {150, true});  // A missed exact second is okay.
  assert(start.eventCount == 1 && start.events[0].kind == EventKind::Started);
  state = apply(start);
  assert(state.phase == Phase::LetterPending);
  assert(dispatch(state, command(state, Action::CompleteStep, 1000), {151, true}).result == Result::WrongPhase);
  state = apply(dispatch(state, command(state, Action::OpenLetter), {151, true}));
  assert(state.phase == Phase::Active && state.completedSteps == 0);
  assert(!advanceTime(state, {152, true}).changed());
}
void testCompletionAndRetries() {
  auto state = active();
  const auto request = command(state, Action::CompleteStep, 1000);
  const auto proposed = dispatch(state, request, {110, true});
  assert(proposed.next.completedSteps == 1 && proposed.next.earnedRewards == 1);
  assert(proposed.eventCount == 2 && proposed.events[1].kind == EventKind::RewardEarned);
  assert(state.completedSteps == 0);  // Pure calculation; do not publish before save.
  const auto retryBeforeCommit = dispatch(state, request, {110, true});
  assert(retryBeforeCommit.events[0].run == proposed.events[0].run);
  assert(retryBeforeCommit.events[0].step == proposed.events[0].step);
  state = apply(proposed);  // Model a successful atomic state+events commit.
  const auto restored = state;  // State rehydration, not a flash persistence test.
  const auto repeated = dispatch(restored, request, {111, true});
  assert(repeated.result == Result::StaleVersion && !repeated.changed());
  const auto oldStep = dispatch(restored, command(restored, Action::CompleteStep, 1000), {111, true});
  assert(oldStep.result == Result::WrongStep && !oldStep.changed());
  auto last = dispatch(state, command(state, Action::CompleteStep, 1001), {190, true});
  assert(last.eventCount == 4);  // Warning + completion + reward + finished.
  state = apply(last);
  assert(state.phase == Phase::Completed && state.earnedRewards == 3);
  assert(!advanceTime(state, {1000, true}).changed());
  assert(!dispatch(state, command(state, Action::AbandonRun), {1000, true}).changed());
}
void testExpiryAndBoundaryPolicy() {
  auto state = active();
  state = apply(dispatch(state, command(state, Action::CompleteStep, 1000), {110, true}));
  auto end = dispatch(state, command(state, Action::CompleteStep, 1001), {200, true});
  assert(end.result == Result::DeadlinePassed && end.changed());
  state = apply(end);
  assert(state.phase == Phase::Expired && state.completedSteps == 1 && state.earnedRewards == 1);
  assert(end.eventCount == 1 && end.events[0].kind == EventKind::RunExpired);
  assert(!advanceTime(state, {201, true}).changed());
  const auto unseen = apply(advanceTime(scheduled(), {250, true}));
  assert(unseen.phase == Phase::Expired && unseen.completedSteps == 0);
  auto inclusive = active(1, DeadlineRule::AllowAtDeadline);
  inclusive = apply(dispatch(inclusive, command(inclusive, Action::CompleteStep, 1000), {200, true}));
  assert(inclusive.phase == Phase::Completed);
}
void testAbandonWarningAndClock() {
  auto state = active();
  state = apply(dispatch(state, command(state, Action::CompleteStep, 1000), {110, true}));
  assert(!advanceTime(state, {109, true}).changed());
  assert(advanceTime(state, {109, true}).result == Result::ClockMovedBackward);
  auto wrong = command(state, Action::AbandonRun); wrong.run += 1;
  assert(dispatch(state, wrong, {120, true}).result == Result::WrongRun);
  state = apply(advanceTime(state, {190, true}));
  assert(state.warningEmitted);
  assert(!advanceTime(state, {191, true}).changed());
  state = apply(dispatch(state, command(state, Action::AbandonRun), {191, true}));
  assert(state.phase == Phase::Abandoned && state.completedSteps == 1 && state.earnedRewards == 1);
  assert(!advanceTime(state, {300, true}).changed());
  auto overflow = active(); overflow.version = std::numeric_limits<uint32_t>::max();
  auto rejected = dispatch(overflow, command(overflow, Action::CompleteStep, 1000), {110, true});
  assert(rejected.result == Result::VersionExhausted && !rejected.changed());
  assert(rejected.next.completedSteps == 0);
}
void testCapacityAndUniqueEvents() {
  auto state = active(kMaxSteps);
  std::set<std::tuple<RunId, EventKind, StepId>> keys;
  for (uint32_t i = 0; i < kMaxSteps; ++i) {
    auto transition = dispatch(state, command(state, Action::CompleteStep, 1000+i), {110+i, true});
    for (size_t e = 0; e < transition.eventCount; ++e) {
      const auto& event = transition.events[e];
      assert(keys.emplace(event.run, event.kind, event.step).second);
    }
    state = apply(transition);
  }
  assert(state.phase == Phase::Completed && state.earnedRewards == 0xffff);
}
int main() {
  auto first=scheduled(),second=scheduled();second.spec.startsAt=first.spec.endsAt;second.spec.endsAt+=100;
  assert(!executionWindowsOverlap(first,second)&&!executionWindowsOverlap(second,first));
  first.policy.deadline=DeadlineRule::AllowAtDeadline;
  assert(executionWindowsOverlap(first,second)&&executionWindowsOverlap(second,first));
  testValidation(); testScheduleAndLetter(); testCompletionAndRetries();
  testExpiryAndBoundaryPolicy(); testAbandonWarningAndClock(); testCapacityAndUniqueEvents();
  std::puts("PASS: run validation, catch-up start, letter gating, step order, retry/replay, reward entitlement, deadline policies, abandonment, clock checks, capacity and unique events");
}
