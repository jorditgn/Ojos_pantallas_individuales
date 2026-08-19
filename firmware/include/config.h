/**
 * @file config.h
 * @author Intellar (https://github.com/intellar)
 * @brief Archivo de configuracion del proyecto de ojo animado.
 * @version 1.0
 *
 * @copyright Copyright (c) 2025
 *
 * @license Ver LICENSE.md para los detalles.
 *
 */
#ifndef CONFIG_H
#define CONFIG_H

// --- Configuracion de hardware y patillaje ---
#define PIN_CS1 TFT_CS_L // Chip-select de la pantalla 1
#define PIN_CS2 TFT_CS_R // Chip-select de la pantalla 2

// --- Configuracion del sensor ToF (VL53L5CX) ---
#define USE_TOF_SENSOR 0 // Poner a 1 para activar el sensor ToF, 0 para desactivarlo.
#define TOF_CALIBRATION_MODE 0 // Poner a 1 para simular datos del sensor durante la depuracion.
#define SHOW_TOF_DEBUG_GRID 0 // Poner a 1 para mostrar la rejilla de depuracion, 0 para ocultarla

#define PIN_TOF_SCL 8
#define PIN_TOF_SDA 18
#define PIN_TOF_INT -1

// --- Numero de pantallas (ojos) ---
#define NUM_EYES 2 // Poner a 1 para un solo ojo, 2 para dos ojos

// --- Configuracion de pantalla e imagen ---
#define SCR_WD 240 // Ancho de la pantalla en pixeles
#define SCR_HT 240 // Alto de la pantalla en pixeles
#define DRAW_AREA_SQUARE 1 // Poner a 1 para area de dibujo cuadrada, 0 para circular.

#define EYE_IMAGE_WIDTH  350
#define EYE_IMAGE_HEIGHT 350
const uint16_t TRANSPARENT_COLOR_KEY = 0x0000; // Color de los assets tratado como transparente (negro).

// --- Rutas de los archivos de assets ---
static const char* EYE_IMAGE_NORMAL_PATH = "/image_giant.bin"; // Imagen para el modo aleatorio/reposo
static const char* EYE_IMAGE_BAD_PATH = "/image_giant_bad.bin"; // Imagen para el modo de seguimiento

// --- Geometria del movimiento ocular ---
const unsigned long SACCADE_DELAY_AFTER_TRACK_MS = 2000; // Espera antes de empezar el movimiento en reposo tras perder un objetivo.
const int MAX_2D_OFFSET_PIXELS = 55;                // Pixeles maximos que el ojo puede alejarse del centro.
const int RESTING_2D_OFFSET_PIXELS = (SCR_WD - EYE_IMAGE_WIDTH) / 2; // Posicion de "reposo" del ojo.

// =========================================================================
// MIRADA - (1) VELOCIDAD, SEGUN EL TIPO DE MOVIMIENTO
// =========================================================================
// Un ojo real tiene DOS modos de movimiento distintos, y usar una sola
// velocidad para ambos es lo que hace que se vea artificial:
//
//   - Sacadica: salto balistico entre puntos de interes. Es muy rapido
//     (30-80 ms en un ojo real) y termina con el ojo QUIETO. Es lo que hace
//     el ojo cuando mira por ahi sin nada que seguir.
//   - Persecucion lisa: seguimiento continuo de algo que se mueve. Es lento
//     y suave. Es lo que toca cuando el ToF esta dando un objetivo valido.
//
// Ambos son factores de LERP por fotograma a ~18 FPS: mas alto = mas rapido.
//
// OJO con subir GAZE_SACCADE_SPEED demasiado: a 18 FPS una sacadica de
// duracion realista serian 1-2 fotogramas, y eso no se lee como un salto
// sino como un teletransporte. Con 0.5 el ojo llega al 90% en unos 3-4
// fotogramas (~185 ms): se percibe como salto pero el movimiento se ve.
const float GAZE_SACCADE_SPEED = 0.5f; // Sacadicas en reposo (rapido).
const float LERP_SPEED = 0.2;          // Persecucion de objetivo ToF (lento y suave).

