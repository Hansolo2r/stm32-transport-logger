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

static void TestShortPulseFromRestIsShockOnly(void)
{
  const char *name = "short pulse from rest";
  EventDetector detector;
  uint8_t events;

  EventDetector_Init(&detector, &default_config);
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
  (void)Feed(&detector, 1400, 100U);
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

int main(void)
{
  TestShortPulseFromRestIsShockOnly();
  TestSustainedMotionSuppressesStartupShock();
  TestLongHighLevelIsNotShock();
  TestShockWhileMovingIsRecorded();
  TestBriefSubShockActivityIsIgnored();
  TestStillnessEndsMotionAtConfiguredTime();
  (void)printf("PASS event detector: 6 scenarios\n");
  return 0;
}
