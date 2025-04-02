#include <WiFi.h>
#include <WebServer.h>
#include <Sankalp_inferencing.h>
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
bool ledState = false;

WebServer server(80);

// Camera configuration (same as before)
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

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

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
}

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
    // Capture image
    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if (!snapshot_buf) {
        server.send(500, "text/plain", "Failed to allocate snapshot buffer");
        return;
    }

    if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
        free(snapshot_buf);
        server.send(500, "text/plain", "Failed to capture image");
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
        server.send(500, "text/plain", "Failed to run classifier");
        return;
    }

    // Format results
    String response = "<h2>Inference Results</h2>";
    response += "<p>DSP: " + String(result.timing.dsp) + "ms, Classification: " + String(result.timing.classification) + "ms</p>";

    
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    response += "<ul>";
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) continue;
        response += "<li>" + String(bb.label) + " (" + String(bb.value, 2) + ") [x:" + bb.x + ", y:" + bb.y + ", w:" + bb.width + ", h:" + bb.height + "]</li>";
    }
    response += "</ul>";
#else
    response += "<ul>";
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        response += "<li>" + String(ei_classifier_inferencing_categories[i]) + ": " + String(result.classification[i].value, 3) + "</li>";
    }
    response += "</ul>";
#endif

#if EI_CLASSIFIER_HAS_ANOMALY
    response += "<p>Anomaly score: " + String(result.anomaly, 3) + "</p>";
#endif

    server.send(200, "text/html", response);
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

// Camera functions (same as before)
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

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif