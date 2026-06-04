#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SD.h>
#include "unihiker_k10.h"

// ==========================================
// CONFIGURACIÓN DE HARDWARE Y PINES
// ==========================================
#define PIN_TRIG   22  // Ajusta según el pin físico que uses para el Trigger
#define PIN_ECHO   23  // Ajusta según el pin físico que uses para el Echo
#define SD_CS_PIN  21  // Pin CS de la SD en la K10

// Umbral de distancia (en cm) para detectar la base y cambiar a Modo Guardia
const int UMBRAL_BASE_GUARDIA = 15; 

// ==========================================
// CREDENCIALES Y ENDPOINTS
// ==========================================
const char* ssid = "TP-Link MIGUEL";
const char* password = "jdam1825";

// URLs de n8n
const char* n8nWebhookUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook-test/099aafb3-27de-473e-a1a7-934d77943d3f";

// ==========================================
// INSTANCIAS Y VARIABLES GLOBALES
// ==========================================
UNIHIKER_K10 k10;
WebServer server(80);
uint8_t screen_dir = 2;

// Definición de los Modos del Robot
enum ModoRobot { MODO_EXPOSICION, MODO_GUARDIA };
ModoRobot modoActual = MODO_EXPOSICION;

// Prototipos de funciones
void onButtonAPressed();
void onButtonBPressed();
void hacerPeticionN8N();
long readUltrasonicDistance();
void ejecutarLogicaGuardia();
void moverRobotGuardia();
void detenerRobot();

// ==========================================
// SETUP PRINCIPAL
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. Inicialización básica de la K10
  k10.begin();
  k10.initScreen(screen_dir);
  k10.creatCanvas();
  k10.initSDFile();
  
  // 2. CONFIGURACIÓN DEL SENSOR ULTRASONIDO
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // 3. SECUENCIA DE PANTALLA DE INICIO (Logo del Club y Proyecto)
  k10.setScreenBackground(0x000000); // CORREGIDO: k10 directo para el fondo
  k10.canvas->canvasDrawImage(60, 60, "S:/Clubw.png");
  k10.canvas->updateCanvas();
  delay(2000);
  
  k10.canvas->canvasClear();
  k10.canvas->canvasDrawImage(60, 60, "S:/Club.png");
  k10.canvas->updateCanvas();
  delay(2000);
  
  k10.canvas->canvasClear();
  k10.setScreenBackground(0x000000); // CORREGIDO: k10 directo para el fondo
  k10.canvas->canvasText("GU-IA", 1, 0xFFFFFF); // Nombre del proyecto en Blanco
  k10.canvas->updateCanvas();
  delay(2000);
  k10.canvas->canvasClear();

  // 4. CONEXIÓN A LA RED WI-FI
  WiFi.begin(ssid, password);
  k10.canvas->canvasText("Conectando a WiFi...", 0, 0xFFFFFF);
  k10.canvas->updateCanvas();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado!");
  
  // Mostrar IP obtenida en pantalla
  k10.canvas->canvasClear();
  k10.canvas->canvasText("WiFi Conectado", 0, 0x00FF00);
  k10.canvas->canvasText(("IP: " + WiFi.localIP().toString()).c_str(), 1, 0xFFFFFF);
  k10.canvas->updateCanvas();
  delay(2000);

  // 5. INICIALIZACIÓN DE LA TARJETA SD Y CÁMARA DE LA K10
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ Error crítico en la tarjeta SD");
    k10.canvas->canvasText("Error SD", 2, 0xFF0000);
    k10.canvas->updateCanvas();
  }
  
  k10.initBgCamerImage();
  k10.setBgCamerImage(false); // Apagada por defecto

  // 6. CONFIGURACIÓN DEL SERVIDOR WEB (Servir el audio sound.wav)
  server.on("/download/sound.wav", HTTP_GET, []() {
    File file = SD.open("/sound.wav", FILE_READ);
    if (!file) {
      server.send(404, "text/plain", "Archivo sound.wav no encontrado en SD");
      return;
    }
    Serial.print("📤 Enviando audio. Tamaño: ");
    Serial.println(file.size());
    server.streamFile(file, "audio/wav");
    file.close();
  });
  
  server.on("/", HTTP_GET, []() {
    String html = "<h1>GU-IA Central Server</h1>";
    html += "<a href='/download/sound.wav'>Descargar peticion de audio (sound.wav)</a>";
    server.send(200, "text/html", html);
  });
  
  server.begin();
  Serial.println("🌐 Servidor Web de Audio Activo.");

  // 7. ASIGNACIÓN DE BOTONES (Callbacks)
  k10.buttonA->setPressedCallback(onButtonAPressed);
  k10.buttonB->setPressedCallback(onButtonBPressed);

  // Pantalla lista para interactuar en Modo Exposición
  k10.canvas->canvasClear();
  k10.canvas->canvasText("Modo: EXPOSICION", 0, 0x00FFFF);
  k10.canvas->canvasText("A: Grabar/Foto | B: Ver Foto", 1, 0xFFFFFF);
  k10.canvas->updateCanvas();
}

