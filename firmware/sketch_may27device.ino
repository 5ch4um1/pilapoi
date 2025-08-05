#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// --- WiFi Configuration for THIS ESP32 (Target) ---
// Connect to the Controller's AP
const char* ap_ssid = "ESP32_POI";       // SSID for the Access Point
const char* ap_password = "your-supersecret-wifi-password";   // Password for the Access Point (min 8 characters)

// Static IP address configuration for THIS ESP32 (Target)
// These must match the network configured on the Controller's AP
IPAddress static_ip(10, 0, 0, 2);      // Desired static IP for this Target ESP32 (e.g., 10.0.0.2)
IPAddress gateway(10, 0, 0, 1);        // Controller's AP IP is the gateway (e.g., 10.0.0.1)
IPAddress subnet(255, 255, 255, 0);       // Subnet mask for the AP network
IPAddress primaryDNS(8, 8, 8, 8);         // Optional: Google DNS, or you can use gateway IP for AP's DNS if it has one
IPAddress secondaryDNS(8, 8, 4, 4);       // Optional: Google DNS

// --- NeoPixel Configuration for THIS ESP32 (Target) ---
#define DATA_PIN 10          // ESP32 GPIO pin connected to its own NeoPixel data input
#define NUM_LEDS 9           // Number of NeoPixels in this ESP32's strip
#define DEFAULT_BRIGHTNESS 100 // Master brightness (0-255)
#define LED_TYPE WS2812B     // Type of NeoPixel (most common is WS2812B)
#define COLOR_ORDER GRB      // Color order (GRB is common for WS2812B)

CRGB leds[NUM_LEDS];         // FastLED LED array for THIS ESP32

WebServer server(80);        // Create a web server on port 80 for THIS ESP32

// Global variable for current brightness (accessed by both tasks)
volatile uint8_t currentBrightness = DEFAULT_BRIGHTNESS;

// --- Pattern Management ---
enum Pattern {
    OFF,
    ON,
    FIRE2012,
    RAINBOW_CYCLE,
    TWINKLE,
    PACIFICA,
    NOISE,
    CONF_ETTI,
    SINE_LON,
    JUGGLER,
    NOISE_FLOW,
    RIPPLE,
    BPM,
    FADE_IN_OUT,
    RAINBOW_MARCH,
    LARSON_SCANNER,
    RAINBOW_BEAT,
    MATRIX_RAIN,
    COLOR_WAVES,
    STROBE,
    COMET,
    NOISE_PLASMA,
    FIREFLIES,
    RAINBOW_WAVES // Replaces Gradient
    // BOUNCING_BALL REMOVED
};

// Volatile keyword ensures proper access from different tasks
volatile Pattern activePattern = OFF; // Initial pattern state for THIS ESP32's LEDs

// --- Pattern specific variables (GLOBAL scope, accessed by LED task) ---
// Fire2012 variables
static byte cooling = 55;
static byte sparking = 120;
static byte FRAMES_PER_SECOND = 60;
bool gReverseDirection = false;

// Twinkle variables
#define TWINKLE_BRIGHTNESS 255
#define TWINKLE_DECAY 8
#define TWINKLE_CHANCE 20

// Noise variables
uint16_t xscale = 3000;
uint16_t yscale = 3000;
uint16_t zscale = 3000;
uint16_t tscale = 10;
uint16_t noise_hue = 0;

// Sinelon variables
uint8_t sinelon_hue = 0;
uint8_t sinelon_bpm = 30;

// Juggler variables
uint8_t juggler_hue = 0;
uint8_t juggler_fade = 20;
uint8_t juggler_dots = 4;
uint8_t juggler_beats = 10;

// Noise Flow variables
uint16_t noise_x = 0;
uint16_t noise_y = 0;
uint16_t noise_z = 0;
uint16_t noise_scale = 1000;
uint8_t noise_flow_hue = 0;

