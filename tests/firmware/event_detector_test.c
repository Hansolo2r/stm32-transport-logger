#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "event_detector.h"

#define SAMPLE_INTERVAL_MS 10U

static const EventDetectorConfig default_config = {
  2200U,
  150U,
  1000U,
  1500U,
  200U,
  SAMPLE_INTERVAL_MS
};

static void Fail(const char *test_name, const char *message)
{
  (void)fprintf(stderr, "FAIL %s: %s\n", test_name, message);
  exit(1);
}

static uint8_t Feed(EventDetector *detector, int16_t magnitude_mg,
                    uint16_t samples)
{
  uint8_t events = EVENT_DETECTOR_NONE;

  for (uint16_t index = 0U; index < samples; index++)
  {
    events |= EventDetector_Update(detector, 0, 0, magnitude_mg);
  }
  return events;
}

static uint8_t FeedVector(EventDetector *detector, int16_t x_mg,
                          int16_t y_mg, int16_t z_mg, uint16_t samples)
{
  uint8_t events = EVENT_DETECTOR_NONE;

  for (uint16_t index = 0U; index < samples; index++)
  {
    events |= EventDetector_Update(detector, x_mg, y_mg, z_mg);
  }
  return events;
}

static void PrimeRest(EventDetector *detector)
{
  (void)FeedVector(detector, 0, 0, 1000, 1U);
}

static void TestShortPulseFromRestIsShockOnly(void)
{
  const char *name = "short pulse from rest";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 2600, 5U);
  events |= Feed(&detector, 1000, 5U);

  if ((events & EVENT_DETECTOR_SHOCK) == 0U)
  {
    Fail(name, "expected one shock");
  }
  if ((events & EVENT_DETECTOR_MOTION_START) != 0U)
  {
    Fail(name, "startup shock must not become motion");
  }
}

static void TestSustainedMotionSuppressesStartupShock(void)
{
  const char *name = "sustained motion suppresses startup shock";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 2600, 5U);
  events |= Feed(&detector, 1400, 95U);

  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "expected motion after 1000 ms of evidence");
  }
  if ((events & EVENT_DETECTOR_SHOCK) != 0U)
  {
    Fail(name, "startup pulse must be suppressed by sustained motion");
  }
}

static void TestLongHighLevelIsNotShock(void)
{
  const char *name = "long high level";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 2600, 30U);
  events |= Feed(&detector, 1000, 30U);

  if ((events & EVENT_DETECTOR_SHOCK) != 0U)
  {
    Fail(name, "300 ms high level must not be a shock");
  }
}

static void TestShockWhileMovingIsRecorded(void)
{
  const char *name = "shock while moving";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 1400, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "test setup did not enter motion");
  }

  events = Feed(&detector, 2600, 5U);
  events |= Feed(&detector, 1400, 1U);
  if ((events & EVENT_DETECTOR_SHOCK) == 0U)
  {
    Fail(name, "short pulse during motion must be recorded");
  }
}

static void TestBriefSubShockActivityIsIgnored(void)
{
  const char *name = "brief sub-shock activity";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 1400, 50U);
  events |= Feed(&detector, 1000, 50U);

  if (events != EVENT_DETECTOR_NONE)
  {
    Fail(name, "500 ms activity must produce no event");
  }
}

static void TestStillnessEndsMotionAtConfiguredTime(void)
{
  const char *name = "stillness ends motion";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  (void)Feed(&detector, 1400, 100U);
  (void)Feed(&detector, 1000, 1U);
  events = Feed(&detector, 1000, 149U);
  if ((events & EVENT_DETECTOR_MOTION_END) != 0U)
  {
    Fail(name, "motion ended before 1500 ms");
  }
  events = Feed(&detector, 1000, 1U);
  if ((events & EVENT_DETECTOR_MOTION_END) == 0U)
  {
    Fail(name, "motion did not end at 1500 ms");
  }
}

static void TestProductionDefaults(void)
{
  const char *name = "production defaults";

  if ((EVENT_DETECTOR_DEFAULT_SHOCK_THRESHOLD_MG != 2200U) ||
      (EVENT_DETECTOR_DEFAULT_MOTION_DELTA_MG != 150U) ||
      (EVENT_DETECTOR_DEFAULT_MOTION_CONFIRM_MS != 1000U) ||
      (EVENT_DETECTOR_DEFAULT_STILL_CONFIRM_MS != 1500U) ||
      (EVENT_DETECTOR_DEFAULT_SHOCK_MAX_DURATION_MS != 200U))
  {
    Fail(name, "default values do not match the approved design");
  }
}

static void TestShockDurationBoundary(void)
{
  const char *name = "shock duration boundary";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 2600, 20U);
  events |= Feed(&detector, 1000, 20U);
  if ((events & EVENT_DETECTOR_SHOCK) == 0U)
  {
    Fail(name, "exactly 200 ms must be a shock");
  }

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = Feed(&detector, 2600, 21U);
  events |= Feed(&detector, 1000, 21U);
  if ((events & EVENT_DETECTOR_SHOCK) != 0U)
  {
    Fail(name, "210 ms must not be a shock");
  }
}

