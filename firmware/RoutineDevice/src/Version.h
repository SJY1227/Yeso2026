#pragma once
namespace routine {
#if defined(ROUTINE_BLE_DEVELOPMENT) && ROUTINE_BLE_DEVELOPMENT
inline constexpr char kFirmwareVersion[]="0.9.0-ble-dev";
#else
inline constexpr char kFirmwareVersion[]="0.9.0";
#endif
}
