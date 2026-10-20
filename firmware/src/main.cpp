/**
 * @file Dual_Display_Firmware.ino
 * @author Intellar (https://github.com/intellar)
 * @brief Main firmware for the dual-display animated eye project.
 * @version 1.1
 *
 * @copyright Copyright (c) 2024
 *
 * @license See LICENSE.md for details.
 *
 */

#include <Arduino.h>
#include "config.h"
// #include "esp32-hal-log.h" // Désactivé pour le débogage
#include "LittleFS.h"
// --- Bibliothèques désactivées pour le débogage ---
#include "drawing_tools.h"
#include "eye_logic.h"
#include "tof_sensor.h"

// --- FPS Counter Variables ---
// --- LED Blink Configuration ---
#define LED_PIN 48 // Broche de la LED intégrée. Changez-la si nécessaire (ex: LED_BUILTIN, 2, etc.)

// --- Battery Monitoring ---
#define BATT_ADC_PIN 4 // Broche ADC pour la lecture de la tension de la batterie


// --- FPS Counter Variables ---
static unsigned long last_fps_time = 0;
static int frame_count = 0;
static float current_fps = 0.0f;


// --- Debugging ---

/**
 * @brief Lit la tension de la batterie et la retourne en pourcentage.
 * @return Le pourcentage de batterie restant (0-100).
 */
int get_battery_percentage() {
  // Lit la valeur brute de l'ADC (0-4095)
  uint32_t raw_value = analogRead(BATT_ADC_PIN);

  // Convertit la valeur brute en millivolts à la broche ADC
  // La référence de tension est d'environ 3.3V (3300mV) pour une lecture max de 4095
  float adc_voltage = (raw_value / 4095.0) * 3300.0;

  // La tension de la batterie est le double de la tension lue à cause du diviseur de tension
  float battery_voltage = adc_voltage * 2.0;

  // Mappe la tension de la batterie (3.2V-4.2V) à un pourcentage (0-100%)
  // map(valeur, min_entree, max_entree, min_sortie, max_sortie)
  int percentage = map(battery_voltage, 3200, 4200, 0, 100);
  return constrain(percentage, 0, 100); // S'assure que la valeur reste entre 0 et 100
}
/**
 * @brief Initializes all subsystems.
 */
void setup() {
  // Ajout d'un délai fixe pour garantir que le moniteur série a le temps de se connecter.
  delay(2000);

  Serial.begin(115200);
  // Attend que le port série soit connecté. Indispensable pour l'ESP32-S3 avec USB natif.
  // Ajout d'un timeout pour ne pas bloquer si le moniteur n'est pas ouvert.
  unsigned long start_time = millis();
  while (!Serial && (millis() - start_time < 2000)) {
    delay(100);
  }

  Serial.println("Booting Dual Display Firmware...");
  Serial.flush(); // Force l'envoi des données
  // Initialise la broche de la LED comme une sortie
  pinMode(LED_PIN, OUTPUT);

  // Boton de modo a GND: el pull-up interno deja el pin en HIGH en reposo.
  pinMode(PIN_MODE_BUTTON, INPUT_PULLUP);
  

  // Initialize LittleFS for asset loading
  // Le 'true' en second paramètre formate le système de fichiers s'il ne peut pas être monté.
  // C'est utile pour la première initialisation ou après une corruption.
  if (!LittleFS.begin(true)) {
    Serial.println("FATAL: LittleFS format/mount failed. Halting.");
    Serial.flush();
    // Si même le formatage échoue, il y a un problème matériel ou de configuration.
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Clignotement rapide pour signaler une erreur fatale
      delay(100);
    }
  }

  sleep(1);

  // --- Initialisation de l'écran et du capteur désactivée pour le débogage ---
  // Initialize displays and load graphical assets
  init_tft();
  
  // Perform an initial clear of both physical screens to ensure a clean state
  clear_all_screens(TFT_BLACK);
  delay(50); // Short delay to ensure screens are cleared
  
  // Show splash screen to user while the rest initializes
  show_splash_screen();
  delay(1000); // Keep splash visible for a moment
  //
  // Initialize the ToF sensor (this part is slow)
  #if USE_TOF_SENSOR
    init_tof_sensor();
  #endif
  // --- Fin de la section désactivée ---

  Serial.println("Initialization complete. Starting main loop.");
  Serial.flush();
}