static void TestRotationAtOneGStartsMotion(void)
{
  const char *name = "one-g rotation";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = FeedVector(&detector, 1000, 0, 0, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "rotation with unchanged magnitude must start motion");
  }
}

static void TestWalkingVariationStartsMotion(void)
{
  const char *name = "walking variation";
  EventDetector detector;
  uint8_t events = EVENT_DETECTOR_NONE;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  for (uint16_t index = 0U; index < 100U; index++)
  {
    int16_t x_mg = ((index & 1U) == 0U) ? 120 : -120;
    events |= EventDetector_Update(&detector, x_mg, 0, 993);
  }
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "alternating axis vibration must start motion");
  }
}

static void TestStationaryNoiseIsIgnored(void)
{
  const char *name = "stationary noise";
  EventDetector detector;
  uint8_t events = EVENT_DETECTOR_NONE;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  for (uint16_t index = 0U; index < 300U; index++)
  {
    int16_t noise = ((index & 1U) == 0U) ? 10 : -10;
    events |= EventDetector_Update(&detector, noise, noise, 1000);
  }
  if (events != EVENT_DETECTOR_NONE)
  {
    Fail(name, "small axis noise must not produce an event");
  }
}

static void TestNewRestingOrientationBecomesReference(void)
{
  const char *name = "new resting orientation";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = FeedVector(&detector, 1000, 0, 0, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "test setup did not enter motion");
  }
  events = FeedVector(&detector, 1000, 0, 0, 150U);
  if ((events & EVENT_DETECTOR_MOTION_END) == 0U)
  {
    Fail(name, "stable new orientation did not end motion");
  }
  events = FeedVector(&detector, 1000, 0, 0, 100U);
  if (events != EVENT_DETECTOR_NONE)
  {
    Fail(name, "new resting orientation restarted motion");
  }
}

static void TestOccasionalRestNoiseStillEndsMotion(void)
{
  const char *name = "occasional rest noise";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = FeedVector(&detector, 1000, 0, 0, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "test setup did not enter motion");
  }

  events = EVENT_DETECTOR_NONE;
  for (uint16_t index = 0U; index < 300U; index++)
  {
    int16_t x_mg = (((index + 1U) % 50U) == 0U) ? 1060 : 1000;
    events |= EventDetector_Update(&detector, x_mg, 0, 0);
  }
  if ((events & EVENT_DETECTOR_MOTION_END) == 0U)
  {
    Fail(name, "isolated sensor jitter kept motion active indefinitely");
  }
}

static void TestIntermittentMovementStaysActive(void)
{
  const char *name = "intermittent movement";
  EventDetector detector;
  uint8_t events;
  int16_t x_mg = 1000;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = FeedVector(&detector, x_mg, 0, 0, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "test setup did not enter motion");
  }

  events = EVENT_DETECTOR_NONE;
  for (uint16_t index = 0U; index < 500U; index++)
  {
    if ((index % 10U) == 0U)
    {
      x_mg = (int16_t)(x_mg + 60);
    }
    events |= EventDetector_Update(&detector, x_mg, 0, 0);
  }
  if ((events & EVENT_DETECTOR_MOTION_END) != 0U)
  {
    Fail(name, "continued axis changes were classified as stillness");
  }
}

static void TestBoundedRestJitterStillEndsMotion(void)
{
  const char *name = "bounded rest jitter";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
  PrimeRest(&detector);
  events = FeedVector(&detector, 1000, 0, 0, 100U);
  if ((events & EVENT_DETECTOR_MOTION_START) == 0U)
  {
    Fail(name, "test setup did not enter motion");
  }

  events = EVENT_DETECTOR_NONE;
  for (uint16_t index = 0U; index < 160U; index++)
  {
    int16_t x_mg = ((index & 1U) == 0U) ? 1060 : 940;
    events |= EventDetector_Update(&detector, x_mg, 0, 0);
  }
  if ((events & EVENT_DETECTOR_MOTION_END) == 0U)
  {
    Fail(name, "small bounded jitter kept motion active");
  }
}

int main(void)
{
  TestShortPulseFromRestIsShockOnly();
  TestSustainedMotionSuppressesStartupShock();
  TestLongHighLevelIsNotShock();
  TestShockWhileMovingIsRecorded();
  TestBriefSubShockActivityIsIgnored();
  TestRotationAtOneGStartsMotion();
  TestWalkingVariationStartsMotion();
  TestStationaryNoiseIsIgnored();
  TestNewRestingOrientationBecomesReference();
  TestOccasionalRestNoiseStillEndsMotion();
  TestIntermittentMovementStaysActive();
  TestBoundedRestJitterStillEndsMotion();
  TestStillnessEndsMotionAtConfiguredTime();
  TestProductionDefaults();
  TestShockDurationBoundary();
  (void)printf("PASS event detector: 15 scenarios\n");
  return 0;
}
