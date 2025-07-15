
#include <WiFi.h>
#include <WebServer.h>
#include <Aniket_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include "esp32-hal-ledc.h"

// Select camera model
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

// WiFi credentials
const char* ssid = "1";
const char* password = "12345678";

// LED pin (built-in LED on AI Thinker is GPIO 4)
#define LED_PIN 4
#define OUTPUT_PIN 12 // Pin to control Arduino (GPIO 12)
bool ledState = false;
bool cropDetected = false;
float cropConfidenceThreshold = 0.7; // 70% confidence threshold

WebServer server(80);

// Camera configuration
#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#endif

// Camera settings
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS   320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS   240
#define EI_CAMERA_FRAME_BYTE_SIZE         3

static bool debug_nn = false;
static bool is_initialised = false;
uint8_t* snapshot_buf = NULL;

camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// Function declarations
bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t* out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float* out_ptr);
void handleRoot();
void handleInference();
void handleLED();
void handleStream();
void runInference();

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(OUTPUT_PIN, LOW);

    // Initialize camera
    if (!ei_camera_init()) {
        Serial.println("Failed to initialize camera!");
        return;
    }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Setup server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/inference", HTTP_GET, handleInference);
    server.on("/led", HTTP_GET, handleLED);
    server.on("/stream", HTTP_GET, handleStream);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
    // Run periodic inference check
    static unsigned long lastInferenceTime = 0;
    if (millis() - lastInferenceTime > 1000) { // Run inference every second
        lastInferenceTime = millis();
        runInference();
    }
}

void runInference() {
    // Capture image
    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if (!snapshot_buf) {
        Serial.println("Failed to allocate snapshot buffer");
        return;
    }

    if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
        free(snapshot_buf);
        Serial.println("Failed to capture image");
        return;
    }

    // Run inference
    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    free(snapshot_buf);

    if (err != EI_IMPULSE_OK) {
        Serial.println("Failed to run classifier");
        return;
    }

    // Check for crop detection
    bool newCropDetected = false;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    // For object detection models
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value >= cropConfidenceThreshold) {
            if (strstr(bb.label, "crop") != NULL) { // Check if label contains "crop"
                newCropDetected = true;
                break;
            }
        }
    }
#else
    // For classification models
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (strstr(ei_classifier_inferencing_categories[i], "crop") != NULL) { // Check if label contains "crop"
            if (result.classification[i].value >= cropConfidenceThreshold) {
                newCropDetected = true;
                break;
            }
        }
    }
#endif

    // Update output pin if state changed
    if (newCropDetected != cropDetected) {
        cropDetected = newCropDetected;
        digitalWrite(OUTPUT_PIN, cropDetected ? HIGH : LOW);
        Serial.println(cropDetected ? "Crop detected - OUTPUT HIGH" : "No crop detected - OUTPUT LOW");
    }
}

// Camera functions
bool ei_camera_init(void) {
    if (is_initialised) return true;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }

    is_initialised = true;
    return true;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t* out_buf) {
    if (!is_initialised) {
        Serial.println("Camera not initialized");
        return false;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return false;
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, out_buf);
    esp_camera_fb_return(fb);

    if (!converted) {
        Serial.println("Conversion failed");
        return false;
    }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float* out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

// Web server handlers
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>ESP32-CAM Inference</title>";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>body{font-family:Arial;text-align:center;margin-top:20px;}";
    html += "button{padding:10px 20px;font-size:16px;}</style></head><body>";
    html += "<h1>ESP32-CAM Edge Impulse</h1>";
    html += "<img id=\"stream\" src=\"/stream\" width=\"320\" height=\"240\">";
    html += "<div id=\"result\"></div>";
    html += "<p><button onclick=\"toggleLED()\">Toggle LED</button> Current state: <span id=\"ledState\">OFF</span></p>";
    html += "<script>";
    html += "function updateInference(){fetch('/inference').then(r=>r.text()).then(t=>document.getElementById('result').innerHTML=t)}";
    html += "function toggleLED(){fetch('/led').then(r=>r.text()).then(t=>document.getElementById('ledState').innerText=t)}";
    html += "setInterval(updateInference, 1000);";
    html += "</script></body></html>";

    server.send(200, "text/html", html);
}

void handleInference() {
    runInference();
    server.send(200, "text/plain", cropDetected ? "Crop detected" : "No crop detected");
}

void handleLED() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    server.send(200, "text/plain", ledState ? "ON" : "OFF");
}

void handleStream() {
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            break;
        }

        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);
        client.write(fb->buf, fb->len);
        server.sendContent("\r\n");

        esp_camera_fb_return(fb);

        // Small delay to prevent watchdog trigger
        delay(10);
    }
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
=======
// Motor Control Pins (Avoiding conflicts)
#define ENA 9   // PWM for Motor A
#define ENB 10  // PWM for Motor B
#define IN1 5   // Changed from original 7 to avoid conflict
#define IN2 6
#define IN3 7   // Changed from original 5
#define IN4 4

// Spray Control Pins
#define sprayPin 8  // Changed from 7 to avoid motor conflict
#define relayPin 13

// Timing Control
#define RUN_TIME 500     // Movement duration (ms)
#define STOP_TIME 4000   // Stop/spray window (ms)
#define SPRAY_DURATION 1000  // Spray active time (ms)

// State Management
enum SystemState { MOVING, STOPPED };
SystemState currentState = MOVING;
unsigned long stateStartTime = 0;
bool hasSprayed = false;

void setup() {
  // Motor control setup
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Spray control setup
  pinMode(sprayPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Start with motors moving
  startMoving();
}

void loop() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - stateStartTime;

  switch(currentState) {
    case MOVING:
      if(elapsedTime >= RUN_TIME) {
        stopMotors();
        currentState = STOPPED;
        stateStartTime = currentTime;
        hasSprayed = false;  // Reset spray flag
      }
      break;

    case STOPPED:
      if(!hasSprayed && digitalRead(sprayPin) == HIGH) {
        activateSpray();
        hasSprayed = true;
        stateStartTime = currentTime; // Reset timer after spraying
      }
      
      if(elapsedTime >= (hasSprayed ? SPRAY_DURATION : 0) + STOP_TIME) {
        startMoving();
        currentState = MOVING;
        stateStartTime = currentTime;
      }
      break;
  }
}

void startMoving() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 60);  // 60% speed
  analogWrite(ENB, 60);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void activateSpray() {
  digitalWrite(relayPin, HIGH);
  delay(SPRAY_DURATION);  // Blocks only during spraying
  digitalWrite(relayPin, LOW);
}