// =========================================================================
// MIRADA - (2) REPERTORIO DE POSES
// =========================================================================
// En reposo el ojo ya no elige coordenadas al azar dentro de un cuadrado.
// Elige entre un repertorio de poses con nombre (izquierda, derecha, al
// frente, arriba-izquierda...), que es lo que hace que la mirada se lea como
// intencionada en vez de como ruido.
//
// Dos detalles deliberados en los valores de la tabla:
//   - Los desplazamientos VERTICALES son mas cortos que los horizontales. Un
//     ojo real recorre bastante menos en vertical sin acompanar con la cabeza,
//     y ademas escaneamos el mundo mucho mas en horizontal que en vertical.
//   - Los pesos suman 100, asi que se leen directamente como porcentajes de
//     probabilidad. El peso alto del CENTRO es lo que produce el retorno
//     constante a la posicion primaria, sin necesidad de ninguna regla extra:
//     la mayoria de transiciones acaban pasando por el frente.
//
// dwell_factor multiplica la duracion de la fijacion en esa pose. El CENTRO
// lo tiene alto a proposito: un vistazo rapido a un lado y vuelta al frente,
// donde se descansa mas rato, es un patron muy humano.
struct GazePose {
    float x;             // -1.0 = izquierda, +1.0 = derecha
    float y;             // -1.0 = arriba,    +1.0 = abajo
    uint8_t weight;      // Probabilidad relativa de que se elija esta pose.
    float dwell_factor;  // Multiplicador de la duracion de la fijacion.
};

const GazePose GAZE_POSES[] = {
    //   x       y     peso  dwell   pose
    {  0.00f,  0.00f,   30,  1.8f }, // AL FRENTE
    { -1.00f,  0.00f,   14,  1.0f }, // IZQUIERDA
    {  1.00f,  0.00f,   14,  1.0f }, // DERECHA
    { -0.70f, -0.60f,    8,  1.0f }, // ARRIBA-IZQUIERDA
    {  0.70f, -0.60f,    8,  1.0f }, // ARRIBA-DERECHA
    {  0.00f, -0.70f,    7,  1.0f }, // ARRIBA
    {  0.00f,  0.55f,    7,  1.0f }, // ABAJO
    { -0.70f,  0.50f,    6,  1.0f }, // ABAJO-IZQUIERDA
    {  0.70f,  0.50f,    6,  1.0f }, // ABAJO-DERECHA
};
const int GAZE_POSE_COUNT = sizeof(GAZE_POSES) / sizeof(GAZE_POSES[0]);

// Desviacion aleatoria que se suma a la pose elegida, en unidades de mirada.
// Sin esto, nueve posiciones exactas se reconocen como repertorio en menos de
// un minuto: el ojo va siempre a los mismos pixeles. Con esto se conserva el
// vocabulario de poses reconocibles pero no se repite nunca literalmente.
// A 0.12 y con MAX_2D_OFFSET_PIXELS = 55, la desviacion maxima son ~6.6 px.
const float GAZE_POSE_JITTER = 0.12f;

// =========================================================================
// MIRADA - (3) DURACION DE LAS FIJACIONES
// =========================================================================
// Cuanto se queda quieto el ojo en una pose antes de saltar a la siguiente.
// Antes esto era un intervalo FIJO y sonaba a metronomo. Las fijaciones
// reales duran entre unos cientos de ms y varios segundos.
// La duracion elegida se multiplica ademas por el dwell_factor de la pose.
const unsigned long GAZE_DWELL_MIN_MS = 700;  // Fijacion mas corta.
const unsigned long GAZE_DWELL_MAX_MS = 2500; // Fijacion mas larga.


// --- Comportamiento del sensor ToF ---
const int MAX_DIST_TOF = 400; // Distancia maxima en mm para considerar "cercano" un objetivo ToF.

// --- Comportamiento del parpadeo ---
// Controla el parpadeo automatico de aspecto natural.
// *** MODO PRUEBA: parpadeo muy frecuente y lento, solo para verificar que
// *** funciona. Una vez confirmado, restaurar a los valores normales
// *** (2500-7000 / 90 / 40 / 110).
const unsigned long BLINK_MIN_INTERVAL_MS = 2500;
const unsigned long BLINK_MAX_INTERVAL_MS = 7000;

