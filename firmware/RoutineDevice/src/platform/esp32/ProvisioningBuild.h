#pragma once
// Never infer insecure provisioning from a debug log level or the saved URL.
#ifndef ROUTINE_BLE_DEVELOPMENT
#define ROUTINE_BLE_DEVELOPMENT 0
#endif
namespace routine::platform {
inline constexpr bool kBleDevelopment=ROUTINE_BLE_DEVELOPMENT!=0;
}
