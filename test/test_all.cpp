#include <unity.h>
#include "battery.h"
#include "timing.h"
#include "config.h"

// Deterministic RNG for tests
static uint32_t g_rng_counter = 0;
static uint32_t testRng() {
  g_rng_counter = g_rng_counter * 1664525 + 1013904223;
  return g_rng_counter;
}

void setUp(void) {
  g_rng_counter = 12345;
}

// Battery tests

void test_init_picks_random_when_uninitialised(void) {
  BatterySimulator bat(testRng);
  bat.init(0, false);
  uint8_t lvl = bat.level();
  TEST_ASSERT(lvl >= BATTERY_INIT_MIN && lvl <= BATTERY_INIT_MAX);
}

void test_init_keeps_saved_level(void) {
  BatterySimulator bat(testRng);
  bat.init(75, true);
  TEST_ASSERT_EQUAL(75, bat.level());
}

void test_init_replaces_low_saved_level(void) {
  BatterySimulator bat(testRng);
  bat.init(5, true);
  TEST_ASSERT(bat.level() >= BATTERY_INIT_MIN);
}

void test_drain_high_band(void) {
  BatterySimulator bat(testRng);
  bat.init(95, true);
  bat.setLastFluctuation(DRAIN_6H + 1);
  bat.update(DRAIN_6H + 1, -1.0f);
  TEST_ASSERT_EQUAL(94, bat.level());
}

void test_drain_mid_band(void) {
  BatterySimulator bat(testRng);
  bat.init(50, true);
  bat.setLastFluctuation(DRAIN_4H + 1);
  bat.update(DRAIN_4H + 1, -1.0f);
  TEST_ASSERT_EQUAL(49, bat.level());
}

void test_drain_low_band(void) {
  BatterySimulator bat(testRng);
  bat.init(15, true);
  bat.setLastFluctuation(DRAIN_2H + 1);
  bat.update(DRAIN_2H + 1, -1.0f);
  TEST_ASSERT_EQUAL(14, bat.level());
}

void test_drain_critical_band(void) {
  BatterySimulator bat(testRng);
  bat.init(20, true);
  bat.setLevel(8);
  bat.setLastFluctuation(DRAIN_1H + 1);
  bat.update(DRAIN_1H + 1, -1.0f);
  TEST_ASSERT_EQUAL(7, bat.level());
}

void test_first_drop_from_100(void) {
  BatterySimulator bat(testRng);
  bat.init(100, true);
  TEST_ASSERT_EQUAL(100, bat.level());

  // firstDropInterval_ is random (10-20 min). Wait DRAIN_FIRST_DROP_MAX
  // to guarantee it fires.
  unsigned long t = DRAIN_FIRST_DROP_MAX + 1;
  bat.setLastDrop(0);
  bat.setLastFluctuation(t);
  bat.update(t, -1.0f);
  TEST_ASSERT_EQUAL(99, bat.level());

  // After the first drop, the special interval is cleared;
  // next drop uses the normal band interval (DRAIN_6H for >80 %)
  bat.setLastDrop(t);
  bat.setLastFluctuation(t + DRAIN_6H + 1);
  bat.update(t + DRAIN_6H + 1, -1.0f);
  TEST_ASSERT_EQUAL(98, bat.level());
}

void test_drain_stops_at_floor(void) {
  BatterySimulator bat(testRng);
  bat.init(20, true);
  bat.setLevel(3);
  bat.setLastFluctuation(DRAIN_1H + 1);
  bat.update(DRAIN_1H + 1, -1.0f);
  TEST_ASSERT_EQUAL(3, bat.level());
}

void test_fluctuation_changes_level(void) {
  BatterySimulator bat(testRng);
  bat.init(50, true);                         // getDropInterval() → DRAIN_4H
  bat.setLastDrop(DRAIN_4H + 1);              // keep drain from firing
  bat.update(DRAIN_4H + 1, -1.0f);                   // triggers fluctuation
  uint8_t lvl = bat.level();
  TEST_ASSERT(lvl == 49 || lvl == 50);
}

void test_fluctuation_clamps(void) {
  BatterySimulator bat(testRng);
  bat.init(20, true);                         // ≥ BATTERY_RECOVERY_MIN, stays 20
  bat.setLevel(3);
  bat.setLastDrop(DRAIN_1H + 1);              // keep drain from firing
  g_rng_counter = 12345;
  bat.update(DRAIN_1H + 1, -1.0f);                   // triggers fluctuation
  TEST_ASSERT(bat.level() >= 3);
}

// Timing tests

void test_super_cycle_length(void) {
  TimingGenerator tg(testRng);
  for (int i = 0; i < MOVES_PER_SUPER_CYCLE; i++) {
    Move m = tg.nextMove();
    TEST_ASSERT(m.dx != 0 || m.dy != 0);
  }
  tg.reset();
  Move a = tg.nextMove();
  tg.reset();
  Move a2 = tg.nextMove();
  TEST_ASSERT_EQUAL(a.dx, a2.dx);
  TEST_ASSERT_EQUAL(a.dy, a2.dy);
}

void test_zero_net_drift(void) {
  TimingGenerator tg(testRng);
  int sumX = 0, sumY = 0;
  for (int i = 0; i < MOVES_PER_SUPER_CYCLE; i++) {
    Move m = tg.nextMove();
    sumX += m.dx;
    sumY += m.dy;
  }
  TEST_ASSERT_EQUAL(0, sumX);
  TEST_ASSERT_EQUAL(0, sumY);
}

void test_move_delay_range(void) {
  TimingGenerator tg(testRng);
  for (int i = 0; i < 100; i++) {
    unsigned long d = tg.moveDelay();
    TEST_ASSERT(d <= MOVE_DELAY_MAX);
  }
}

void test_idle_pause_probability(void) {
  TimingGenerator tg(testRng);
  unsigned int pauses = 0;
  for (int i = 0; i < 1000; i++) {
    if (tg.maybeIdlePause()) pauses++;
  }
  TEST_ASSERT(pauses >= 70 && pauses <= 130);
}

void test_idle_pause_duration_range(void) {
  TimingGenerator tg(testRng);
  for (int i = 0; i < 100; i++) {
    unsigned long p = tg.idlePauseDuration();
    TEST_ASSERT(p >= IDLE_PAUSE_MIN);
    TEST_ASSERT(p <= IDLE_PAUSE_MAX);
  }
}

// Runner

int main(void) {
  UnityBegin("test_all");

  RUN_TEST(test_init_picks_random_when_uninitialised);
  RUN_TEST(test_init_keeps_saved_level);
  RUN_TEST(test_init_replaces_low_saved_level);
  RUN_TEST(test_drain_high_band);
  RUN_TEST(test_drain_mid_band);
  RUN_TEST(test_drain_low_band);
  RUN_TEST(test_drain_critical_band);
  RUN_TEST(test_first_drop_from_100);
  RUN_TEST(test_drain_stops_at_floor);
  RUN_TEST(test_fluctuation_changes_level);
  RUN_TEST(test_fluctuation_clamps);

  RUN_TEST(test_super_cycle_length);
  RUN_TEST(test_zero_net_drift);
  RUN_TEST(test_move_delay_range);
  RUN_TEST(test_idle_pause_probability);
  RUN_TEST(test_idle_pause_duration_range);

  UnityEnd();
  return 0;
}
