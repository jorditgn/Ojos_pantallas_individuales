/**
 * @file eye_logic.h
 * @author Intellar (https://github.com/intellar)
 * @brief Public interface for managing eye movement logic.
 * @version 1.0
 *
 * @copyright Copyright (c) 2025
 *
 * @license See LICENSE.md for details.
 *
 */
#ifndef EYE_LOGIC_H
#define EYE_LOGIC_H

#include "tof_sensor.h" // For TofTarget
#include "drawing_tools.h" // For EyeImageType

// State for a single eye's logical position
struct EyePosition {
    float x = 0.0f; // Current horizontal position (-1.0 to 1.0)
    float y = 0.0f; // Current vertical position (-1.0 to 1.0)

    // Explicit constructor to allow initialization with two floats
    EyePosition(float initial_x = 0.0f, float initial_y = 0.0f) : x(initial_x), y(initial_y) {}
};

// Updates the eye positions based on the sensor target and internal state (saccade/tracking)
void update_eye_positions(const TofTarget& target);

// Gets the current calculated position of a specific eye
EyePosition get_eye_position(int eye_index);

// --- Modos de funcionamiento (ver la seccion MODOS en config.h) ---
enum EyeMode {
    EYE_MODE_NORMAL = 0, // La textura cambia segun el sensor.
    EYE_MODE_RED_ONLY    // Textura fija en RED_ONLY_IMAGE.
};

// Alterna entre los dos modos. Lo llama el sondeo del boton.
void toggle_eye_mode();

// Modo activo ahora mismo.
EyeMode get_eye_mode();

// Determines which eye image to use based on the target's validity
EyeImageType get_current_eye_image_type(const TofTarget& target);

// Updates the internal blink state machine. Call once per frame, before drawing.
void update_blink();

// Gets the current eyelid closure level (0 = fully open, BLINK_LEVEL_MAX = fully closed).
// Same value is shared by both eyes so they blink in sync.
uint8_t get_blink_level();

// Updates the pupil dilation state. Call once per frame, before drawing.
// Runs on its own internal timers and does NOT depend on eye position, so the
// call order relative to update_eye_positions() does not matter.
void update_pupil_dilation(const TofTarget& target);

// Gets the current pupil dilation (0.0 = contracted/baked size, 1.0 = fully dilated).
// Same value is shared by both eyes.
float get_pupil_dilation();

#endif // EYE_LOGIC_H