/**
 * @file eye_logic.cpp
 * @author Intellar (https://github.com/intellar)
 * @brief Implementation of the eye movement and state logic.
 * @version 1.0
 *
 * @copyright Copyright (c) 2025
 *
 * @license See LICENSE.md for details.
 *
 */
#include "eye_logic.h" // Correctly includes the header from the 'include' path
#include "config.h"
#include <Arduino.h>
#include <math.h> // for powf(), used by the blink easing curve

// --- Module-Private State ---

// Global state for each eye's position
static EyePosition eye_positions[NUM_EYES];

// Idle gaze: which pose from GAZE_POSES the eye is currently holding, where
// exactly it's aiming (pose + jitter), and when it will pick the next one.
static int current_pose_index = 0; // Index 0 is the straight-ahead pose.
static float gaze_target_x = 0.0f;
static float gaze_target_y = 0.0f;
static unsigned long next_gaze_change_at = 0;

/**
 * @brief Picks the next idle gaze pose and how long to hold it.
 * Weighted random over GAZE_POSES, rejecting the pose currently held so the
 * eye never "moves" to where it already is (which would read as a stall).
 */
static void pick_next_gaze_pose(unsigned long now) {
    // Total weight of every pose EXCEPT the one we're on.
    long total_weight = 0;
    for (int i = 0; i < GAZE_POSE_COUNT; i++) {
        if (i != current_pose_index) total_weight += GAZE_POSES[i].weight;
    }

    long r = random(total_weight);
    int chosen = current_pose_index; // Fallback if every other weight is 0.
    for (int i = 0; i < GAZE_POSE_COUNT; i++) {
        if (i == current_pose_index) continue;
        r -= GAZE_POSES[i].weight;
        if (r < 0) {
            chosen = i;
            break;
        }
    }
    current_pose_index = chosen;

    // Aim at the pose plus a small random offset, so a given pose is never
    // hit at exactly the same pixel twice (see GAZE_POSE_JITTER in config.h).
    float jitter_x = (random(-100, 101) / 100.0f) * GAZE_POSE_JITTER;
    float jitter_y = (random(-100, 101) / 100.0f) * GAZE_POSE_JITTER;
    gaze_target_x = constrain(GAZE_POSES[chosen].x + jitter_x, -1.0f, 1.0f);
    gaze_target_y = constrain(GAZE_POSES[chosen].y + jitter_y, -1.0f, 1.0f);

    unsigned long dwell = random(GAZE_DWELL_MIN_MS, GAZE_DWELL_MAX_MS + 1);
    next_gaze_change_at = now + (unsigned long)(dwell * GAZE_POSES[chosen].dwell_factor);
}

// Tracking state variables
static unsigned long last_track_time = 0;
static float last_known_target_x = 0.0f;
static float last_known_target_y = 0.0f;

/**
 * @brief Updates the eye positions based on the sensor target.
 * This function contains the core logic for switching between tracking a target
 * and performing idle saccade movements.
 * @param target The target data from the ToF sensor.
 */
void update_eye_positions(const TofTarget& target) {
    float final_target_x, final_target_y;
    // Which of the two motion modes applies this frame: a ballistic saccade
    // between idle poses, or smooth pursuit of a tracked target. They move at
    // very different speeds (see the GAZE section in config.h).
    bool idle_saccade = false;

    if (target.is_valid) {
        // A valid target is present, so we aim for it.
        final_target_x = target.x;
        final_target_y = target.y;
        last_track_time = millis(); // Update the time we last had a valid track

        // Store the last known good position
        last_known_target_x = target.x;
        last_known_target_y = target.y;
    } else {
        // No valid target, switch to idle behavior.
        #if TOF_CALIBRATION_MODE
            // Hold the last known position so it can be read off the screen.
            final_target_x = last_known_target_x;
            final_target_y = last_known_target_y;
        #else
            unsigned long now = millis();
            // Only start looking around once enough time has passed since the
            // target was lost, so the eye doesn't immediately abandon a spot
            // it was tracking a moment ago.
            if (now - last_track_time > SACCADE_DELAY_AFTER_TRACK_MS && now >= next_gaze_change_at) {
                pick_next_gaze_pose(now);
            }
            final_target_x = gaze_target_x;
            final_target_y = gaze_target_y;
            idle_saccade = true;
        #endif
    }

    // Saccades snap, pursuit glides. Using one speed for both is what makes an
    // idle eye look like it's drifting instead of looking around.
    const float speed = idle_saccade ? GAZE_SACCADE_SPEED : LERP_SPEED;

    // Smoothly interpolate (LERP) each eye's position towards the final target.
    // Both eyes share the same target: real gaze is conjugate, the eyes move
    // together, so they are deliberately not desynchronised here.
    for (int i = 0; i < NUM_EYES; i++) {
        eye_positions[i].x += (final_target_x - eye_positions[i].x) * speed;
        eye_positions[i].y += (final_target_y - eye_positions[i].y) * speed;
    }
}

