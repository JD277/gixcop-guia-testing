#include "unihiker_k10.h"
#include <WiFi.h>
#include <SD.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ====================== CONFIGURACIÓN ====================== //
const char* ssid = "JESUSPROFE 8256";
const char* password = "123456789";
const char* webhookUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook/bc98f2bd-4775-4da7-adad-76e8d006d4b8";

UNIHIKER_K10 k10;
Music music;
WebServer server(80);
bool wifiConnected = false;
bool isProcessing = false; // Bandera para evitar doble pulsación

// ====================== FUNCIONES AUXILIARES ====================== //
void mostrarTexto(const char* texto, int x, int y, uint32_t color) {
  k10.canvas->canvasClear();
  k10.canvas->canvasText(texto, x, y, color, k10.canvas->eCNAndENFont24, 25, true);
  k10.canvas->updateCanvas();
}

void enviarWAVaN8N(const char* filePath) {
  if (!wifiConnected) return;
  
  HTTPClient http;
  File file = SD.open(filePath, FILE_READ);
  
  if (!file) {
    Serial.println("❌ No se pudo abrir el archivo WAV");
    return;
  }

  http.begin(webhookUrl);
  http.addHeader("Content-Type", "audio/wav");
  
  int httpResponseCode = http.sendRequest("POST", &file, file.size());
  
  if (httpResponseCode > 0) {
    Serial.printf("✅ Enviado a n8n. Código: %d\n", httpResponseCode);
  } else {
    Serial.printf("❌ Error HTTP: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  file.close();
  http.end();
}

// ====================== SETUP ====================== //
void setup() {
  Serial.begin(115200);
  
  // 1. Inicialización Hardware
  k10.begin();
  k10.initScreen(2);
  k10.creatCanvas();
  k10.initBgCamerImage();
  k10.setBgCamerImage(false); // Cámara apagada por defecto
  k10.initSDFile();
  k10.setScreenBackground(0x000000);
  
  // 2. Conexión WiFi
  WiFi.begin(ssid, password);
  mostrarTexto("Conectando...", 40, 100, 0xFFFFFF);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("✅ WiFi Conectado: " + WiFi.localIP().toString());
  } else {
    mostrarTexto("Sin WiFi", 80, 100, 0xFF0000);
  }
  
  // 3. Verificación SD
  if (!SD.begin(21)) {
    mostrarTexto("Error SD", 80, 100, 0xFF0000);
    while(1);
  }
  
  // 4. Servidor Web
  
  // Página Principal (Panel de Control)
  server.on("/", HTTP_GET, []() {
    String html = "<html><body style='font-family: sans-serif; text-align: center;'>";
    html += "<h1>🤖 Panel K10</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<hr>";
    html += "<h3>📸 Imagen</h3>";
    html += "<a href='/download/photo.png'><button style='padding: 10px 20px; font-size: 16px;'>Descargar photo.png</button></a>";
    html += "<br><br>";
    html += "<h3>🎙️ Audio</h3>";
    html += "<a href='/download/sound.wav'><button style='padding: 10px 20px; font-size: 16px;'>Descargar sound.wav</button></a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  // Ruta para descargar la IMAGEN
  server.on("/download/photo.png", HTTP_GET, []() {
    File file = SD.open("/photo.png", FILE_READ);
    if (file) {
      server.streamFile(file, "image/png");
      file.close();
    } else {
      server.send(404, "text/plain", "Imagen no encontrada");
    }
  });

  // Ruta para descargar el AUDIO
  server.on("/download/sound.wav", HTTP_GET, []() {
    File file = SD.open("/sound.wav", FILE_READ);
    if (file) {
      server.streamFile(file, "audio/wav");
      file.close();
    } else {
      server.send(404, "text/plain", "Audio no encontrado");
    }
  });

  server.begin();
  
  // 5. Callback Botón
  k10.buttonA->setPressedCallback(onButtonAPressed);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  
}

void loop() {
  server.handleClient();
}

// ====================== LÓGICA DEL BOTÓN ====================== //
void onButtonAPressed() {
  if (isProcessing || !wifiConnected) return;
  isProcessing = true;
  
  k10.rgb->write(-1, 0x00FF00); // LED Verde
  
  // --- PASO 1: FOTO ---
  Serial.println("📸 Tomando foto...");
  mostrarTexto("Foto...", 100, 100, 0xFFFF00);
  k10.setBgCamerImage(true);
  delay(500); // Tiempo para enfocar/capturar
  k10.photoSaveToTFCard("S:/photo.png");
  k10.setBgCamerImage(false); // ⚠️ CRÍTICO: Apagar cámara para liberar I2S
  
  // --- PASO 2: AUDIO ---
  Serial.println("🎙️ Grabando audio...");
  mostrarTexto("Grabando...", 60, 100, 0x00FFFF);
  k10.canvas->updateCanvas();
  
  // Grabar 3 segundos (ajustable)
  music.recordSaveToTFCard("S:/sound.wav", 3);

  // --- PASO 3: ENVÍO A N8N (Opcional, si quieres mantenerlo) ---
  // Si prefieres solo guardar localmente y descargar por web, comenta esta línea:
  enviarWAVaN8N("/sound.wav");
  
  // --- FIN ---
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  k10.rgb->write(-1, 0x000000);
  isProcessing = false;
}