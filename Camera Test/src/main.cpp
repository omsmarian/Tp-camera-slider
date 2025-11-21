#include <Arduino.h>
#include <BleKeyboard.h>
#include <esp_task_wdt.h>
#include "interface.h"
#include "drivers/ServoDriver.h"
#include "drivers/StepperDriver.h"
#include "drivers/SequenceManager.h"

// ========== Configuración de Pines ==========
const int SERVO_PIN = 19;
const int STEPPER_PUL = 4; 
const int STEPPER_DIR = 13;
const int STEPPER_ENA = 12;

// NUEVO: Pines de Finales de Carrera
const int FC_1 = 23; 
const int FC_2 = 15;

const int BLUE_LED = 33; // LED Azul para estado BLE
const int RED_LED = 35;  // LED Rojo para estado de Error
const int GREEN_LED = 32; // LED Verde para estado de Operación

BleKeyboard bleKeyboard("ESP Camera Slider", "DIY", 100);
ServoDriver* servoDriver = nullptr;
StepperDriver* stepperDriver = nullptr;
SequenceManager* sequenceManager = nullptr;

void takePhoto() {
  if (bleKeyboard.isConnected()) {
    Serial.println("📸 Disparando foto...");
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
  } else {
    Serial.println("⚠️ Bluetooth no conectado");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  
  esp_task_wdt_init(10, false);
  
  Serial.println("🔧 Inicializando drivers...");
  
  servoDriver = new ServoDriver(SERVO_PIN);
  if (!servoDriver->begin()) return;
  servoDriver->setDefaultSpeed(50);
  
  // MODIFICADO: Se pasan los pines de FC y LED verde al constructor
  stepperDriver = new StepperDriver(STEPPER_PUL, STEPPER_DIR, STEPPER_ENA, FC_1, FC_2, GREEN_LED);
  
  if (!stepperDriver->begin(200)) return;
  stepperDriver->setMaxSpeed(2000);
  stepperDriver->setSpeed(1000);
  stepperDriver->enable();
  
  sequenceManager = new SequenceManager(servoDriver, stepperDriver);
  if (!sequenceManager->begin()) return;
  
  bleKeyboard.begin();
  setPhotoCallback(takePhoto);
  setupWebServer();
  
  Serial.println("✅ SISTEMA LISTO (Con Finales de Carrera)");
}

void loop() {
  static bool wasConnected = false;
  bool isConnected = bleKeyboard.isConnected();
  
  // Debug: Mostrar estado de conexión
  if (isConnected != wasConnected) {
    if (isConnected) {
      digitalWrite(BLUE_LED, HIGH);
    } else {
      digitalWrite(BLUE_LED, LOW);
    }
    updateBLEStatus(isConnected);
    wasConnected = isConnected;
  }
  
  // Actualizar LED azul
  digitalWrite(BLUE_LED, isConnected ? HIGH : LOW);
  
  vTaskDelay(pdMS_TO_TICKS(50));
}