#ifndef EVENT_DETECTOR_H
#define EVENT_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define EVENT_DETECTOR_DEFAULT_SHOCK_THRESHOLD_MG 2200U
#define EVENT_DETECTOR_DEFAULT_MOTION_DELTA_MG 150U
#define EVENT_DETECTOR_DEFAULT_MOTION_CONFIRM_MS 1000U
#define EVENT_DETECTOR_DEFAULT_STILL_CONFIRM_MS 1500U
#define EVENT_DETECTOR_DEFAULT_SHOCK_MAX_DURATION_MS 200U

typedef struct
{
  uint16_t shock_threshold_mg;
  uint16_t motion_delta_mg;
  uint16_t motion_confirm_ms;
  uint16_t still_confirm_ms;
  uint16_t shock_max_duration_ms;
  uint16_t sample_interval_ms;
} EventDetectorConfig;

typedef struct
{
  EventDetectorConfig config;
  uint16_t motion_evidence_samples;
  uint16_t still_samples;
  uint16_t shock_samples;
  int16_t reference_x_mg;
  int16_t reference_y_mg;
  int16_t reference_z_mg;
  int16_t previous_x_mg;
  int16_t previous_y_mg;
  int16_t previous_z_mg;
  int16_t still_reference_x_mg;
  int16_t still_reference_y_mg;
  int16_t still_reference_z_mg;
  uint8_t motion_active;
  uint8_t pending_startup_shock;
  uint8_t shock_started_while_moving;
  uint8_t reference_valid;
  uint8_t previous_valid;
  uint8_t still_reference_valid;
} EventDetector;

enum
{
  EVENT_DETECTOR_NONE = 0U,
  EVENT_DETECTOR_SHOCK = 1U << 0,
  EVENT_DETECTOR_MOTION_START = 1U << 1,
  EVENT_DETECTOR_MOTION_END = 1U << 2
};

void EventDetector_Init(EventDetector *detector,
                        const EventDetectorConfig *config);
void EventDetector_Reset(EventDetector *detector);
uint8_t EventDetector_Update(EventDetector *detector, int16_t x_mg,
                             int16_t y_mg, int16_t z_mg);
uint8_t EventDetector_IsMotionActive(const EventDetector *detector);

#ifdef __cplusplus
}
#endif

#endif