EyePosition get_eye_position(int eye_index) {
    if (eye_index >= 0 && eye_index < NUM_EYES) {
        return eye_positions[eye_index];
    }
    return EyePosition{0.0f, 0.0f}; // Return a default/safe value by explicitly constructing it
}

EyeImageType get_current_eye_image_type(const TofTarget& target) {
    return target.is_valid ? EYE_IMAGE_BAD : EYE_IMAGE_NORMAL;
}

// --- Blink State Machine -------------------------------------------------

enum BlinkState {
    BLINK_IDLE,    // Eyes open, waiting for the next scheduled blink
    BLINK_CLOSING, // Eyelid moving from open -> closed
    BLINK_HOLDING, // Eyelid staying closed briefly
    BLINK_OPENING  // Eyelid moving from closed -> open
};

static BlinkState blink_state = BLINK_IDLE;
static unsigned long blink_state_started_at = 0;
static unsigned long next_blink_at = 0;
static uint8_t current_blink_level = 0; // 0 = open, BLINK_LEVEL_MAX = closed
static bool blink_initialized = false;

// Smooth ease (in/out) so the lid doesn't move at a constant, mechanical speed.
static float ease_in_out(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

static unsigned long random_blink_delay() {
    return random(BLINK_MIN_INTERVAL_MS, BLINK_MAX_INTERVAL_MS + 1);
}

void update_blink() {
    unsigned long now = millis();

    if (!blink_initialized) {
        // First call: schedule the first blink a bit into the future.
        next_blink_at = now + random_blink_delay();
        blink_initialized = true;
    }

    switch (blink_state) {
        case BLINK_IDLE:
            current_blink_level = 0;
            if (now >= next_blink_at) {
                blink_state = BLINK_CLOSING;
                blink_state_started_at = now;
            }
            break;

        case BLINK_CLOSING: {
            unsigned long elapsed = now - blink_state_started_at;
            if (elapsed >= BLINK_CLOSE_DURATION_MS) {
                current_blink_level = BLINK_LEVEL_MAX;
                blink_state = BLINK_HOLDING;
                blink_state_started_at = now;
            } else {
                float t = (float)elapsed / (float)BLINK_CLOSE_DURATION_MS;
                current_blink_level = (uint8_t)(ease_in_out(t) * BLINK_LEVEL_MAX);
            }
            break;
        }

        case BLINK_HOLDING: {
            unsigned long elapsed = now - blink_state_started_at;
            current_blink_level = BLINK_LEVEL_MAX;
            if (elapsed >= BLINK_HOLD_DURATION_MS) {
                blink_state = BLINK_OPENING;
                blink_state_started_at = now;
            }
            break;
        }

        case BLINK_OPENING: {
            unsigned long elapsed = now - blink_state_started_at;
            if (elapsed >= BLINK_OPEN_DURATION_MS) {
                current_blink_level = 0;
                blink_state = BLINK_IDLE;
                next_blink_at = now + random_blink_delay();
            } else {
                float t = (float)elapsed / (float)BLINK_OPEN_DURATION_MS;
                current_blink_level = (uint8_t)((1.0f - ease_in_out(t)) * BLINK_LEVEL_MAX);
            }
            break;
        }
    }
}

uint8_t get_blink_level() {
    return current_blink_level;
}

// --- Pupil Dilation -------------------------------------------------------
//
// Built from two independent layers (see the numbered sections in config.h):
//   (3) excursions - a random resting level, re-picked on its own timer,
//       smoothed by the asymmetric response speeds in (1). This is the
//       "commanded" level, and it's what ToF (4) replaces when active.
//   (2) hippus    - a continuous ripple added AFTER the smoothing.
//
// The hippus is added after, not before, on purpose: it's a property of the
// pupil itself, not a stimulus being tracked, so it should not be attenuated
// or distorted by the response filter. Feeding it through the filter would
// flatten it and skew it downwards (the asymmetric speeds bias a filtered
// oscillation towards its fast direction).
//
// Nothing here reads the gaze. The pupil runs on its own clocks by design.

static float current_pupil_dilation = 0.0f;  // 0.0 = baked size, 1.0 = fully dilated. Final output.
static float commanded_dilation = 0.5f;      // Smoothed level, before hippus is added.
static float excursion_level = 0.5f;         // Current randomly-picked resting level.
static unsigned long next_excursion_at = 0;
static bool excursion_initialized = false;

static unsigned long random_excursion_delay() {
    return random(DILATION_EXCURSION_MIN_INTERVAL_MS, DILATION_EXCURSION_MAX_INTERVAL_MS + 1);
}

static float random_excursion_level() {
    // random() is integer-only, so work in hundredths and scale back down.
    long lo = (long)(DILATION_EXCURSION_MIN_LEVEL * 100.0f);
    long hi = (long)(DILATION_EXCURSION_MAX_LEVEL * 100.0f);
    return random(lo, hi + 1) / 100.0f;
}

/**
 * @brief Continuous background ripple ("hippus"), in dilation units.
 * Sum of two non-harmonic sines, so it has no perceptible repeat period.
 * Each phase is taken modulo its own period to keep the sinf() argument
 * small: feeding a raw, ever-growing millis() into sinf() would slowly lose
 * float precision as uptime increases.
 */
static float pupil_hippus(unsigned long now) {
    float phase_a = (now % DILATION_HIPPUS_PERIOD_A_MS) / (float)DILATION_HIPPUS_PERIOD_A_MS;
    float phase_b = (now % DILATION_HIPPUS_PERIOD_B_MS) / (float)DILATION_HIPPUS_PERIOD_B_MS;
    return DILATION_HIPPUS_AMPLITUDE * 0.5f * (sinf(TWO_PI * phase_a) + sinf(TWO_PI * phase_b));
}

void update_pupil_dilation(const TofTarget& target) {
    unsigned long now = millis();

#if DILATION_TEST_MODE
    // Continuous 0->1->0 sweep (~3s), bypassing every layer below, so the
    // overlay itself can be verified and tuned on the hardware.
    current_pupil_dilation = 0.5f * (1.0f - cosf(now * 0.002f));
    return;
#endif

    float target_dilation;

#if USE_TOF_SENSOR
    if (target.is_valid) {
        // Closer target -> more dilated (alert/startle reaction).
        float t = (float)(DILATION_FAR_MM - target.distance_mm) / (float)(DILATION_FAR_MM - DILATION_NEAR_MM);
        target_dilation = constrain(t, 0.0f, 1.0f);
    } else
#endif
    {
        // No usable ToF target: drift between random resting levels on our
        // own timer.
        if (!excursion_initialized) {
            next_excursion_at = now + random_excursion_delay();
            excursion_initialized = true;
        }
        if (now >= next_excursion_at) {
            excursion_level = random_excursion_level();
            next_excursion_at = now + random_excursion_delay();
        }
        target_dilation = excursion_level;
    }

    // Asymmetric response: slow to open, quick to close, as a real pupil.
    float speed = (target_dilation > commanded_dilation) ? DILATION_DILATE_SPEED
                                                         : DILATION_CONSTRICT_SPEED;
    commanded_dilation += (target_dilation - commanded_dilation) * speed;

    current_pupil_dilation = constrain(commanded_dilation + pupil_hippus(now), 0.0f, 1.0f);
}

float get_pupil_dilation() {
    return current_pupil_dilation;
}