// ==========================================
// BUCLE PRINCIPAL (LOOP)
// ==========================================
void loop() {
  // Mantener el servidor web escuchando peticiones siempre
  server.handleClient();

  // Leer constantemente la distancia del ultrasonido para evaluar cambio de modo
  long distancia = readUltrasonicDistance();

  // Lógica de Conmutación de Modos por Ultrasonido
  if (distancia > 0 && distancia <= UMBRAL_BASE_GUARDIA) {
    if (modoActual != MODO_GUARDIA) {
      modoActual = MODO_GUARDIA;
      detenerRobot(); // Seguridad al cambiar de estado
      k10.setBgCamerImage(false); // Apagar cámara de la K10 en modo guardia
      k10.canvas->canvasClear();
      k10.canvas->canvasText("Modo: GUARDIA (Base)", 0, 0xFF0000); // Rojo
      k10.canvas->canvasText("Monitoreando entorno...", 1, 0xFFFFFF);
      k10.canvas->updateCanvas();
      Serial.println("🚨 Modo Guardia Activado. Robot en la base.");
    }
    // Ejecuta las rutinas autónomas de movimiento/vigilancia
    ejecutarLogicaGuardia();
  } 
  else {
    if (modoActual != MODO_EXPOSICION) {
      modoActual = MODO_EXPOSICION;
      detenerRobot(); // Detener cualquier movimiento autónomo previo
      k10.canvas->canvasClear();
      k10.canvas->canvasText("Modo: EXPOSICION", 0, 0x00FFFF); // Cian
      k10.canvas->canvasText("A: Grabar/Foto | B: Ver Foto", 1, 0xFFFFFF);
      k10.canvas->updateCanvas();
      Serial.println("🏛️ Modo Exposición Activado. Listo para visitantes.");
    }
  }

  delay(50); // Estabilidad del ciclo
}

// ==========================================
// MANEJO DE BOTONES E INTERACCIONES (MODO EXPO)
// ==========================================

// BOTÓN A: Captura de Datos (Foto, grabación y disparo a n8n)
void onButtonAPressed() {
  if (modoActual != MODO_EXPOSICION) return; 

  k10.canvas->canvasClear();
  k10.canvas->canvasText("Capturando obra...", 0, 0xFFFF00);
  k10.canvas->updateCanvas();

  // 1. Activar cámara brevemente y guardar foto en SD
  k10.setBgCamerImage(true);
  delay(500); 
  k10.photoSaveToTFCard("S:/photo.bmp");
  k10.setBgCamerImage(false); 

  // 2. Grabación de Audio
  k10.canvas->canvasClear();
  k10.canvas->canvasText("🎙️ Grabando audio...", 0, 0x00FF00);
  k10.canvas->canvasText("Hable ahora...", 1, 0xFFFFFF);
  k10.canvas->updateCanvas();
  
  // NOTA: Recuerda descomentar/ajustar la función nativa de grabación de la K10 si es necesario
  // k10.startRecord("S:/sound.wav"); 
  delay(4000); 
  // k10.stopRecord();

  k10.canvas->canvasClear();
  k10.canvas->canvasText("Procesando en n8n...", 0, 0x00FFFF);
  k10.canvas->updateCanvas();

  // 3. Notificar a n8n mediante HTTP para que recoja el audio y analice la obra
  hacerPeticionN8N();
}

// BOTÓN B: Visualizar la última foto guardada localmente
void onButtonBPressed() {
  if (modoActual != MODO_EXPOSICION) return;

  k10.canvas->canvasDrawImage(0, 0, "S:/photo.bmp");
  delay(3000);
  
  // Restaurar el menú de exposición
  k10.canvas->canvasClear();
  k10.canvas->canvasText("Modo: EXPOSICION", 0, 0x00FFFF);
  k10.canvas->canvasText("A: Grabar/Foto | B: Ver Foto", 1, 0xFFFFFF);
  k10.canvas->updateCanvas();
}

// ==========================================
// COMUNICACIÓN CON EL SERVIDOR N8N
// ==========================================
void hacerPeticionN8N() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(n8nWebhookUrl);
    http.addHeader("Content-Type", "application/json");

    String ipLocal = WiFi.localIP().toString();
    String jsonPayload = "{\"dispositivo\":\"UNIHIKER_K10\",\"audio_url\":\"http://" + ipLocal + "/download/sound.wav\",\"status\":\"ready\"}";

    int httpResponseCode = http.POST(jsonPayload);

    k10.canvas->canvasClear();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("📦 n8n Response: " + response);
      
      k10.canvas->canvasText("Respuesta Recibida:", 0, 0x00FF00);
      k10.canvas->canvasText("GU-IA procesado OK", 1, 0xFFFFFF); 
    } else {
      Serial.println("❌ Error en HTTP POST: " + String(httpResponseCode));
      k10.canvas->canvasText("Error de Conexión", 0, 0xFF0000);
    }
    k10.canvas->updateCanvas();
    http.end();
    
    delay(4000); 
    
    k10.canvas->canvasClear();
    k10.canvas->canvasText("Modo: EXPOSICION", 0, 0x00FFFF);
    k10.canvas->canvasText("A: Grabar/Foto | B: Ver Foto", 1, 0xFFFFFF);
    k10.canvas->updateCanvas();
  }
}

// ==========================================
// LÓGICA DEL SENSOR ULTRASONIDO Y MOVIMIENTO
// ==========================================
long readUltrasonicDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000); 
  if (duration == 0) return -1; 
  
  long distanceCm = duration * 0.034 / 2;
  return distanceCm;
}

void ejecutarLogicaGuardia() {
  long distanciaObstaculo = readUltrasonicDistance();
  
  if (distanciaObstaculo > 0 && distanciaObstaculo < 20) {
    Serial.println("⚠️ ¡Obstáculo detectado en modo Guardia! Evadiendo...");
    detenerRobot();
    delay(500);
    // Espacio para la lógica de giro de tus motores
    delay(1000);
  } else {
    moverRobotGuardia();
  }
}

// ==========================================
// CONTROL DE MOTORES
// ==========================================
void moverRobotGuardia() {
  // Añade aquí las llamadas a tus librerías de motores (ej: MegaPi o drivers dedicados)
}

void detenerRobot() {
  // Añade aquí los comandos de parada de motores
}