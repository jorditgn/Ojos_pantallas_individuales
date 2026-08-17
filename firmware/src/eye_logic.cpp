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

// Saccade (random movement) variables
static unsigned long last_saccade_time = 0;
static float saccade_target_x = 0.0f;
static float saccade_target_y = 0.0f;

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
        #if !TOF_CALIBRATION_MODE 
        if (millis() - last_track_time > SACCADE_DELAY_AFTER_TRACK_MS) {
            // If enough time has passed since losing a target, get a new random saccade target.
            if (millis() - last_saccade_time > SACCADE_INTERVAL_MS) {
                last_saccade_time = millis();
                saccade_target_x = random(-100, 101) / 100.0f;
                saccade_target_y = random(-100, 101) / 100.0f;
            }
        }
        #endif

        // If not in calibration mode, use the saccade target. Otherwise, hold the last known position.
        #if TOF_CALIBRATION_MODE
            final_target_x = last_known_target_x;
            final_target_y = last_known_target_y;
        #else
            final_target_x = saccade_target_x;
            final_target_y = saccade_target_y;
        #endif
    }

    // Smoothly interpolate (LERP) each eye's position towards the final target.
    for (int i = 0; i < NUM_EYES; i++) {
        eye_positions[i].x += (final_target_x - eye_positions[i].x) * LERP_SPEED;
        eye_positions[i].y += (final_target_y - eye_positions[i].y) * LERP_SPEED;
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