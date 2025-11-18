#include "drivers/EndstopDriver.h"
#include <esp_task_wdt.h>

EndstopDriver::EndstopDriver(int min, int max)
  : pinMin(min), pinMax(max),
    minTriggered(false), maxTriggered(false),
    onMinTriggerCallback(nullptr), onMaxTriggerCallback(nullptr),
    taskHandle(nullptr), mutex(nullptr),
    lastMinTrigger(0), lastMaxTrigger(0) {
}

EndstopDriver::~EndstopDriver() {
  if (taskHandle != nullptr) {
    vTaskDelete(taskHandle);
  }
  if (mutex != nullptr) {
    vSemaphoreDelete(mutex);
  }
}

bool EndstopDriver::begin() {
  // Configurar pines como INPUT_PULLUP
  // Los endstops NC (Normally Closed) están en LOW normalmente y van a HIGH cuando se activan
  pinMode(pinMin, INPUT_PULLUP);
  pinMode(pinMax, INPUT_PULLUP);
  
  // Crear mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) {
    Serial.println("❌ EndstopDriver: Error creando mutex");
    return false;
  }
  
  // Crear task con alta prioridad (mayor que stepper)
  BaseType_t result = xTaskCreatePinnedToCore(
    endstopTask,
    "EndstopTask",
    4096,      // Stack de 4KB
    this,
    4,         // Prioridad 4 (mayor que stepper=1 y servo=2)
    &taskHandle,
    1          // Core 1
  );
  
  if (result != pdPASS) {
    Serial.println("❌ EndstopDriver: Error creando task");
    return false;
  }
  
  Serial.printf("✅ EndstopDriver inicializado (MIN:%d MAX:%d) - Prioridad 4\n", 
                pinMin, pinMax);
  return true;
}

void EndstopDriver::endstopTask(void* parameter) {
  EndstopDriver* driver = static_cast<EndstopDriver*>(parameter);
  
  // Suscribir task al watchdog
  esp_task_wdt_add(NULL);
  
  while (true) {
    // Resetear watchdog
    esp_task_wdt_reset();
    
    // Verificar endstops
    driver->checkEndstops();
    
    // Verificar cada 10ms (100Hz es suficiente para endstops)
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void EndstopDriver::checkEndstops() {
  unsigned long currentTime = millis();
  
  // Leer estado de los pines (HIGH = activado con NC - Normally Closed)
  // Con NC: Circuito cerrado = LOW normal, Circuito abierto (activado) = HIGH
  bool minPressed = (digitalRead(pinMin) == HIGH);
  bool maxPressed = (digitalRead(pinMax) == HIGH);
  
  // Verificar endstop mínimo con debounce
  if (minPressed && !minTriggered) {
    if (currentTime - lastMinTrigger > debounceDelay) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      minTriggered = true;
      xSemaphoreGive(mutex);
      
      lastMinTrigger = currentTime;
      Serial.println("🛑 ENDSTOP MIN activado!");
      
      // Ejecutar callback si existe
      if (onMinTriggerCallback != nullptr) {
        onMinTriggerCallback();
      }
    }
  } else if (!minPressed && minTriggered) {
    // Resetear cuando se suelta
    if (currentTime - lastMinTrigger > debounceDelay) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      minTriggered = false;
      xSemaphoreGive(mutex);
      Serial.println("✅ ENDSTOP MIN liberado");
    }
  }
  
  // Verificar endstop máximo con debounce
  if (maxPressed && !maxTriggered) {
    if (currentTime - lastMaxTrigger > debounceDelay) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      maxTriggered = true;
      xSemaphoreGive(mutex);
      
      lastMaxTrigger = currentTime;
      Serial.println("🛑 ENDSTOP MAX activado!");
      
      // Ejecutar callback si existe
      if (onMaxTriggerCallback != nullptr) {
        onMaxTriggerCallback();
      }
    }
  } else if (!maxPressed && maxTriggered) {
    // Resetear cuando se suelta
    if (currentTime - lastMaxTrigger > debounceDelay) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      maxTriggered = false;
      xSemaphoreGive(mutex);
      Serial.println("✅ ENDSTOP MAX liberado");
    }
  }
}

void EndstopDriver::setMinTriggerCallback(void (*callback)()) {
  onMinTriggerCallback = callback;
}

void EndstopDriver::setMaxTriggerCallback(void (*callback)()) {
  onMaxTriggerCallback = callback;
}

void EndstopDriver::reset() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  minTriggered = false;
  maxTriggered = false;
  xSemaphoreGive(mutex);
  Serial.println("🔄 Endstops reseteados");
}
