#include "interface.h"
#include "drivers/ServoDriver.h"
#include "drivers/StepperDriver.h"
#include "drivers/SequenceManager.h"
#include "drivers/EndstopDriver.h"
#include <LittleFS.h>

AsyncWebServer server(80);

// Configuración Access Point
const char* ap_ssid = "ESP32-CameraSlider";
const char* ap_password = "slider123";  // Mínimo 8 caracteres

// Referencias externas a drivers (definidos en main.cpp)
extern ServoDriver* servoDriver;
extern StepperDriver* stepperDriver;
extern SequenceManager* sequenceManager;
extern class EndstopDriver* endstopDriver;

// Callback para disparar foto
void (*photoCallbackFunc)() = nullptr;
bool bleConnected = false;

void setPhotoCallback(void (*callback)()) {
  photoCallbackFunc = callback;
}

void updateBLEStatus(bool connected) {
  bleConnected = connected;
}

void setupWebServer() {
  // Inicializar LittleFS (no SPIFFS)
  if(!LittleFS.begin(true)){
    Serial.println("❌ Error montando LittleFS");
    return;
  }
  Serial.println("✅ LittleFS montado correctamente");
  
  // Listar archivos para verificar
  Serial.println("\n📁 Archivos en LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("  - ");
    Serial.print(file.name());
    Serial.print(" (");
    Serial.print(file.size());
    Serial.println(" bytes)");
    file = root.openNextFile();
  }

  // Configurar WiFi como Access Point
  Serial.println("\n📡 Configurando Access Point...");
  WiFi.mode(WIFI_AP);
  
  bool apStarted = WiFi.softAP(ap_ssid, ap_password);
  
  if(apStarted) {
    Serial.println("✅ Access Point iniciado!");
    Serial.print("📶 SSID: ");
    Serial.println(ap_ssid);
    Serial.print("🔑 Password: ");
    Serial.println(ap_password);
    Serial.print("🌐 IP del servidor: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("\n👉 Conéctate a la red WiFi y accede a la IP mostrada arriba");
  } else {
    Serial.println("\n❌ Error iniciando Access Point");
    return;
  }

  // Ruta principal - servir index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 GET /");
    if(LittleFS.exists("/index.html")){
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html no encontrado en LittleFS");
    }
  });

  // Servir CSS
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 GET /style.css");
    if(LittleFS.exists("/style.css")){
      request->send(LittleFS, "/style.css", "text/css");
    } else {
      request->send(404, "text/plain", "style.css no encontrado");
    }
  });

  // Servir JS
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 GET /script.js");
    if(LittleFS.exists("/script.js")){
      request->send(LittleFS, "/script.js", "application/javascript");
    } else {
      request->send(404, "text/plain", "script.js no encontrado");
    }
  });

  // Ruta para disparar foto
  server.on("/photo", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📸 GET /photo");
    if (bleConnected && photoCallbackFunc != nullptr) {
      photoCallbackFunc();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(200, "application/json", 
        "{\"success\":false,\"message\":\"Bluetooth no conectado\"}");
    }
  });

  // Ruta para estado BLE y endstops
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"connected\":";
    json += bleConnected ? "true" : "false";
    
    // Agregar estado de endstops si el driver existe
    if(endstopDriver) {
      json += ",\"endstop_min\":";
      json += endstopDriver->isMinTriggered() ? "true" : "false";
      json += ",\"endstop_max\":";
      json += endstopDriver->isMaxTriggered() ? "true" : "false";
    }
    
    json += "}";
    request->send(200, "application/json", json);
  });

  // ========== Control Manual ==========
  
  // Controlar servo
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!servoDriver) {
      request->send(500, "application/json", "{\"success\":false,\"message\":\"Driver no inicializado\"}");
      return;
    }
    if(request->hasParam("angle") && request->hasParam("speed")) {
      int angle = request->getParam("angle")->value().toInt();
      int speed = request->getParam("speed")->value().toInt();
      
      if(servoDriver->moveTo(angle, speed, false)) {
        request->send(200, "application/json", "{\"success\":true,\"angle\":" + String(angle) + ",\"speed\":" + String(speed) + "}");
      } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Error moviendo servo\"}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Faltan parámetros\"}");
    }
  });

  // Controlar stepper
  server.on("/stepper", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!stepperDriver) {
      request->send(500, "application/json", "{\"success\":false,\"message\":\"Driver no inicializado\"}");
      return;
    }
    if(request->hasParam("distance") && request->hasParam("speed")) {
      float distance = request->getParam("distance")->value().toFloat();
      int speed = request->getParam("speed")->value().toInt();
      
      // Convertir velocidad de 0-100% a steps/segundo
      int stepsPerSec = map(speed, 0, 100, 100, 2000);
      long steps = stepperDriver->mmToSteps(distance, 8.0); // 8mm por revolución
      
      if(stepperDriver->moveRelative(steps, stepsPerSec, false)) {
        request->send(200, "application/json", "{\"success\":true,\"distance\":" + String(distance) + ",\"speed\":" + String(speed) + "}");
      } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Error moviendo stepper\"}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Faltan parámetros\"}");
    }
  });

  // Habilitar/deshabilitar stepper
  server.on("/stepper/enable", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!stepperDriver) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    if(request->hasParam("value")) {
      bool enable = request->getParam("value")->value() == "true";
      if(enable) stepperDriver->enable();
      else stepperDriver->disable();
      request->send(200, "application/json", "{\"success\":true,\"enabled\":" + String(enable ? "true" : "false") + "}");
    } else {
      request->send(400, "application/json", "{\"success\":false}");
    }
  });

  // Resetear posición stepper
  server.on("/stepper/zero", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!stepperDriver) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    stepperDriver->zero();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ========== Gestión de Secuencias ==========
  
  // Crear secuencia
  server.on("/sequence/create", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    if(request->hasParam("name", true)) {
      String name = request->getParam("name", true)->value();
      int index = sequenceManager->createSequence(name);
      request->send(200, "application/json", "{\"success\":true,\"index\":" + String(index) + "}");
    } else {
      request->send(400, "application/json", "{\"success\":false}");
    }
  });

  // Agregar movimiento a secuencia
  server.on("/sequence/add", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    
    if(request->hasParam("seq", true) && request->hasParam("distance", true) && 
       request->hasParam("speed", true) && request->hasParam("angle", true) &&
       request->hasParam("angleSpeed", true)) {
      
      int seqIndex = request->getParam("seq", true)->value().toInt();
      
      Movement mov;
      mov.horizontalDistance = request->getParam("distance", true)->value().toFloat();
      mov.horizontalSpeed = request->getParam("speed", true)->value().toInt();
      mov.angle = request->getParam("angle", true)->value().toInt();
      mov.angleSpeed = request->getParam("angleSpeed", true)->value().toInt();
      mov.simultaneous = request->hasParam("simultaneous", true) ? 
                         request->getParam("simultaneous", true)->value() == "true" : false;
      mov.pauseAfter = request->hasParam("pause", true) ? 
                       request->getParam("pause", true)->value().toInt() : 0;
      
      if(sequenceManager->addMovement(seqIndex, mov)) {
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(500, "application/json", "{\"success\":false}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Faltan parámetros\"}");
    }
  });

  // Ejecutar secuencia
  server.on("/sequence/execute", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    if(request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      if(sequenceManager->executeSequence(index)) {
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(500, "application/json", "{\"success\":false}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false}");
    }
  });

  // Pausar/reanudar secuencia
  server.on("/sequence/pause", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    sequenceManager->pause();
    request->send(200, "application/json", "{\"success\":true}");
  });

  server.on("/sequence/resume", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    sequenceManager->resume();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // Detener secuencia
  server.on("/sequence/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{\"success\":false}");
      return;
    }
    sequenceManager->stop();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // Listar secuencias
  server.on("/sequence/list", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "[]");
      return;
    }
    String json = sequenceManager->getAllSequencesAsJson();
    request->send(200, "application/json", json);
  });

  // Obtener info de una secuencia
  server.on("/sequence/get", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!sequenceManager) {
      request->send(500, "application/json", "{}");
      return;
    }
    if(request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      String json = sequenceManager->getSequenceAsJson(index);
      request->send(200, "application/json", json);
    } else {
      request->send(400, "application/json", "{}");
    }
  });

  // Capturar 404
  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.print("❌ 404: ");
    Serial.println(request->url());
    request->send(404, "text/plain", "Archivo no encontrado: " + request->url());
  });

  server.begin();
  Serial.println("✅ Servidor web iniciado\n");
}


