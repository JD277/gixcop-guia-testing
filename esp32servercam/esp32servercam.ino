// Versión sin streaming, solo foto bajo demanda
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
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

#define LED_GPIO_NUM 4
// Pines y configuración (igual que arriba)...

// Cabecera especial para indicarle al navegador que recibirá un flujo constante de imágenes
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

WebServer server(80);
bool ledState = false;
const char *ssid = "TP-Link MIGUEL";
const char *password = "jdam1825";


void handleCapture() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        server.send(500, "text/plain", "Error al capturar");
        return;
    }
    
    // Enviamos la imagen JPEG pura en formato binario
    server.setContentLength(fb->len);
    server.send(200, "image/jpeg", "");
    server.sendContent((const char *)fb->buf, fb->len);
    
    esp_camera_fb_return(fb);
}

void handleStream() {
    WiFiClient client = server.client();

    // Enviamos las cabeceras para inicializar el streaming de video
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: ");
    client.print(_STREAM_CONTENT_TYPE);
    client.print("\r\n\r\n");

    // Bucle infinito de envío de frames mientras el usuario no cierre la página
    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            delay(10);
            continue;
        }

        // Enviar delimitador de inicio de frame
        client.print(_STREAM_BOUNDARY);
        
        // Enviar tamaño del frame actual
        char hdr[64];
        sprintf(hdr, _STREAM_PART, fb->len);
        client.print(hdr);
        
        // Enviar los bytes de la imagen
        client.write(fb->buf, fb->len);
        
        esp_camera_fb_return(fb);
        
        // Un pequeño delay para no saturar el procesador
        delay(1); 
    }
}

// --- RUTA 3: INTERFAZ GRÁFICA (Visualizador en el navegador) ---
void handleRoot() {
    // Apuntamos la etiqueta <img> directamente a la ruta de transmisión continua /stream
    String html = "<html><head><title>ESP32-CAM Stream</title>";
    html += "<style>body{text-align:center; background:#1e1e1e; color:white; font-family:sans-serif;} img{max-width:100%; border:3px solid #444; border-radius:8px;}</style></head>";
    html += "<body>";
    html += "<h1>ESP32-CAM: Video en Tiempo Real</h1>";
    html += "<img src='/stream' />"; // <-- El truco mágico está aquí
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_GPIO_NUM, OUTPUT);
    digitalWrite(LED_GPIO_NUM, LOW);
    
    // --- COLOCA AQUÍ TU CONFIGURACIÓN ESPECÍFICA DE LA CÁMARA (Pins de la Ai-Thinker, etc.) ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
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
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 15;
  config.fb_count = 2;
    // Inicializar la cámara físicamente
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("¡Fallo crítico al inicializar la cámara! Error: 0x%x", err);
        return; // Si falla aquí, no dejes que continúe de forma errónea
    }
    // Configurar sensores internos para mayor velocidad
    sensor_t * s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_UXGA); // Forzar la resolución en el sensor externo

    s->set_vflip(s, 1);       // 1 = Activa el volteo vertical (de cabeza). Usa 0 para desactivar.
    s->set_hmirror(s, 1);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado a Wi-Fi!");
    Serial.print("IP de la ESP32-CAM: ");
    Serial.println(WiFi.localIP()); // Copia esta IP para tu script de Python
    
    // Solo dejamos la ruta que le interesa a la IA
    server.on("/", handleRoot);
    server.on("/capture", handleCapture);
    server.on("/stream", handleStream);
    server.begin();
}

void loop() {
    server.handleClient();
}