// Ripple variables
uint8_t ripple_center = 0;
int8_t ripple_dir = 1;
uint8_t ripple_hue = 0;
uint8_t ripple_speed = 4;
uint8_t ripple_fade = 30;

// BPM variables
uint8_t gHue = 0; // FastLED global hue, used for some patterns

// FadeInOut variables (Modified for smoother transitions)
CRGB currentFadeColor = CRGB::Red; // Initial color, will cycle
unsigned long fadeStartTime = 0;
int fadeDuration = 8000; // 8 seconds per fade cycle for smoother effect

// Rainbow March variables (inherits from global hue)

// Rainbow Beat variables
// No specific global variables needed, uses gHue from BPM

// Matrix Rain variables
byte matrix_color_hue = HUE_GREEN; // Green "code" color
static uint8_t matrix_fade_rate = 15; // How quickly the trails fade

// Color Waves variables
uint8_t wave_hue = 0;
uint8_t wave_bpm = 15; // Speed of the waves

// Strobe variables
bool strobe_state = false;
unsigned long last_strobe_toggle = 0;
int strobe_delay_ms = 50; // milliseconds between on/off states

// Larson Scanner variables
int larson_pos = 0;
int larson_dir = 1;
CRGB larson_color = CRGB::Red; // Default color for the scanner
int larson_head_size = 4; // Size of the scanner head
int larson_fade_speed = 40; // How quickly the trail fades (higher = faster fade)

// Comet Tail variables (NEW)
int comet_pos = 0;
int comet_dir = 1;
uint8_t comet_fade_amount = 30; // How fast the comet trail fades (higher = shorter trail)
uint8_t comet_hue = 0;

// Perlin Noise Plasma variables (NEW)
uint16_t noise_plasma_x = 0;
uint16_t noise_plasma_y = 0;
uint16_t noise_plasma_scale = 100; // Scale of the noise field
uint8_t noise_plasma_hue_speed = 1; // How fast the hue shifts

// Fireflies variables (NEW)
#define FIREFLIES_CHANCE 5 // Chance out of 100 for a new firefly to appear
#define FIREFLIES_FADE_AMOUNT 15 // How fast fireflies fade (higher = faster)

// Rainbow Waves variables (NEW - replaces Gradient)
uint16_t rainbow_wave_offset = 0;
uint8_t rainbow_wave_speed = 1; // Adjust for slower/faster waves
uint8_t rainbow_wave_density = 5; // How many waves fit on the strip (lower = wider waves)

// BOUNCING_BALL (REMOVED)


// --- Function to turn off all LEDs ---
void turnOffLeds() {
    activePattern = OFF;
    FastLED.setBrightness(0); // Temporarily set brightness to 0 to turn off
    fill_solid(leds, NUM_LEDS, CRGB::Black); // Ensure all pixels are black
    FastLED.show(); // Push the black pixels to the strip
    FastLED.setBrightness(currentBrightness); // Restore brightness, but LEDs remain off due to black fill
}

// --- Pattern Functions for THIS (Target) device (called from LED Task) ---
// Note: All delay() and FastLED.delay() calls have been removed or replaced with EVERY_N_MILLISECONDS
// The LED task will control the overall frame rate.

void runFire2012() {
    static byte heat[NUM_LEDS];
    EVERY_N_MILLISECONDS(1000 / FRAMES_PER_SECOND) { // Control framerate
        for( int i = 0; i < NUM_LEDS; i++) {
            heat[i] = qsub8( heat[i], random8(0, ((cooling * 10) / NUM_LEDS) + 2));
        }
        for( int k= NUM_LEDS - 1; k >= 2; k--) {
            heat[k] = (heat[k-1] + heat[k-2] + heat[k-2] ) / 3;
        }
        if( random8() < sparking ) {
            int y = random8(7);
            heat[y] = qadd8( heat[y], random8(160,255) );
        }
        for( int j = 0; j < NUM_LEDS; j++) {
            CRGB color = HeatColor( heat[j]);
            int pixelnumber;
            if( gReverseDirection ) {
                pixelnumber = (NUM_LEDS - 1) - j;
            } else {
                pixelnumber = j;
            }
            leds[pixelnumber] = color;
        }
        FastLED.show();
    }
}

