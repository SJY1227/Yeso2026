#pragma once
#include "HomeRenderer.h"
#include "../application/ProductView.h"
#include "Motion.h"
namespace routine::ui {
void renderDeviceStatus(uint16_t* pixels,bool wifiConnected,int batteryPercent);
void renderProduct(uint16_t* pixels, const application::ProductState& state,
                   const application::ProductView& view, bool clockSet, uint16_t minutes,MotionFrame motion={});
}
