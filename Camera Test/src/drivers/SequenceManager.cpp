#include "drivers/SequenceManager.h"
#include "drivers/ServoDriver.h"
#include "drivers/StepperDriver.h"
#include <esp_task_wdt.h>

SequenceManager::SequenceManager(ServoDriver* servo, StepperDriver* stepper)
  : servoDriver(servo), stepperDriver(stepper),
    activeSequenceIndex(-1), isExecuting(false), isPaused(false) {
  executionTask = nullptr;
  mutex = nullptr;
}

SequenceManager::~SequenceManager() {
  stop();
  if (executionTask != nullptr) {
    vTaskDelete(executionTask);
  }
  if (mutex != nullptr) {
    vSemaphoreDelete(mutex);
  }
}

bool SequenceManager::begin() {
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) {
    Serial.println("❌ SequenceManager: Error creando mutex");
    return false;
  }
  
  Serial.println("✅ SequenceManager inicializado");
  return true;
}

int SequenceManager::createSequence(const String& name) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  
  Sequence newSeq;
  newSeq.name = name;
  newSeq.loop = false;
  newSeq.repeatCount = 1;
  
  sequences.push_back(newSeq);
  int index = sequences.size() - 1;
  
  xSemaphoreGive(mutex);
  
  Serial.printf("✅ Secuencia '%s' creada (index: %d)\n", name.c_str(), index);
  return index;
}