void runRainbowCycle() {
    static uint8_t hue = 0;
    EVERY_N_MILLISECONDS(20) { // Update every 20ms for smooth animation
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CHSV(hue + (i * 255 / NUM_LEDS), 255, 255);
        }
        FastLED.show();
        hue++;
        if (hue >= 255) hue = 0;
    }
}

void runTwinkle() {
    EVERY_N_MILLISECONDS(50) { // Control twinkle speed
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].nscale8(255 - TWINKLE_DECAY);
        }
        if (random8(100) < TWINKLE_CHANCE) {
            int pixel = random16(NUM_LEDS);
            leds[pixel] = CHSV(random8(), 255, TWINKLE_BRIGHTNESS);
        }
        FastLED.show();
    }
}

void pacifica_loop_helper() {
  static unsigned long pacifica_phase1 = 0;
  static unsigned long pacifica_phase2 = 0;
  pacifica_phase1 += beatsin8( 10, 1, 4 );
  pacifica_phase2 += beatsin8( 12, 2, 6 );

  for( int i=0; i<NUM_LEDS; ++i ) {
    uint8_t wave_val = qadd8( cos8( (i*16) + pacifica_phase1), sin8( (i*23) + pacifica_phase2));
    leds[i] = CHSV( 140, 255, wave_val);
  }
}
void runPacifica() {
  EVERY_N_MILLISECONDS(20) { // Update rate for Pacifica
    pacifica_loop_helper();
    FastLED.show();
  }
}

void runNoise() {
    static uint16_t x = random16();
    static uint16_t y = random16();
    static uint16_t z = random16();
    static uint16_t t = random16();

    EVERY_N_MILLISECONDS(10) { // Control noise animation speed
        x += xscale;
        y += yscale;
        z += zscale;
        t += tscale;

        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t noise = inoise8(x + i * 100, y, z);
            leds[i] = CHSV(noise_hue + noise, 255, noise);
        }
        FastLED.show();
    }
}

void runConfetti() {
    EVERY_N_MILLISECONDS(10) { // Control confetti speed
        fadeToBlackBy(leds, NUM_LEDS, 10);
        int pos = random16(NUM_LEDS);
        leds[pos] += CHSV(random8(), 200, 255);
        FastLED.show();
    }
}

void runSinelon() {
    EVERY_N_MILLISECONDS(10) { // Control sinelon speed
        fadeToBlackBy(leds, NUM_LEDS, 20);
        int pos = beatsin16(sinelon_bpm, 0, NUM_LEDS - 1);
        leds[pos] += CHSV(sinelon_hue++, 255, 192);
        FastLED.show();
    }
}

void runJuggler() {
    EVERY_N_MILLISECONDS(10) { // Control juggler speed
        fadeToBlackBy(leds, NUM_LEDS, juggler_fade);
        for (int i = 0; i < juggler_dots; i++) {
            leds[beatsin16(juggler_beats + i * 7, 0, NUM_LEDS - 1)] |= CHSV(juggler_hue + i * 32, 255, 255);
        }
        FastLED.show();
        juggler_hue++;
    }
}

void runNoiseFlow() {
    EVERY_N_MILLISECONDS(20) { // Control noise flow speed
        noise_x += 100;
        noise_y += 50;
        noise_z += 25;

        for (int i = 0; i < NUM_LEDS; i++) {
            uint16_t x_offset = noise_x + (i * noise_scale);
            uint16_t y_offset = noise_y;
            uint16_t z_offset = noise_z;

            uint8_t noise_val = inoise8(x_offset, y_offset, z_offset);
            leds[i] = CHSV(noise_flow_hue + (noise_val / 4), 255, noise_val);
        }
        noise_flow_hue++;
        FastLED.show();
    }
}

