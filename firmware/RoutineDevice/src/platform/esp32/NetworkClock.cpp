#include "NetworkClock.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <atomic>
#include <cstring>
#include <ctime>

namespace routine::platform {
namespace {
std::atomic<uint32_t> timeReceipts{0};
constexpr int64_t kEarliestTime = 1577836800;  // Reject an unset clock (2020-01-01).
void receivedTime(struct timeval* value) {
  // Called on the TCP/IP task; never render or mutate product state here.
  if (value && value->tv_sec >= kEarliestTime) timeReceipts.fetch_add(1);
}
}
void NetworkClock::begin(const ClockSettings& settings) {
  settings_ = settings;
  WiFi.persistent(true);  // The SDK's NVS keeps the one configured network.
  WiFi.setAutoReconnect(false);  // Bounded attempts are owned by ClockSync.
  configured_ = loadSavedNetwork();
  requestSync();
}
bool NetworkClock::loadSavedNetwork() {
  if (!WiFi.mode(WIFI_STA)) return false;
  wifi_config_t saved{};
  const bool exists = esp_wifi_get_config(WIFI_IF_STA, &saved) == ESP_OK && saved.sta.ssid[0];
  // Do not print SSID/password or keep an extra credential copy in the app.
  volatile uint8_t* bytes = reinterpret_cast<volatile uint8_t*>(&saved);
  for (size_t i = 0; i < sizeof(saved); ++i) bytes[i] = 0;
  return exists;
}
bool NetworkClock::connected() const { return WiFi.status() == WL_CONNECTED; }
void NetworkClock::stopTime() {
  if (timeRunning_) esp_sntp_stop();  // ESP-IDF's thread-safe wrapper.
  timeRunning_ = false;
}
void NetworkClock::apply(application::SyncActions actions) {
  if (actions.stopTime) stopTime();
  if (actions.connect) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);  // Keep the stored network on failure/retry.
    WiFi.begin();
  }
  if (actions.requestTime) {
    stopTime();
    receiptBaseline_ = timeReceipts.load();
    esp_sntp_set_time_sync_notification_cb(receivedTime);
    configTzTime(settings_.timezone, settings_.primaryServer, settings_.secondaryServer);
    timeRunning_ = true;
  }
}
void NetworkClock::requestSync() {
  ++requests_;
  apply(sync_.request(millis(), configured_, connected()));
}
void NetworkClock::tick() {
  const bool received = timeRunning_ && timeReceipts.load() != receiptBaseline_;
  const bool wasFresh = sync_.fresh();
  apply(sync_.tick(millis(), connected(), received));
  if (!wasFresh && sync_.fresh()) ++successes_;
}
bool NetworkClock::suspend() {
  apply(sync_.suspend());
  return WiFi.mode(WIFI_OFF);  // Credentials survive; every wake reconnects.
}
bool NetworkClock::configure(const char* ssid, const char* password) {
  if (!ssid || !password || !*ssid || std::strlen(ssid) > 32) return false;
  const size_t length = std::strlen(password);
  if (length && (length < 8 || length > 63)) return false;
  for (size_t i = 0; i < length; ++i)
    if (static_cast<unsigned char>(password[i]) < 32 || static_cast<unsigned char>(password[i]) > 126) return false;
  stopTime();
  if (!WiFi.mode(WIFI_STA)) return false;
  // Use the configuration result, not an old association status from a previous
  // failed password attempt (WiFi.begin() returns that status).
  if (!WiFi.STA.begin() || !WiFi.STA.connect(ssid, password, 0, nullptr, false)) return false;
  configured_ = true;
  requestSync();
  return true;
}
int64_t NetworkClock::unixSeconds() const { return static_cast<int64_t>(time(nullptr)); }
bool NetworkClock::localMinutes(uint16_t& minutes) const {
  if (!sync_.known()) return false;
  const time_t now = time(nullptr);
  struct tm local{};
  if (now < kEarliestTime || !localtime_r(&now, &local)) return false;
  minutes = static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
  return true;
}
}
