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

static uint64_t VectorDistanceSquared(int16_t first_x_mg,
                                      int16_t first_y_mg,
                                      int16_t first_z_mg,
                                      int16_t second_x_mg,
                                      int16_t second_y_mg,
                                      int16_t second_z_mg)
{
  int32_t delta_x = (int32_t)first_x_mg - second_x_mg;
  int32_t delta_y = (int32_t)first_y_mg - second_y_mg;
  int32_t delta_z = (int32_t)first_z_mg - second_z_mg;

  return ((uint64_t)(delta_x * (int64_t)delta_x) +
          (uint64_t)(delta_y * (int64_t)delta_y) +
          (uint64_t)(delta_z * (int64_t)delta_z));
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
  detector->reference_x_mg = 0;
  detector->reference_y_mg = 0;
  detector->reference_z_mg = 0;
  detector->previous_x_mg = 0;
  detector->previous_y_mg = 0;
  detector->previous_z_mg = 0;
  detector->still_reference_x_mg = 0;
  detector->still_reference_y_mg = 0;
  detector->still_reference_z_mg = 0;
  detector->motion_active = 0U;
  detector->pending_startup_shock = 0U;
  detector->shock_started_while_moving = 0U;
  detector->reference_valid = 0U;
  detector->previous_valid = 0U;
  detector->still_reference_valid = 0U;
}

uint8_t EventDetector_Update(EventDetector *detector, int16_t x_mg,
                             int16_t y_mg, int16_t z_mg)
{
  uint8_t events = EVENT_DETECTOR_NONE;
  uint32_t magnitude_squared;
  uint16_t variation_threshold_mg;
  uint16_t motion_confirm_samples;
  uint16_t still_confirm_samples;
  uint16_t shock_max_samples;
  uint8_t above_shock;
  uint8_t pose_changed = 0U;
  uint8_t sample_varied = 0U;
  uint8_t inside_still_region = 0U;
  uint8_t motion_sample;

  if (detector == NULL)
  {
    return events;
  }

  magnitude_squared = AccelerationSquared(x_mg, y_mg, z_mg);
  variation_threshold_mg = (uint16_t)(detector->config.motion_delta_mg / 3U);
  if (variation_threshold_mg < 25U)
  {
    variation_threshold_mg = 25U;
  }
  motion_confirm_samples = SamplesForMilliseconds(
    detector, detector->config.motion_confirm_ms);
  still_confirm_samples = SamplesForMilliseconds(
    detector, detector->config.still_confirm_ms);
  shock_max_samples = SamplesForMilliseconds(
    detector, detector->config.shock_max_duration_ms);

  above_shock = (magnitude_squared >=
                 ThresholdSquared(detector->config.shock_threshold_mg)) ? 1U : 0U;

  if (detector->reference_valid != 0U)
  {
    pose_changed = (VectorDistanceSquared(
      x_mg, y_mg, z_mg,
      detector->reference_x_mg,
      detector->reference_y_mg,
      detector->reference_z_mg) >=
      ThresholdSquared(detector->config.motion_delta_mg)) ? 1U : 0U;
  }
  else
  {
    detector->reference_x_mg = x_mg;
    detector->reference_y_mg = y_mg;
    detector->reference_z_mg = z_mg;
    detector->reference_valid = 1U;
  }

  if (detector->previous_valid != 0U)
  {
    sample_varied = (VectorDistanceSquared(
      x_mg, y_mg, z_mg,
      detector->previous_x_mg,
      detector->previous_y_mg,
      detector->previous_z_mg) >=
      ThresholdSquared(variation_threshold_mg)) ? 1U : 0U;
  }
  detector->previous_x_mg = x_mg;
  detector->previous_y_mg = y_mg;
  detector->previous_z_mg = z_mg;
  detector->previous_valid = 1U;

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
    motion_sample = (uint8_t)(pose_changed |
      ((sample_varied != 0U) && (above_shock == 0U) &&
       (detector->pending_startup_shock == 0U)));
    if (motion_sample != 0U)
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
      detector->still_reference_x_mg = x_mg;
      detector->still_reference_y_mg = y_mg;
      detector->still_reference_z_mg = z_mg;
      detector->still_reference_valid = 1U;
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
  else
  {
    if (detector->still_reference_valid != 0U)
    {
      inside_still_region = (VectorDistanceSquared(
        x_mg, y_mg, z_mg,
        detector->still_reference_x_mg,
        detector->still_reference_y_mg,
        detector->still_reference_z_mg) <=
        ThresholdSquared(detector->config.motion_delta_mg)) ? 1U : 0U;
    }

    if (inside_still_region != 0U)
    {
      IncrementSaturated(&detector->still_samples, still_confirm_samples);
    }
    else
    {
      detector->still_reference_x_mg = x_mg;
      detector->still_reference_y_mg = y_mg;
      detector->still_reference_z_mg = z_mg;
      detector->still_reference_valid = 1U;
      detector->still_samples = 0U;
    }

    if (detector->still_samples >= still_confirm_samples)
    {
      detector->motion_active = 0U;
      detector->still_samples = 0U;
      detector->motion_evidence_samples = 0U;
      detector->pending_startup_shock = 0U;
      detector->reference_x_mg = x_mg;
      detector->reference_y_mg = y_mg;
      detector->reference_z_mg = z_mg;
      detector->reference_valid = 1U;
      detector->still_reference_valid = 0U;
      events |= EVENT_DETECTOR_MOTION_END;
    }
  }

  return events;
}

uint8_t EventDetector_IsMotionActive(const EventDetector *detector)
{
  return (detector != NULL) ? detector->motion_active : 0U;
}