void runRipple() {
    EVERY_N_MILLISECONDS(ripple_speed) { // Control ripple speed
        fadeToBlackBy(leds, NUM_LEDS, ripple_fade);

        leds[constrain(ripple_center, 0, NUM_LEDS - 1)] = CHSV(ripple_hue, 255, 255);

        ripple_center += ripple_dir;

        if (ripple_center >= NUM_LEDS || ripple_center < 0) {
            ripple_dir *= -1;
            ripple_center = constrain(ripple_center, 0, NUM_LEDS - 1);
            ripple_hue += 30;
        }
        FastLED.show();
    }
}

void runBPM() {
    uint8_t beatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8( beatsPerMinute, 64, 255);
    EVERY_N_MILLISECONDS(10) { // Control BPM speed
        for( int i = 0; i < NUM_LEDS; i++) {
            leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
        }
        FastLED.show();
        gHue++;
    }
}

// MODIFIED: Smoother Fade In/Out
void runFadeInOut() {
    EVERY_N_MILLISECONDS(10) { // Fine-grained updates for smooth fade
        unsigned long elapsed = millis() - fadeStartTime;
        if (elapsed > fadeDuration) {
            fadeStartTime = millis();
            elapsed = 0;
            // Cycle through hue for the next color instead of random
            gHue += 30; // Increment global hue for the next fade color
            currentFadeColor = CHSV(gHue, 255, 255);
        }
        // Use beatsin8 to create a smooth up/down fade
        uint8_t brightness = beatsin8(10, 0, 255, 0, elapsed * 255 / fadeDuration);
        
        fill_solid(leds, NUM_LEDS, currentFadeColor);
        FastLED.setBrightness(brightness);
        FastLED.show();
        FastLED.setBrightness(currentBrightness);
    }
}

void runRainbowMarch() {
    static uint8_t startIndex = 0;
    EVERY_N_MILLISECONDS(20) { // Control march speed
        fill_rainbow(leds, NUM_LEDS, startIndex, 255 / NUM_LEDS);
        FastLED.show();
        startIndex++;
    }
}

void runLarsonScanner() {
    EVERY_N_MILLISECONDS(20) { // Adjust speed
        fadeToBlackBy(leds, NUM_LEDS, larson_fade_speed);

        for (int i = 0; i < larson_head_size; i++) {
            int pixel = larson_pos - i;
            if (pixel >= 0 && pixel < NUM_LEDS) {
                uint8_t brightness = map(i, 0, larson_head_size - 1, 255, 50);
                leds[pixel] = ColorFromPalette(HeatColors_p, brightness);
            }
        }

        FastLED.show();

        larson_pos += larson_dir;

        if (larson_pos >= NUM_LEDS + (larson_head_size -1) || larson_pos < 0) {
            larson_dir *= -1;
            if (larson_pos >= NUM_LEDS + (larson_head_size - 1)) larson_pos = NUM_LEDS + (larson_head_size -1) -1;
            if (larson_pos < 0) larson_pos = 0;
            delay(100); // Small pause at ends. This is the only place a delay is acceptable within an EVERY_N_MILLIS block.
        }
    }
}

void runRainbowBeat() {
    EVERY_N_MILLISECONDS(10) { // Control beat speed
        uint8_t beatA = beatsin8(17, 0, 255);
        uint8_t beatB = beatsin8(13, 0, 255);
        fill_rainbow(leds, NUM_LEDS, (beatA + beatB) / 2, 8);
        FastLED.show();
    }
}

