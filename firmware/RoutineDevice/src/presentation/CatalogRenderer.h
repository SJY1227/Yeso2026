#pragma once
#include "../application/ProductState.h"
#include "../application/ProductView.h"

namespace routine::ui {
// Catalog page body and theme. Clock and confirmation overlays belong to the caller.
void renderCatalogPage(uint16_t* dst, const application::ProductState& state,
                       const application::ProductView& view);
}