const unsigned long BLINK_CLOSE_DURATION_MS = 590;
const unsigned long BLINK_HOLD_DURATION_MS = 200;
const unsigned long BLINK_OPEN_DURATION_MS = 210;
const uint8_t BLINK_LEVEL_MAX = 128;               // Valor de cierre total que espera draw_eye_image().

// Cuanto se curva el borde del parpado: la "abertura" del ojo se mantiene mas
// alta en el centro horizontal y se estrecha antes por las comisuras (como un
// parpado real en forma de almendra), en vez de cerrarse como una linea
// horizontal recta. Valor mas alto = curva mas pronunciada.
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

// En un ojo real el parpado superior es mas grande y hace la mayor parte del
// recorrido de cierre; el inferior apenas sube. Este ratio reparte el cierre
// entre ambos: 0.70 = el superior recorre el 70% de la distancia total y el
// inferior el 30% restante. Con esto el punto donde se juntan los parpados
// ya no cae en el centro geometrico del ojo, sino desplazado hacia abajo,
// como en un parpadeo humano. La suma de ambos recorridos sigue llegando al
// cierre completo en BLINK_LEVEL_MAX, solo cambia el reparto entre los dos.
const float EYELID_UPPER_RATIO = 0.70f;

// El parpado inferior tambien es mas recto que el superior en un ojo real:
// su curva tipo almendra es mucho mas sutil. Este factor atenua la amplitud
// del arco (EYELID_ARC_AMPLITUDE_PX) solo para el borde inferior; el
// superior conserva la curva completa.
const float EYELID_LOWER_ARC_FACTOR = 0.30f;

// --- Comportamiento de la dilatacion de pupila ---
// Circulo negro superpuesto en tiempo real sobre la pupila ya horneada en la
// textura, centrado en el espacio de TEXTURA (fijo independientemente del
// movimiento del ojo, ya que la textura solo se traslada en pantalla, nunca se
// escala ni se rota). Medido directamente sobre image_giant.bin e
// image_giant_bad.bin: ambas tienen un disco negro solido de ~20 px de radio
// centrado exactamente en el centro geometrico de la textura, y el color del
// iris se extiende hasta unos 60 px antes de que empiece la esclerotica.
const int16_t PUPIL_CENTER_X = EYE_IMAGE_WIDTH / 2;  // 175
const int16_t PUPIL_CENTER_Y = EYE_IMAGE_HEIGHT / 2; // 175

// Radio de la pupila YA horneada en la textura (~20 px, medido). La
// superposicion tiene que arrancar aqui, no en 0: un circulo mas pequeno que
// la pupila horneada queda completamente tapado por ella, asi que mapear la
// dilatacion desde 0 dejaria sin efecto todos los valores por debajo de ~0.42
// y pareceria que el efecto directamente no funciona.
const int16_t PUPIL_BASE_RADIUS_PX = 20;

// Radio de la superposicion con pupil_dilation == 1.0. Se mantiene por debajo
// del radio del iris (~60 px) para que siempre quede algo de color de iris
// visible, incluso con la pupila dilatada al maximo. Con dilatacion 0.0 la
// superposicion vale exactamente PUPIL_BASE_RADIUS_PX, es decir, visualmente
// identica a la textura sin tocar.
const int16_t PUPIL_MAX_RADIUS_PX = 48;
const int16_t PUPIL_FEATHER_PX = 5; // Anchura del borde suavizado, en pixeles.

// =========================================================================
// DILATACION DE PUPILA - (1) VELOCIDAD DE RESPUESTA
// =========================================================================
// Con que rapidez se mueve la pupila hacia el nivel que se le ha ordenado.
// Las dos direcciones NO tienen la misma velocidad, y es deliberado: en un ojo
// real la constriccion es rapida (via parasimpatica, ~0.3 s) mientras que la
// dilatacion es lenta (via simpatica, 1-2 s). Usar una unica velocidad
// simetrica hace que la pupila parezca mecanica; esta asimetria es lo que mas
// contribuye a que se lea como organica.
// Ambos son factores de LERP por fotograma a ~18 FPS: mas alto = mas rapido.
const float DILATION_DILATE_SPEED = 0.055f;   // Al agrandarse (lento, ~1.2 s).
const float DILATION_CONSTRICT_SPEED = 0.15f; // Al encogerse (rapido, ~0.4 s).

