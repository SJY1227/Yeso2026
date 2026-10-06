#pragma once
#include "../application/ProductView.h"
#include "Motion.h"
namespace routine::ui {
// Read-only projection: rendering/navigating a daily letter grants no rewards.
void renderSummary(uint16_t* dst, const application::ProductState& state,
                   const application::ProductView& view, MotionFrame motion);
}
