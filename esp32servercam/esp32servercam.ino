#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "SD_MMC.h"
#include "FS.h"

// --- Pines AI-Thinker ESP32-CAM ---
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define LED_GPIO_NUM   4

// --- Streaming Headers ---
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

WebServer server(80);

// Credenciales por defecto
const char* DEFAULT_SSID = "TP-Link MIGUEL";
const char* DEFAULT_PASS = "jdam1825";

// --- MANEJO DE LED DIGITAL (ON/OFF) ---
void setLedState(const String& state) {
  // Normalizar entrada a minúsculas para evitar errores de tipeo
  String s = state;
  s.toLowerCase();
  
  if (s == "on" || s == "1" || s == "true") {
    digitalWrite(LED_GPIO_NUM, HIGH);
  } else {
    digitalWrite(LED_GPIO_NUM, LOW);
  }
}

// --- HANDLERS HTTP ---
void handleCapture() {
  // Leer parámetro 'led'. Si no existe, por defecto está apagado
  String ledArg = server.hasArg("led") ? server.arg("led") : "off";
  setLedState(ledArg);
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Error al capturar");
    return;
  }
  
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.sendContent((const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleStream() {
  String ledArg = server.hasArg("led") ? server.arg("led") : "off";
  setLedState(ledArg);

  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: ");
  client.print(_STREAM_CONTENT_TYPE);
  client.print("\r\n\r\n");

  unsigned long lastFrame = 0;
  while (client.connected()) {
    server.handleClient(); 
    
    if (millis() - lastFrame > 100) { 
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) { delay(10); continue; }

      client.print(_STREAM_BOUNDARY);
      char hdr[64];
      sprintf(hdr, _STREAM_PART, fb->len);
      client.print(hdr);
      client.write(fb->buf, fb->len);
      esp_camera_fb_return(fb);
      lastFrame = millis();
    }
    yield();
  }
}

void handleRoot() {
  String html = "<html><head><title>ESP32-CAM</title>";
  html += "<style>body{background:#1e1e1e;color:white;font-family:sans-serif;text-align:center;}";
  html += "img{max-width:90%;border-radius:8px;margin-top:20px;}";
  html += ".controls{margin:20px;padding:15px;background:#333;border-radius:8px;display:inline-block;}";
  html += "button{padding:10px 20px;margin:5px;cursor:pointer;font-size:16px;}";
  html += ".active{background:#4CAF50;color:white;}</style></head>";
  html += "<body><h1>ESP32-CAM Stream</h1>";
  html += "<div class='controls'>";
  html += "<p>Control de Iluminación</p>";
  html += "<button id='btnOn' onclick=\"setLed('on')\">LED ON</button> ";
  html += "<button id='btnOff' onclick=\"setLed('off')\">LED OFF</button><br><br>";
  html += "<button onclick=\"window.open('/capture?led=on', '_blank')\">📸 Foto con Luz</button> ";
  html += "<button onclick=\"window.open('/capture?led=off', '_blank')\">📷 Foto sin Luz</button><br><br>";
  html += "<button onclick=\"startStream('on')\">▶️ Stream CON Luz</button> ";
  html += "<button onclick=\"startStream('off')\">⏹️ Stream SIN Luz</button>";
  html += "</div><br>";
  html += "<img id='camImg' src='' style='display:none;' />";
  html += "<script>";
  html += "function setLed(state){";
  html += "  fetch('/capture?led='+state).then(()=>{";
  html += "    document.getElementById('btnOn').className = state==='on'?'active':'';";
  html += "    document.getElementById('btnOff').className = state==='off'?'active':'';";
  html += "  });";
  html += "}";
  html += "function startStream(state){";
  html += "  document.getElementById('camImg').src='/stream?led='+state;";
  html += "  document.getElementById('camImg').style.display='block';";
  html += "}";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}

// --- LÓGICA WIFI INTELIGENTE ---
bool tryConnect(const char* ssid, const char* pass, int timeoutSecs = 10) {
  Serial.printf("Conectando a: %s ... ", ssid);
  WiFi.begin(ssid, pass);
  
  for(int i=0; i<timeoutSecs*2; i++) {
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("¡OK!");
      Serial.print("IP: "); Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("Fallo.");
  WiFi.disconnect();
  return false;
}

void setupWiFi() {
  if (tryConnect(DEFAULT_SSID, DEFAULT_PASS)) return;

  Serial.println("Default falló. Leyendo wifi.txt...");
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD no detectada.");
    return;
  }

  File file = SD_MMC.open("/wifi.txt");
  if (!file) {
    Serial.println("wifi.txt no encontrado.");
    SD_MMC.end();
    return;
  }

  String lineSsid, linePass;
  bool isSsidLine = true;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (isSsidLine) {
      lineSsid = line;
      isSsidLine = false;
    } else {
      linePass = line;
      isSsidLine = true;
      
      if (tryConnect(lineSsid.c_str(), linePass.c_str())) {
        file.close();
        SD_MMC.end();
        return;
      }
    }
  }
  
  file.close();
  SD_MMC.end();
  Serial.println("No se pudo conectar a ninguna red.");
}

void setup() {
  Serial.begin(115200);
  
  // CONFIGURACIÓN LED COMO SALIDA DIGITAL SIMPLE
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW); // Asegurar que inicia apagado

  // CONFIGURACIÓN CÁMARA: CANAL 1 (El canal 0 queda libre ahora)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_1; 
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error cámara: 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  setupWiFi();

  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  server.begin();
  
  Serial.println("Servidor listo.");
}

void loop() {
  server.handleClient();
}