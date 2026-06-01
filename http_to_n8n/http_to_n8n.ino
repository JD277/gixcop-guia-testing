#include <WiFi.h>
#include <HTTPClient.h>
#include "unihiker_k10.h"

// 🔐 Credenciales WiFi
const char* ssid = "TP-Link MIGUEL";
const char* password = "jdam1825";

// 🌐 URL de tu API o endpoint
const char* serverUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook/bc98f2bd-4775-4da7-adad-76e8d006d4b8";

UNIHIKER_K10 k10;

void setup() {
  Serial.begin(115200);
  k10.begin();
  k10.initScreen(2);
  k10.creatCanvas();  // Crear canvas
  
  // Conectar a WiFi
  WiFi.begin(ssid, password);
  k10.canvas->canvasText("Conectando a WiFi...", 0, 0xFFFFFF);  // Row 0, color blanco
  k10.canvas->updateCanvas();  // Actualizar pantalla
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi conectado!");
  Serial.print("📍 IP: ");
  Serial.println(WiFi.localIP());
  
  k10.canvas->canvasText(("IP: " + WiFi.localIP().toString()).c_str(), 1, 0xFFFFFF);  // Row 1
  k10.canvas->updateCanvas();
  
  // Hacer request HTTP
  hacerRequestGET();
}

void loop() {
  delay(10000);
}

void hacerRequestGET() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("📦 Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("📄 Response: ");
      Serial.println(response);
      
      // Mostrar en pantalla (rows 2 y 3)
      k10.canvas->canvasText("Respuesta recibida", 2, 0x00FF00);  // Verde
      k10.canvas->updateCanvas();
      
    } else {
      Serial.print("❌ Error en request: ");
      Serial.println(httpResponseCode);
      k10.canvas->canvasText(("Error: " + String(httpResponseCode)).c_str(), 2, 0xFF0000);  // Rojo
      k10.canvas->updateCanvas();
    }
    
    http.end();
  }
}