bool SequenceManager::deleteSequence(int index) {
  if (index < 0 || index >= sequences.size()) {
    return false;
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  sequences.erase(sequences.begin() + index);
  xSemaphoreGive(mutex);
  
  return true;
}

bool SequenceManager::addMovement(int sequenceIndex, const Movement& movement) {
  if (sequenceIndex < 0 || sequenceIndex >= sequences.size()) {
    return false;
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  sequences[sequenceIndex].movements.push_back(movement);
  xSemaphoreGive(mutex);
  
  Serial.printf("✅ Movimiento agregado a secuencia %d\n", sequenceIndex);
  return true;
}

bool SequenceManager::removeMovement(int sequenceIndex, int movementIndex) {
  if (sequenceIndex < 0 || sequenceIndex >= sequences.size()) {
    return false;
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  
  if (movementIndex < 0 || movementIndex >= sequences[sequenceIndex].movements.size()) {
    xSemaphoreGive(mutex);
    return false;
  }
  
  sequences[sequenceIndex].movements.erase(
    sequences[sequenceIndex].movements.begin() + movementIndex
  );
  
  xSemaphoreGive(mutex);
  return true;
}

bool SequenceManager::clearSequence(int sequenceIndex) {
  if (sequenceIndex < 0 || sequenceIndex >= sequences.size()) {
    return false;
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  sequences[sequenceIndex].movements.clear();
  xSemaphoreGive(mutex);
  
  return true;
}

void SequenceManager::executionTaskFunc(void* parameter) {
  SequenceManager* manager = static_cast<SequenceManager*>(parameter);
  
  // Suscribir task al watchdog
  esp_task_wdt_add(NULL);
  
  if (manager->activeSequenceIndex < 0 || 
      manager->activeSequenceIndex >= manager->sequences.size()) {
    manager->isExecuting = false;
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
    return;
  }
  
  const Sequence& seq = manager->sequences[manager->activeSequenceIndex];
  
  Serial.printf("▶️ Ejecutando secuencia: %s\n", seq.name.c_str());
  
  for (int repeat = 0; repeat < seq.repeatCount || seq.loop; repeat++) {
    for (size_t i = 0; i < seq.movements.size(); i++) {
      // Resetear watchdog cada movimiento
      esp_task_wdt_reset();
      
      // Verificar pausa
      while (manager->isPaused && manager->isExecuting) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      
      // Verificar si se detuvo
      if (!manager->isExecuting) {
        break;
      }
      
      Serial.printf("📍 Movimiento %d/%d\n", i + 1, seq.movements.size());
      manager->executeMovement(seq.movements[i]);
    }
    
    if (!manager->isExecuting) {
      break;
    }
    
    if (seq.loop) {
      Serial.println("🔄 Repitiendo secuencia (loop)...");
    }
  }
  
  manager->isExecuting = false;
  Serial.println("✅ Secuencia completada");
  
  esp_task_wdt_delete(NULL);
  vTaskDelete(NULL);
}

void SequenceManager::executeMovement(const Movement& movement) {
  if (movement.simultaneous) {
    // Mover ambos motores simultáneamente
    Serial.println("⚙️ Movimiento simultáneo");
    
    // Convertir velocidad de 0-100% a steps/segundo
    int stepperSpeed = map(movement.horizontalSpeed, 0, 100, 100, 2000);
    
    // Convertir mm a steps (asumiendo 8mm por revolución - ajustar según tu setup)
    long steps = stepperDriver->mmToSteps(movement.horizontalDistance, 8.0);
    
    // Iniciar movimientos sin esperar
    stepperDriver->moveRelative(steps, stepperSpeed, false);
    servoDriver->moveTo(movement.angle, movement.angleSpeed, false);
    
    // Esperar a que ambos terminen
    while (stepperDriver->getIsMoving() || servoDriver->getIsMoving()) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  } else {
    // Mover secuencialmente
    Serial.println("⚙️ Movimiento secuencial");
    
    // Primero el stepper
    if (movement.horizontalDistance != 0) {
      int stepperSpeed = map(movement.horizontalSpeed, 0, 100, 100, 2000);
      long steps = stepperDriver->mmToSteps(movement.horizontalDistance, 8.0);
      stepperDriver->moveRelative(steps, stepperSpeed, true);
    }
    
    // Luego el servo
    if (movement.angle >= 0) {
      servoDriver->moveTo(movement.angle, movement.angleSpeed, true);
    }
  }
  
  // Pausa después del movimiento
  if (movement.pauseAfter > 0) {
    Serial.printf("⏸️ Pausa: %dms\n", movement.pauseAfter);
    vTaskDelay(pdMS_TO_TICKS(movement.pauseAfter));
  }
}

bool SequenceManager::executeSequence(int sequenceIndex) {
  if (sequenceIndex < 0 || sequenceIndex >= sequences.size()) {
    Serial.println("❌ Índice de secuencia inválido");
    return false;
  }
  
  if (isExecuting) {
    Serial.println("⚠️ Ya hay una secuencia en ejecución");
    return false;
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  activeSequenceIndex = sequenceIndex;
  isExecuting = true;
  isPaused = false;
  xSemaphoreGive(mutex);
  
  BaseType_t result = xTaskCreatePinnedToCore(
    executionTaskFunc,
    "SequenceTask",
    8192,
    this,
    1,
    &executionTask,
    1
  );
  
  if (result != pdPASS) {
    Serial.println("❌ Error creando task de ejecución");
    isExecuting = false;
    return false;
  }
  
  return true;
}

void SequenceManager::pause() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  isPaused = true;
  xSemaphoreGive(mutex);
  Serial.println("⏸️ Secuencia pausada");
}

void SequenceManager::resume() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  isPaused = false;
  xSemaphoreGive(mutex);
  Serial.println("▶️ Secuencia reanudada");
}

void SequenceManager::stop() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  isExecuting = false;
  isPaused = false;
  xSemaphoreGive(mutex);
  
  // Detener motores
  servoDriver->stop();
  stepperDriver->stop();
  
  Serial.println("⏹️ Secuencia detenida");
}

const Sequence* SequenceManager::getSequence(int index) const {
  if (index < 0 || index >= sequences.size()) {
    return nullptr;
  }
  return &sequences[index];
}

String SequenceManager::getSequenceAsJson(int index) const {
  if (index < 0 || index >= sequences.size()) {
    return "{}";
  }
  
  const Sequence& seq = sequences[index];
  String json = "{";
  json += "\"name\":\"" + seq.name + "\",";
  json += "\"loop\":" + String(seq.loop ? "true" : "false") + ",";
  json += "\"repeatCount\":" + String(seq.repeatCount) + ",";
  json += "\"movements\":[";
  
  for (size_t i = 0; i < seq.movements.size(); i++) {
    if (i > 0) json += ",";
    const Movement& m = seq.movements[i];
    json += "{";
    json += "\"distance\":" + String(m.horizontalDistance, 2) + ",";
    json += "\"speed\":" + String(m.horizontalSpeed) + ",";
    json += "\"angle\":" + String(m.angle) + ",";
    json += "\"angleSpeed\":" + String(m.angleSpeed) + ",";
    json += "\"simultaneous\":" + String(m.simultaneous ? "true" : "false") + ",";
    json += "\"pause\":" + String(m.pauseAfter);
    json += "}";
  }
  
  json += "]}";
  return json;
}

String SequenceManager::getAllSequencesAsJson() const {
  String json = "[";
  
  for (size_t i = 0; i < sequences.size(); i++) {
    if (i > 0) json += ",";
    json += getSequenceAsJson(i);
  }
  
  json += "]";
  return json;
}