void runMatrixRain() {
    EVERY_N_MILLISECONDS(30) { // Adjust for speed
        fadeToBlackBy(leds, NUM_LEDS, matrix_fade_rate);
        if (random8(10) < 3) {
            leds[random16(NUM_LEDS)] = CHSV(matrix_color_hue, 255, 255);
        }
        FastLED.show();
    }
}

void runColorWaves() {
    static uint16_t s_x = 0;
    static uint16_t s_scale = 30;

    EVERY_N_MILLISECONDS(20) { // Control wave speed
        s_x += wave_bpm;

        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t wave_value = qadd8( qadd8( sin8(i*10 + s_x), cos8(i*15 + s_x/2) ), sin8(i*20 + s_x/4) );
            leds[i] = CHSV( wave_hue + (i * 4), 255, wave_value);
        }
        wave_hue++;
        FastLED.show();
    }
}

void runStrobe() {
    EVERY_N_MILLISECONDS(1) { // Check frequently
        if (millis() - last_strobe_toggle >= strobe_delay_ms) {
            strobe_state = !strobe_state;
            last_strobe_toggle = millis();
        }
        if (strobe_state) {
            fill_solid(leds, NUM_LEDS, CRGB::White);
        } else {
            fill_solid(leds, NUM_LEDS, CRGB::Black);
        }
        FastLED.show();
    }
}

// EXISTING PATTERN: Comet Tail
void runCometTail() {
    EVERY_N_MILLISECONDS(30) { // Adjust speed of comet
        fadeToBlackBy(leds, NUM_LEDS, comet_fade_amount); // Fade the tail

        leds[comet_pos] = CHSV(comet_hue, 255, 255); // Head of the comet

        comet_pos += comet_dir;
        if (comet_pos >= NUM_LEDS || comet_pos < 0) {
            comet_dir *= -1; // Reverse direction
            comet_hue += 60; // Change color on direction change
        }
        FastLED.show();
    }
}

// EXISTING PATTERN: Perlin Noise Plasma
void runNoisePlasma() {
    EVERY_N_MILLISECONDS(20) { // Smooth animation speed
        noise_plasma_x += noise_plasma_hue_speed; // Animate x-offset for horizontal flow
        noise_plasma_y += noise_plasma_hue_speed / 2; // Animate y-offset for vertical flow (slower)

        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t noise = inoise8(noise_plasma_x + (i * noise_plasma_scale), noise_plasma_y);
            // Map noise value to a color palette for smooth transitions
            leds[i] = ColorFromPalette(RainbowColors_p, noise);
        }
        FastLED.show();
    }
}

// EXISTING PATTERN: Fireflies
void runFireflies() {
    EVERY_N_MILLISECONDS(50) { // Update rate for fireflies
        fadeToBlackBy(leds, NUM_LEDS, FIREFLIES_FADE_AMOUNT); // Fade existing fireflies

        if (random8(100) < FIREFLIES_CHANCE) { // Chance to spawn a new firefly
            int pixel = random16(NUM_LEDS);
            leds[pixel] = CHSV(random8(), 200, 255); // Random color firefly
        }
        FastLED.show();
    }
}

// NEW PATTERN: Rainbow Waves (replaces Gradient)
void runRainbowWaves() {
    EVERY_N_MILLISECONDS(20) { // Speed of the waves
        for (int i = 0; i < NUM_LEDS; i++) {
            // Calculate hue based on position and a rolling offset for wave effect
            uint8_t hue = (i * rainbow_wave_density + rainbow_wave_offset) % 255;
            // Apply a sine wave to brightness for undulating effect
            uint8_t brightness = beatsin8(15, 60, 255, 0, i * 10); // 15 BPM, min 60, max 255 brightness
            leds[i] = CHSV(hue, 255, brightness);
        }
        FastLED.show();
        rainbow_wave_offset += rainbow_wave_speed; // Shift the wave
    }
}

// BOUNCING_BALL (REMOVED)


// --- Web Server Handlers for incoming commands (called from WebServer Task) ---