/**
 * @brief Devuelve true una sola vez por pulsacion del boton de modo.
 * El boton va a GND con el pull-up interno, de modo que en reposo se lee HIGH
 * y pulsado LOW. Solo se informa del flanco de PULSACION; el de suelta se
 * ignora, que es lo que evita que una pulsacion cuente como dos cambios.
 *
 * Sobre el antirrebote: este sondeo corre una vez por fotograma, y a los
 * 10-16 FPS que da el firmware eso son 60-100 ms entre lecturas, ya mas que
 * MODE_BUTTON_DEBOUNCE_MS. Es decir que hoy el filtro casi no trabaja: esta
 * para que el comportamiento siga siendo correcto si el ritmo sube. La
 * consecuencia real de sondear tan despacio es la otra: un toque mas corto
 * que un fotograma se puede perder, hay que pulsar con normalidad.
 */
static bool mode_button_pressed(unsigned long now) {
  static int last_raw = HIGH;
  static int stable_state = HIGH;
  static unsigned long last_raw_change = 0;

  int raw = digitalRead(PIN_MODE_BUTTON);
  if (raw != last_raw) {
    last_raw = raw;
    last_raw_change = now;
  }
  if (raw != stable_state && (now - last_raw_change) >= MODE_BUTTON_DEBOUNCE_MS) {
    stable_state = raw;
    return (stable_state == LOW);
  }
  return false;
}

/**
 * @brief Tres destellos en las dos pantallas: acuse de recibo visible del
 * cambio de modo. El color dice a donde se va (rojo = solo ojo rojo, blanco =
 * vuelta a normal), de modo que no hace falta mirar el ojo para saberlo.

 * Escribe directo al panel con clear_all_screens(), sin pasar por los
 * framebuffers, asi que no corrompe el fotograma que se esta montando: el
 * siguiente ciclo del bucle vuelve a dibujar el ojo encima.
 * Bloquea el bucle unos MODE_FLASH_COUNT * (ON + OFF) ms, y es deliberado. Un
 * destello repartido entre fotogramas a 10 FPS no se leeria como destello, y
 * durante esos ~420 ms lo unico que se deja de hacer es leer el sensor.
 */
static void flash_mode_change(uint16_t color) {
  for (int i = 0; i < MODE_FLASH_COUNT; i++) {
    clear_all_screens(color);
    delay(MODE_FLASH_ON_MS);
    clear_all_screens(TFT_BLACK);
    delay(MODE_FLASH_OFF_MS);
  }
}


// Forward declaration for the main application logic
void main_loop();

/**
 * @brief Main application loop.
 */
void loop() {
  // Call the main application logic
  main_loop();
}

