#include "battery.h"
#include "config.h"

#ifdef ESP_PLATFORM
#include <esp_system.h>
static uint32_t defaultRng() { return esp_random(); }
#else
#include <cstdlib>
static uint32_t defaultRng() { return static_cast<uint32_t>(rand()); }
#endif

BatterySimulator::BatterySimulator(RngFunc rng)
  : level_(0), lastDrop_(0), lastFluctuation_(0),
    lastTemp_(0.0f), firstDropInterval_(0),
    initialised_(false), rng_(rng ? rng : defaultRng) {}

uint8_t BatterySimulator::level() const { return level_; }
unsigned long BatterySimulator::lastDrop() const { return lastDrop_; }
void BatterySimulator::setLastDrop(unsigned long t) { lastDrop_ = t; }
void BatterySimulator::setLastFluctuation(unsigned long t) { lastFluctuation_ = t; }
void BatterySimulator::setLevel(uint8_t lvl) { level_ = lvl; }

void BatterySimulator::init(uint8_t savedLevel, bool wasInitialised) {
  if (!wasInitialised || savedLevel < BATTERY_RECOVERY_MIN) {
    uint32_t range = BATTERY_INIT_MAX - BATTERY_INIT_MIN + 1;
    level_ = BATTERY_INIT_MIN + (rng_() % range);
  } else {
    level_ = savedLevel;
  }
  initialised_ = true;

  // If starting at 100 %, set a random 10-20 min first-drop timer
  if (level_ == BATTERY_MAX) {
    uint32_t range = DRAIN_FIRST_DROP_MAX - DRAIN_FIRST_DROP_MIN + 1;
    firstDropInterval_ = DRAIN_FIRST_DROP_MIN + (rng_() % range);
  } else {
    firstDropInterval_ = 0;
  }
}

void BatterySimulator::update(unsigned long now, float tempCelsius) {
  // Use first-drop interval (10-20 min) when at 100 %, otherwise normal band
  unsigned long interval = (level_ == BATTERY_MAX && firstDropInterval_ > 0)
    ? firstDropInterval_
    : getDropInterval();

  // ── Drain ──
  if (now - lastDrop_ >= interval) {
    if (level_ > BATTERY_MIN) {
      if (lastTemp_ > 0.0f && tempCelsius > lastTemp_) {
        // Chip hotter than last drain → extra drain from heat
        level_ -= (level_ - 1 > BATTERY_MIN) ? 2 : 1;
      } else {
        level_--;
      }
    }
    lastDrop_ = now;
    lastTemp_ = tempCelsius;
    // After draining from 100 %, clear the special first-drop interval
    firstDropInterval_ = 0;
  }

  // ── Fluctuation ──
  if (now - lastFluctuation_ >= interval) {
    int8_t delta = -static_cast<int8_t>(rng_() % 2);
    int16_t newLevel = static_cast<int16_t>(level_) + delta;
    if (newLevel > BATTERY_MAX) newLevel = BATTERY_MAX;
    if (newLevel < BATTERY_MIN) newLevel = BATTERY_MIN;
    level_ = static_cast<uint8_t>(newLevel);
    lastFluctuation_ = now;
  }
}

unsigned long BatterySimulator::getDropInterval() const {
  if (level_ > 80) return DRAIN_6H;
  if (level_ > 30) return DRAIN_4H;
  if (level_ > 10) return DRAIN_2H;
  return DRAIN_1H;
}