void handleSetPattern() {
    String patternName = server.arg("name");
    Serial.print("Received pattern command: ");
    Serial.println(patternName);

    // These changes to activePattern are read by the LED task
    if (patternName == "off") {
        activePattern = OFF;
        fill_solid(leds, NUM_LEDS, CRGB::Black); // Ensure black is sent immediately
        FastLED.setBrightness(currentBrightness); // Restore brightness, in case it was 0 for OFF
        FastLED.show();
    } else if (patternName == "on") {
        activePattern = ON;
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.setBrightness(currentBrightness);
        FastLED.show();
    } else if (patternName == "fire") { activePattern = FIRE2012; }
    else if (patternName == "rainbow") { activePattern = RAINBOW_CYCLE; }
    else if (patternName == "twinkle") { activePattern = TWINKLE; }
    else if (patternName == "pacifica") { activePattern = PACIFICA; }
    else if (patternName == "noise") { activePattern = NOISE; }
    else if (patternName == "confetti") { activePattern = CONF_ETTI; }
    else if (patternName == "sinelon") { activePattern = SINE_LON; }
    else if (patternName == "juggler") { activePattern = JUGGLER; }
    else if (patternName == "noise_flow") { activePattern = NOISE_FLOW; }
    else if (patternName == "ripple") {
        activePattern = RIPPLE;
        ripple_center = NUM_LEDS / 2;
        ripple_dir = 1;
        ripple_hue = random8();
    }
    else if (patternName == "bpm") { activePattern = BPM; gHue = 0; }
    else if (patternName == "fade_in_out") {
        activePattern = FADE_IN_OUT;
        fadeStartTime = millis();
        gHue = random8(); // Start with random hue, then cycle
        currentFadeColor = CHSV(gHue, 255, 255);
    }
    else if (patternName == "rainbow_march") { activePattern = RAINBOW_MARCH; }
    else if (patternName == "larson_scanner") {
        activePattern = LARSON_SCANNER;
        larson_pos = 0; larson_dir = 1; larson_color = CRGB::Red;
    } else if (patternName == "rainbow_beat") { activePattern = RAINBOW_BEAT; }
    else if (patternName == "matrix_rain") { activePattern = MATRIX_RAIN; matrix_color_hue = HUE_GREEN; }
    else if (patternName == "color_waves") { activePattern = COLOR_WAVES; wave_hue = 0; }
    else if (patternName == "strobe") { activePattern = STROBE; strobe_state = false; last_strobe_toggle = millis(); }
    // Existing long exposure patterns
    else if (patternName == "comet_tail") {
        activePattern = COMET;
        comet_pos = 0; comet_dir = 1; comet_hue = random8();
    }
    else if (patternName == "noise_plasma") {
        activePattern = NOISE_PLASMA;
        noise_plasma_x = random16(); noise_plasma_y = random16();
    }
    else if (patternName == "fireflies") {
        activePattern = FIREFLIES;
        fill_solid(leds, NUM_LEDS, CRGB::Black); // Clear strip on start
    }
    // New patterns
    else if (patternName == "rainbow_waves") {
        activePattern = RAINBOW_WAVES;
        rainbow_wave_offset = 0; // Reset offset
    }
    // BOUNCING_BALL REMOVED

    server.send(200, "text/plain", "OK");
}