void main_loop() {
  // --- FPS Calculation ---
  frame_count++;
  unsigned long current_millis = millis();
  if (current_millis - last_fps_time >= 1000) {
    // Calculate FPS over the last second
    current_fps = frame_count / ((current_millis - last_fps_time) / 1000.0f);
    last_fps_time = current_millis;
    frame_count = 0;
    Serial.printf("FPS: %.1f\n", current_fps);
    Serial.flush();
  }

  #if BUTTON_DEBUG
  {
    static unsigned long ultimo = 0;
    static int anterior = -1;
    int crudo = digitalRead(PIN_MODE_BUTTON);
    // Imprime cada medio segundo Y ademas en cuanto el pin cambie, para que no
    // se pierda una pulsacion corta entre dos impresiones periodicas.
    if (crudo != anterior || current_millis - ultimo >= 500) {
      if (crudo != anterior) Serial.print("*CAMBIO* ");
      anterior = crudo;
      ultimo = current_millis;
      Serial.print("BOTON pin 15 = ");
      Serial.println(crudo == HIGH ? "HIGH (reposo)" : "LOW (pulsado)");
    }
  }
  #endif


  // --- 0. Boton de modo ---
  // Antes de leer el sensor, para que el fotograma que viene ya se dibuje con
  // el modo nuevo.
  if (mode_button_pressed(current_millis)) {
    toggle_eye_mode();
    Serial.printf("MODO: %s\n", get_eye_mode() == EYE_MODE_RED_ONLY
                                    ? "SOLO ROJO (textura fija)"
                                    : "NORMAL (la textura sigue al sensor)");
    Serial.flush();
    flash_mode_change(get_eye_mode() == EYE_MODE_RED_ONLY ? MODE_FLASH_COLOR_RED_ONLY
                                                          : MODE_FLASH_COLOR_NORMAL);
  }


  // --- 1. Sensor Update ---
  #if USE_TOF_SENSOR
    #if TOF_CALIBRATION_MODE
      // In calibration mode, force an update on every frame
      update_tof_sensor_data();
    #else
    update_tof_sensor_data();
    #endif
  #endif
  TofTarget target = get_tof_target();

  // --- Telemetria temporal del ToF (ver TOF_TELEMETRY en config.h) ---
  // Imprime lo que ve el sensor y lo que acaba recibiendo el ojo. La posicion
  // del ojo va por detras del objetivo por el suavizado: comparar las dos
  // columnas dice si esta siguiendo de verdad y en que eje.
  #if USE_TOF_SENSOR && TOF_TELEMETRY
  {
    static unsigned long last_telemetry = 0;
    if (current_millis - last_telemetry >= TOF_TELEMETRY_PERIOD_MS) {
      last_telemetry = current_millis;
      EyePosition ep = get_eye_position(EYE_LEFT);
      if (target.is_valid) {
        Serial.printf("ToF OBJETIVO zona(col=%d,fil=%d) %4d mm  target x=%+.2f y=%+.2f   ojo x=%+.2f y=%+.2f\n",
                      target.min_dist_pixel_x, target.min_dist_pixel_y,
                      target.distance_mm, target.x, target.y, ep.x, ep.y);
      } else {
        Serial.printf("ToF --nada--                              (mirada libre)  ojo x=%+.2f y=%+.2f\n",
                      ep.x, ep.y);
      }
    }
  }
  #endif

  // --- 2. Eye Position Logic ---
  // Update the logical positions of the eyes based on the target
  update_eye_positions(target);

  // Update the blink state machine (shared by both eyes, so they blink in sync)
  update_blink();
  uint8_t blink_level = get_blink_level();

  // Update the pupil dilation state (shared by both eyes)
  update_pupil_dilation(target);
  float pupil_dilation = get_pupil_dilation();

  // --- 3. Drawing ---
  for (int i = 0; i < NUM_EYES; i++) {
    select_screen(i);
    clear_buffer(TFT_BLACK);

    // Get the final calculated position and image type for the current eye
    EyePosition pos = get_eye_position(i);
    EyeImageType image_type = get_current_eye_image_type(target);

    // Draw the eye at its final calculated position (eyelid level animated by update_blink(),
    // pupil dilation animated by update_pupil_dilation())
    draw_eye_at_target(pos.x, pos.y, blink_level, image_type, pupil_dilation);

    // Optional: Draw the ToF debug grid on one of the screens
    #if USE_TOF_SENSOR && SHOW_TOF_DEBUG_GRID
      if (i == EYE_RIGHT) { // Draw only on the right eye screen
        const int16_t grid_size = 80;
        const int16_t grid_pos = (SCR_WD - grid_size) / 2;
        // Get raw sensor data for display
        const VL53L5CX_ResultsData* tof_data = get_tof_measurement_data();
        // Use the same 'target' that was used for the eye movement
       draw_tof_debug_grid(grid_pos, grid_pos, grid_size, tof_data, target.min_dist_pixel_x, target.min_dist_pixel_y);

        // --- Draw FPS Counter ---
        char fps_str[10];
        dtostrf(current_fps, 4, 1, fps_str); // Format float to string (width 4, 1 decimal)
        char display_str[15];

        // --- Draw Battery Level ---
        int batt_level = get_battery_percentage();

        sprintf(display_str, "FPS:%s B:%d%%", fps_str, batt_level);
        drawString_fb(display_str, 5, 5, TFT_WHITE);
      }
    #endif
  }

  // --- 4. Display Update ---
  // Push the completed framebuffers to the physical screens
  display_all_buffers();
}