// =========================================================================
// DILATACION DE PUPILA - (2) HIPPUS (ondulacion continua de fondo)
// =========================================================================
// Una pupila real nunca esta quieta: oscila constantemente incluso con luz
// estable. Eso se llama hippus, y su ausencia es lo que hace que una pupila
// estatica se lea como muerta. Esta siempre activo, se suma por encima de todo
// lo demas, y es lo que hace el ojo cuando no pasa nada en particular.
//
// Esta construido a partir de DOS senos cuyos periodos son no armonicos (no
// son multiplos entre si), y es a proposito. Un unico seno se reconoce como
// bucle en pocos ciclos; la suma de estos dos solo se realinea al cabo de
// ~5 minutos, asi que nunca se percibe como un patron que se repite.
const float DILATION_HIPPUS_AMPLITUDE = 0.15f;          // Profundidad de la ondulacion, en unidades de dilatacion (+/-).
const unsigned long DILATION_HIPPUS_PERIOD_A_MS = 4300; // Primer seno.
const unsigned long DILATION_HIPPUS_PERIOD_B_MS = 7100; // Segundo seno. Mantenerlo no armonico respecto a A.

// =========================================================================
// DILATACION DE PUPILA - (3) EXCURSIONES (cambios grandes aleatorios)
// =========================================================================
// Por encima del hippus, la pupila elige un nuevo nivel de reposo a intervalos
// aleatorios y deriva hacia el. Estos son los cambios grandes y evidentes.
//
// Esto tiene su PROPIO temporizador, deliberadamente sin relacion con
// GAZE_DWELL_MIN_MS/MAX_MS ni con los temporizadores del parpadeo. Compartir reloj
// con la mirada es justo lo que hacia que una version anterior se viera mal:
// cada dilatacion caia sobre un movimiento del ojo y toda la cara se leia como
// un mecanismo con un solo motor.
const float DILATION_EXCURSION_MIN_LEVEL = 0.10f; // Nivel de reposo mas contraido.
const float DILATION_EXCURSION_MAX_LEVEL = 0.95f; // Nivel de reposo mas dilatado.
const unsigned long DILATION_EXCURSION_MIN_INTERVAL_MS = 1800; // Espera mas corta antes de volver a elegir.
const unsigned long DILATION_EXCURSION_MAX_INTERVAL_MS = 4500; // Espera mas larga antes de volver a elegir.

// =========================================================================
// DILATACION DE PUPILA - (4) RESPUESTA AL SENSOR ToF
// =========================================================================
// Solo se usa cuando USE_TOF_SENSOR esta activado Y hay un objetivo valido.
// Mientras eso se cumple, la distancia sustituye a las excursiones aleatorias
// como nivel ordenado (objetivo mas cerca -> mas dilatada, una reaccion de
// alerta/susto). El hippus del apartado (2) sigue funcionando por encima, asi
// que la pupila sigue respirando en vez de congelarse a un tamano fijo
// mientras se esta siguiendo algo.
const int DILATION_NEAR_MM = 100;         // Distancia a la que la dilatacion llega a 1.0.
const int DILATION_FAR_MM = MAX_DIST_TOF; // Distancia a la que la dilatacion vale 0.0.

// =========================================================================
// DILATACION DE PUPILA - (5) DEPURACION
// =========================================================================
// Poner a 1 para saltarse por completo los apartados (1)-(4) y barrer la
// dilatacion 0->1->0 de forma continua (periodo ~3 s). Sirve para comprobar
// que la superposicion se dibuja bien y para ajustar PUPIL_MAX_RADIUS_PX /
// PUPIL_FEATHER_PX sin tener que esperar a los temporizadores aleatorios.
#define DILATION_TEST_MODE 0

#endif // CONFIG_H
