#include <Arduino.h>
#include <BleKeyboard.h>
#include <esp_task_wdt.h>
#include "interface.h"
#include "drivers/ServoDriver.h"
#include "drivers/StepperDriver.h"
#include "drivers/SequenceManager.h"
#include "drivers/EndstopDriver.h"

// ========== Configuración de Pines ==========
// Servo
const int SERVO_PIN = 13;

// Stepper (TB6600)
const int STEPPER_PUL = 14;  // Pin de pulso (STEP)
const int STEPPER_DIR = 27;  // Pin de dirección
const int STEPPER_ENA = 26;  // Pin de enable

// Endstops (Fines de carrera)
const int ENDSTOP_MIN = 25;  // Pin endstop mínimo
const int ENDSTOP_MAX = 33;  // Pin endstop máximo

// ========== Drivers ==========
BleKeyboard bleKeyboard("ESP Camera Slider", "DIY", 100);
ServoDriver* servoDriver = nullptr;
StepperDriver* stepperDriver = nullptr;
SequenceManager* sequenceManager = nullptr;
EndstopDriver* endstopDriver = nullptr;

// Función para disparar foto
void takePhoto() {
  if (bleKeyboard.isConnected()) {
    Serial.println("📸 Disparando foto...");
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
  } else {
    Serial.println("⚠️ Bluetooth no conectado");
  }
}

// Callback para cuando se activa un endstop
void onEndstopTriggered() {
  if (stepperDriver != nullptr) {
    stepperDriver->stop();
    Serial.println("🛑 Motor detenido por endstop");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Configurar watchdog con timeout mayor
  Serial.println("⚙️ Configurando watchdog...");
  esp_task_wdt_init(10, false); // 10 segundos, no panic automático
  Serial.println("✅ Watchdog configurado\n");
  
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  ESP32 Camera Slider Control System   ║");
  Serial.println("║         FreeRTOS Architecture          ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  
  // ========== Inicializar Drivers ==========
  Serial.println("🔧 Inicializando drivers...");
  
  // Servo Driver
  Serial.println("  → ServoDriver...");
  servoDriver = new ServoDriver(SERVO_PIN);
  if (!servoDriver->begin()) {
    Serial.println("❌ Error inicializando ServoDriver");
    return;
  }
  servoDriver->setDefaultSpeed(50);
  
  // Stepper Driver
  Serial.println("  → StepperDriver...");
  stepperDriver = new StepperDriver(STEPPER_PUL, STEPPER_DIR, STEPPER_ENA);
  if (!stepperDriver->begin(200)) {  // 200 steps por revolución
    Serial.println("❌ Error inicializando StepperDriver");
    return;
  }
  stepperDriver->setMaxSpeed(2000);
  stepperDriver->setSpeed(1000);
  stepperDriver->enable();
  
  // Endstop Driver
  Serial.println("  → EndstopDriver...");
  endstopDriver = new EndstopDriver(ENDSTOP_MIN, ENDSTOP_MAX);
  if (!endstopDriver->begin()) {
    Serial.println("❌ Error inicializando EndstopDriver");
    return;
  }
  // Configurar callbacks para detener motor
  endstopDriver->setMinTriggerCallback(onEndstopTriggered);
  endstopDriver->setMaxTriggerCallback(onEndstopTriggered);
  
  // Asignar endstop driver al stepper driver
  stepperDriver->setEndstopDriver(endstopDriver);
  
  // Sequence Manager
  Serial.println("  → SequenceManager...");
  sequenceManager = new SequenceManager(servoDriver, stepperDriver);
  if (!sequenceManager->begin()) {
    Serial.println("❌ Error inicializando SequenceManager");
    return;
  }
  
  Serial.println("✅ Todos los drivers inicializados\n");
  
  // ========== Iniciar Bluetooth ==========
  Serial.println("📡 Iniciando Bluetooth...");
  bleKeyboard.begin();
  delay(1000);
  Serial.println("✅ Bluetooth iniciado\n");
  
  // ========== Configurar Web Interface ==========
  Serial.println("🌐 Configurando servidor web...");
  setPhotoCallback(takePhoto);
  setupWebServer();
  
  // ========== Sistema Listo ==========
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           ✅ SISTEMA LISTO             ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("📱 Conecta 'ESP Camera Slider' desde Bluetooth");
  Serial.println("🌐 Accede a la interfaz web desde la IP mostrada arriba");
  Serial.println();
  Serial.println("📌 Configuración de pines:");
  Serial.printf("   Servo:    GPIO %d\n", SERVO_PIN);
  Serial.printf("   Stepper:  PUL=%d DIR=%d ENA=%d\n", STEPPER_PUL, STEPPER_DIR, STEPPER_ENA);
  Serial.printf("   Endstops: MIN=%d MAX=%d\n", ENDSTOP_MIN, ENDSTOP_MAX);
  Serial.println();
  Serial.println("🎯 Tasks FreeRTOS creadas:");
  Serial.println("   - EndstopTask (Core 1, Prioridad 4) ⭐ Máxima prioridad");
  Serial.println("   - ServoTask (Core 1, Prioridad 2)");
  Serial.println("   - StepperTask (Core 0, Prioridad 1)");
  Serial.println();
}

void loop() {
  // Verificar estado de conexión BLE
  static bool wasConnected = false;
  bool isConnected = bleKeyboard.isConnected();
  
  if (isConnected != wasConnected) {
    updateBLEStatus(isConnected);
    if (isConnected) {
      Serial.println("🟢 Bluetooth conectado");
    } else {
      Serial.println("🔴 Bluetooth desconectado");
    }
    wasConnected = isConnected;
  }

  // Dar tiempo al scheduler y watchdog
  vTaskDelay(pdMS_TO_TICKS(50));
}
