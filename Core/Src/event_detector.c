#include "event_detector.h"

#include <stddef.h>

static uint16_t SamplesForMilliseconds(const EventDetector *detector,
                                       uint16_t milliseconds)
{
  uint32_t interval = detector->config.sample_interval_ms;

  if (interval == 0U)
  {
    interval = 1U;
  }
  return (uint16_t)(((uint32_t)milliseconds + interval - 1U) / interval);
}

static uint32_t ThresholdSquared(uint16_t threshold_mg)
{
  uint32_t threshold = threshold_mg;

  return threshold * threshold;
}

static uint32_t AccelerationSquared(int16_t x_mg, int16_t y_mg,
                                    int16_t z_mg)
{
  int32_t x = x_mg;
  int32_t y = y_mg;
  int32_t z = z_mg;

  return (uint32_t)((x * x) + (y * y) + (z * z));
}

static void IncrementSaturated(uint16_t *value, uint16_t limit)
{
  if (*value < limit)
  {
    (*value)++;
  }
}

void EventDetector_Init(EventDetector *detector,
                        const EventDetectorConfig *config)
{
  if ((detector == NULL) || (config == NULL))
  {
    return;
  }
  detector->config = *config;
  EventDetector_Reset(detector);
}

void EventDetector_Reset(EventDetector *detector)
{
  if (detector == NULL)
  {
    return;
  }
  detector->motion_evidence_samples = 0U;
  detector->still_samples = 0U;
  detector->shock_samples = 0U;
  detector->motion_active = 0U;
  detector->pending_startup_shock = 0U;
  detector->shock_started_while_moving = 0U;
}

uint8_t EventDetector_Update(EventDetector *detector, int16_t x_mg,
                             int16_t y_mg, int16_t z_mg)
{
  uint8_t events = EVENT_DETECTOR_NONE;
  uint32_t magnitude_squared;
  uint16_t motion_low_mg;
  uint16_t motion_high_mg;
  uint16_t still_delta_mg;
  uint16_t still_low_mg;
  uint16_t still_high_mg;
  uint16_t motion_confirm_samples;
  uint16_t still_confirm_samples;
  uint16_t shock_max_samples;
  uint8_t above_shock;
  uint8_t outside_motion_band;
  uint8_t inside_still_band;

  if (detector == NULL)
  {
    return events;
  }

  magnitude_squared = AccelerationSquared(x_mg, y_mg, z_mg);
  motion_low_mg = (uint16_t)(1000U - detector->config.motion_delta_mg);
  motion_high_mg = (uint16_t)(1000U + detector->config.motion_delta_mg);
  still_delta_mg = (detector->config.motion_delta_mg > 50U) ?
                   (uint16_t)(detector->config.motion_delta_mg - 50U) : 25U;
  still_low_mg = (uint16_t)(1000U - still_delta_mg);
  still_high_mg = (uint16_t)(1000U + still_delta_mg);
  motion_confirm_samples = SamplesForMilliseconds(
    detector, detector->config.motion_confirm_ms);
  still_confirm_samples = SamplesForMilliseconds(
    detector, detector->config.still_confirm_ms);
  shock_max_samples = SamplesForMilliseconds(
    detector, detector->config.shock_max_duration_ms);

  above_shock = (magnitude_squared >=
                 ThresholdSquared(detector->config.shock_threshold_mg)) ? 1U : 0U;
  outside_motion_band = ((magnitude_squared < ThresholdSquared(motion_low_mg)) ||
                         (magnitude_squared > ThresholdSquared(motion_high_mg))) ?
                        1U : 0U;
  inside_still_band = ((magnitude_squared >= ThresholdSquared(still_low_mg)) &&
                       (magnitude_squared <= ThresholdSquared(still_high_mg))) ?
                      1U : 0U;

  if (above_shock != 0U)
  {
    if (detector->shock_samples == 0U)
    {
      detector->shock_started_while_moving = detector->motion_active;
    }
    IncrementSaturated(&detector->shock_samples,
                       (uint16_t)(shock_max_samples + 1U));
  }
  else if (detector->shock_samples != 0U)
  {
    if (detector->shock_samples <= shock_max_samples)
    {
      if (detector->shock_started_while_moving != 0U)
      {
        events |= EVENT_DETECTOR_SHOCK;
      }
      else if (detector->motion_active == 0U)
      {
        detector->pending_startup_shock = 1U;
      }
    }
    detector->shock_samples = 0U;
    detector->shock_started_while_moving = 0U;
  }

  if (detector->motion_active == 0U)
  {
    if (outside_motion_band != 0U)
    {
      IncrementSaturated(&detector->motion_evidence_samples,
                         motion_confirm_samples);
    }
    else if (detector->motion_evidence_samples > 0U)
    {
      detector->motion_evidence_samples--;
    }

    if (detector->motion_evidence_samples >= motion_confirm_samples)
    {
      detector->motion_active = 1U;
      detector->motion_evidence_samples = 0U;
      detector->still_samples = 0U;
      detector->pending_startup_shock = 0U;
      events |= EVENT_DETECTOR_MOTION_START;
    }
    else if ((detector->motion_evidence_samples == 0U) &&
             (detector->pending_startup_shock != 0U))
    {
      detector->pending_startup_shock = 0U;
      events |= EVENT_DETECTOR_SHOCK;
    }
  }
  else if (inside_still_band != 0U)
  {
    IncrementSaturated(&detector->still_samples, still_confirm_samples);
    if (detector->still_samples >= still_confirm_samples)
    {
      detector->motion_active = 0U;
      detector->still_samples = 0U;
      detector->motion_evidence_samples = 0U;
      detector->pending_startup_shock = 0U;
      events |= EVENT_DETECTOR_MOTION_END;
    }
  }
  else
  {
    detector->still_samples = 0U;
  }

  return events;
}

uint8_t EventDetector_IsMotionActive(const EventDetector *detector)
{
  return (detector != NULL) ? detector->motion_active : 0U;
}