void handleSetBrightness() {
    uint8_t newBrightness = server.arg("value").toInt();
    Serial.printf("Received remote brightness command: %d\n", newBrightness);
    currentBrightness = newBrightness; // Volatile global var
    FastLED.setBrightness(currentBrightness);
    // FastLED.show(); // No need to call show here, LED task will do it
    server.send(200, "text/plain", "OK");
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// --- FreeRTOS Tasks ---

// Task to handle LED patterns
void ledTask(void *pvParameters) {
    (void) pvParameters; // Suppress unused parameter warning

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(currentBrightness);
    turnOffLeds(); // Initialize to off

    Serial.println("LED Task running on Core ");
    Serial.println(xPortGetCoreID());

    for (;;) { // Infinite loop for the task
        // Ensure brightness is set for the current frame
        FastLED.setBrightness(currentBrightness);

        switch (activePattern) {
            case OFF:
                fill_solid(leds, NUM_LEDS, CRGB::Black);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(500)); // Sleep when off to save CPU
                break;
            case ON:
                fill_solid(leds, NUM_LEDS, CRGB::White);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(100)); // Sleep when on, no animation
                break;
            case FIRE2012:
                runFire2012();
                break;
            case RAINBOW_CYCLE:
                runRainbowCycle();
                break;
            case TWINKLE:
                runTwinkle();
                break;
            case PACIFICA:
                runPacifica();
                break;
            case NOISE:
                runNoise();
                break;
            case CONF_ETTI:
                runConfetti();
                break;
            case SINE_LON:
                runSinelon();
                break;
            case JUGGLER:
                runJuggler();
                break;
            case NOISE_FLOW:
                runNoiseFlow();
                break;
            case RIPPLE:
                runRipple();
                break;
            case BPM:
                runBPM();
                break;
            case FADE_IN_OUT:
                runFadeInOut();
                break;
            case RAINBOW_MARCH:
                runRainbowMarch();
                break;
            case LARSON_SCANNER:
                runLarsonScanner();
                break;
            case RAINBOW_BEAT:
                runRainbowBeat();
                break;
            case MATRIX_RAIN:
                runMatrixRain();
                break;
            case COLOR_WAVES:
                runColorWaves();
                break;
            case STROBE:
                runStrobe();
                break;
            case COMET:
                runCometTail();
                break;
            case NOISE_PLASMA:
                runNoisePlasma();
                break;
            case FIREFLIES:
                runFireflies();
                break;
            case RAINBOW_WAVES: // NEW
                runRainbowWaves();
                break;
            // BOUNCING_BALL REMOVED
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to yield CPU if patterns are very fast or do not have internal timing
    }
}

// Task to handle Web Server operations
void webServerTask(void *pvParameters) {
    (void) pvParameters; // Suppress unused parameter warning

    server.on("/setpattern", HTTP_GET, handleSetPattern);
    server.on("/setbrightness", HTTP_GET, handleSetBrightness);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started (Target) on Core ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        server.handleClient(); // This runs the web server.
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield CPU to other tasks
    }
}


// --- Setup ---
void setup() {
    Serial.begin(115200);

    Serial.print("Connecting to AP ");
    Serial.println(ap_ssid);

    // Set a static IP for the Target ESP32, matching the Controller's AP network
    if (!WiFi.config(static_ip, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("STA Failed to configure static IP");
    }

    // Connect to the Controller's AP
    WiFi.begin(ap_ssid, ap_password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected to AP");
    Serial.print("Target ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    // Create the LED task on Core 0
    xTaskCreatePinnedToCore(
        ledTask,         // Task function.
        "LED Task",      // Name of task.
        4096,            // Stack size of task (bytes).
        NULL,            // Parameter of the task.
        1,               // Priority of the task.
        NULL,            // Task handle to keep track of created task.
        0);              // Core where task should run (Core 0 for FastLED).

    // Create the Web Server task on Core 1
    xTaskCreatePinnedToCore(
        webServerTask,   // Task function.
        "Web Server Task", // Name of task.
        8192,            // Stack size of task (bytes, larger for network).
        NULL,            // Parameter of the task.
        1,               // Priority of the task.
        NULL,            // Task handle to keep track of created task.
        1);              // Core where task should run (Core 1 for networking).

    // setup() finishes, and tasks run independently.
}

// --- Loop ---
// The loop() function is now largely empty as tasks handle everything.
void loop() {
    // Nothing to do here. Tasks handle everything.
}
