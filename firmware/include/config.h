/**
 * @file config.h
 * @author Intellar (https://github.com/intellar)
 * @brief Configuration file for the animated eye project.
 * @version 1.0
 *
 * @copyright Copyright (c) 2025
 *
 * @license See LICENSE.md for details.
 *
 */
#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware & Pinout Configuration ---
#define PIN_CS1 TFT_CS_L // Chip-select for screen 1
#define PIN_CS2 TFT_CS_R // Chip-select for screen 2

// --- ToF Sensor (VL53L5CX) Configuration ---
#define USE_TOF_SENSOR 0 // Set to 1 to enable the ToF sensor, 0 to disable it.
#define TOF_CALIBRATION_MODE 0 // Set to 1 to simulate sensor data for debugging.
#define SHOW_TOF_DEBUG_GRID 0 // Set to 1 to display the debug grid, 0 to hide it

#define PIN_TOF_SCL 8
#define PIN_TOF_SDA 18
#define PIN_TOF_INT -1

// --- Nombre d'écrans (yeux) ---
#define NUM_EYES 2 // Mettre à 1 pour un seul oeil, 2 pour deux yeux

// --- Display & Image Configuration ---
#define SCR_WD 240 // Screen width in pixels
#define SCR_HT 240 // Screen height in pixels
#define DRAW_AREA_SQUARE 1 // Set to 1 for a square drawing area, 0 for circular.

#define EYE_IMAGE_WIDTH  350
#define EYE_IMAGE_HEIGHT 350
const uint16_t TRANSPARENT_COLOR_KEY = 0x0000; // The color in assets treated as transparent (black).

// --- Asset File Paths ---
static const char* EYE_IMAGE_NORMAL_PATH = "/image_giant.bin"; // Image for random/idle mode
static const char* EYE_IMAGE_BAD_PATH = "/image_giant_bad.bin"; // Image for tracking mode

// --- Saccade (Eye Movement) Behavior ---
// Controls how the eye darts around.
const unsigned long SACCADE_DELAY_AFTER_TRACK_MS = 2000; // Time to wait before starting random movement after losing a target.
const unsigned long SACCADE_INTERVAL_MS = 1500;     // Time between saccades (darting movements).
const float LERP_SPEED = 0.2;                       // Interpolation speed (0.0 to 1.0). Higher is faster/jerkier.
const int MAX_2D_OFFSET_PIXELS = 55;                // Max pixels the eye can move from the center.
const int RESTING_2D_OFFSET_PIXELS = (SCR_WD - EYE_IMAGE_WIDTH) / 2; // The "resting" position for the eye.


// --- ToF Sensor Behavior ---
const int MAX_DIST_TOF = 400; // Maximum distance in mm to consider a ToF target "close".

// --- Blink Behavior ---
// Controls automatic, natural-looking eyelid blinking.
// *** MODO PRUEBA: parpadeo muy frecuente y lento, solo para verificar que
// *** funciona. Una vez confirmado, restaurar a los valores normales
// *** (2500-7000 / 90 / 40 / 110).
const unsigned long BLINK_MIN_INTERVAL_MS = 2500;
const unsigned long BLINK_MAX_INTERVAL_MS = 7000;
const unsigned long BLINK_CLOSE_DURATION_MS = 190;
const unsigned long BLINK_HOLD_DURATION_MS = 90;
const unsigned long BLINK_OPEN_DURATION_MS = 110;
const uint8_t BLINK_LEVEL_MAX = 128;               // Fully closed value expected by draw_eye_image().

// How curved the eyelid boundary is: the eye "opening" stays taller at the
// horizontal center and shrinks first near the corners (like a real almond-
// shaped eyelid), instead of closing as a flat horizontal line.
// Bigger value = more pronounced curve.
const int16_t EYELID_ARC_AMPLITUDE_PX = 60;

// Anchura sobre la que se reparte la cupula del arco.
// OJO: la textura del ojo mide EYE_IMAGE_WIDTH (350 px) pero la pantalla solo
// muestra SCR_WD (240 px) centrados. Si el arco se reparte sobre los 350 px,
// las columnas visibles caen todas en la zona plana de la cupula y la
// diferencia entre el centro y los bordes visibles es de apenas un 27% de la
// amplitud -> el parpado parece RECTO. Repartiendolo sobre el ancho visible,
// el borde de la pantalla cae justo en el extremo de la cupula y se aprecia
// la curva completa.
const int16_t EYELID_ARC_WIDTH_PX = SCR_WD;

// Pixeles de apertura restante a partir de los cuales el parpado se cierra del
// todo de golpe. Sin esto, las ultimas 1-4 filas que quedan abiertas caen justo
// sobre la banda central del globo ocular (esclerotica blanca a ambos lados del
// iris) y destellan como una linea blanquecina en el instante del cierre.
// El efecto se nota mas cuanto mas lento sea BLINK_CLOSE_DURATION_MS.
// Subir a 6-8 si todavia se aprecia; bajar a 2 si el cierre parece "saltar".
const int16_t EYELID_SNAP_CLOSED_PX = 4;



#endif // CONFIG_H