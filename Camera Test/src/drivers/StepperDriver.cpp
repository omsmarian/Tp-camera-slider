#include "drivers/StepperDriver.h"
#include "drivers/EndstopDriver.h"
#include <esp_task_wdt.h>

StepperDriver::StepperDriver(int pul, int dir, int ena)
  : pinPUL(pul), pinDIR(dir), pinENA(ena),
    currentPosition(0), targetPosition(0), currentSpeed(1000),
    isMoving(false), isEnabled(false),
    stepsPerRevolution(200), maxSpeed(2000), acceleration(500),
    endstopDriver(nullptr) {
  commandQueue = nullptr;
  taskHandle = nullptr;
  mutex = nullptr;
}

StepperDriver::~StepperDriver() {
  disable();
  if (taskHandle != nullptr) {
    vTaskDelete(taskHandle);
  }
  if (commandQueue != nullptr) {
    vQueueDelete(commandQueue);
  }
  if (mutex != nullptr) {
    vSemaphoreDelete(mutex);
  }
}

bool StepperDriver::begin(int stepsPerRev) {
  stepsPerRevolution = stepsPerRev;
  
  // Configurar pines
  pinMode(pinPUL, OUTPUT);
  pinMode(pinDIR, OUTPUT);
  if (pinENA >= 0) {
    pinMode(pinENA, OUTPUT);
    digitalWrite(pinENA, HIGH); // Deshabilitado por defecto (activo bajo)
  }
  
  digitalWrite(pinPUL, LOW);
  digitalWrite(pinDIR, LOW);
  
  // Crear mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) {
    Serial.println("❌ StepperDriver: Error creando mutex");
    return false;
  }
  
  // Crear cola de comandos
  commandQueue = xQueueCreate(10, sizeof(StepperCommand));
  if (commandQueue == nullptr) {
    Serial.println("❌ StepperDriver: Error creando queue");
    return false;
  }
  
  // Crear task
  BaseType_t result = xTaskCreatePinnedToCore(
    stepperTask,
    "StepperTask",
    8192,  // Aumentar stack a 8KB
    this,
    1,  // Reducir prioridad para no bloquear sistema
    &taskHandle,
    0  // Core 0
  );
  
  if (result != pdPASS) {
    Serial.println("❌ StepperDriver: Error creando task");
    return false;
  }
  
  Serial.printf("✅ StepperDriver inicializado (PUL:%d DIR:%d ENA:%d)\n", 
                pinPUL, pinDIR, pinENA);
  return true;
}

void StepperDriver::setEndstopDriver(EndstopDriver* driver) {
  endstopDriver = driver;
  Serial.println("✅ EndstopDriver asignado a StepperDriver");
}

void StepperDriver::stepperTask(void* parameter) {
  StepperDriver* driver = static_cast<StepperDriver*>(parameter);
  StepperCommand cmd;
  
  // Suscribir task al watchdog
  esp_task_wdt_add(NULL);
  
  while (true) {
    // Resetear watchdog al inicio de cada iteración
    esp_task_wdt_reset();
    
    if (xQueueReceive(driver->commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      driver->processCommand(cmd);
    }
  }
}

void StepperDriver::processCommand(StepperCommand cmd) {
  if (!isEnabled) {
    Serial.println("⚠️ StepperDriver: Motor deshabilitado");
    return;
  }
  
  // Resetear watchdog antes de operación larga
  esp_task_wdt_reset();
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = true;
  
  // Calcular posición objetivo
  if (cmd.relative) {
    targetPosition = currentPosition + cmd.targetPosition;
  } else {
    targetPosition = cmd.targetPosition;
  }
  
  long stepsToMove = targetPosition - currentPosition;
  xSemaphoreGive(mutex);
  
  if (stepsToMove == 0) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    isMoving = false;
    xSemaphoreGive(mutex);
    return;
  }
  
  int speed = (cmd.speed > 0) ? cmd.speed : currentSpeed;
  speed = constrain(speed, 1, maxSpeed);
  
  stepMotor(stepsToMove, speed);
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = false;
  xSemaphoreGive(mutex);
  
  Serial.printf("✅ Stepper en posición: %ld steps\n", currentPosition);
}

void StepperDriver::stepMotor(long steps, int speed) {
  // Determinar dirección
  bool forward = steps > 0;
  digitalWrite(pinDIR, forward ? HIGH : LOW);
  
  long absSteps = abs(steps);
  
  // Calcular delay entre pulsos (microsegundos)
  // speed está en steps/segundo
  unsigned long delayMicros = 1000000 / speed;
  
  // Para delays grandes (>10ms), usar vTaskDelay cada N pasos
  const int FEED_WDT_EVERY = 100; // Alimentar watchdog cada 100 pasos
  
  // Mover paso a paso
  for (long i = 0; i < absSteps; i++) {
    // Verificar endstops antes de cada paso
    if (endstopDriver != nullptr) {
      // Si vamos hacia adelante y se activa el máximo, detener
      if (forward && endstopDriver->isMaxTriggered()) {
        Serial.println("⚠️ Movimiento detenido por ENDSTOP MAX");
        break;
      }
      // Si vamos hacia atrás y se activa el mínimo, detener
      if (!forward && endstopDriver->isMinTriggered()) {
        Serial.println("⚠️ Movimiento detenido por ENDSTOP MIN");
        break;
      }
    }
    
    // Pulso
    digitalWrite(pinPUL, HIGH);
    delayMicroseconds(5);  // Mínimo 2.5µs para TB6600
    digitalWrite(pinPUL, LOW);
    
    // Actualizar posición
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (forward) {
      currentPosition++;
    } else {
      currentPosition--;
    }
    xSemaphoreGive(mutex);
    
    // Esperar según velocidad
    if (delayMicros > 10000) {
      // Si el delay es mayor a 10ms, usar vTaskDelay
      vTaskDelay(pdMS_TO_TICKS(delayMicros / 1000));
    } else {
      delayMicroseconds(delayMicros);
    }
    
    // Alimentar watchdog periódicamente
    if (i % FEED_WDT_EVERY == 0) {
      vTaskDelay(1); // Dar chance al scheduler y watchdog
    }
  }
}

bool StepperDriver::moveTo(long position, int speed, bool wait) {
  StepperCommand cmd;
  cmd.targetPosition = position;
  cmd.speed = speed;
  cmd.relative = false;
  cmd.waitCompletion = wait;
  
  if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ StepperDriver: Queue llena");
    return false;
  }
  
  if (wait) {
    while (isMoving) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  return true;
}

bool StepperDriver::moveRelative(long steps, int speed, bool wait) {
  StepperCommand cmd;
  cmd.targetPosition = steps;
  cmd.speed = speed;
  cmd.relative = true;
  cmd.waitCompletion = wait;
  
  if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ StepperDriver: Queue llena");
    return false;
  }
  
  if (wait) {
    while (isMoving) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  return true;
}

void StepperDriver::enable() {
  if (pinENA >= 0) {
    digitalWrite(pinENA, LOW);  // Activo bajo
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  isEnabled = true;
  xSemaphoreGive(mutex);
  Serial.println("✅ Stepper habilitado");
}

void StepperDriver::disable() {
  if (pinENA >= 0) {
    digitalWrite(pinENA, HIGH);  // Activo bajo
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  isEnabled = false;
  xSemaphoreGive(mutex);
  Serial.println("⚪ Stepper deshabilitado");
}

void StepperDriver::stop() {
  xQueueReset(commandQueue);
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = false;
  targetPosition = currentPosition;
  xSemaphoreGive(mutex);
}

void StepperDriver::setSpeed(int speed) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  currentSpeed = constrain(speed, 1, maxSpeed);
  xSemaphoreGive(mutex);
}

void StepperDriver::setMaxSpeed(int speed) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  maxSpeed = speed;
  xSemaphoreGive(mutex);
}

void StepperDriver::setAcceleration(int accel) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  acceleration = accel;
  xSemaphoreGive(mutex);
}

void StepperDriver::setStepsPerRevolution(int steps) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  stepsPerRevolution = steps;
  xSemaphoreGive(mutex);
}

void StepperDriver::zero() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  currentPosition = 0;
  targetPosition = 0;
  xSemaphoreGive(mutex);
  Serial.println("🔄 Posición reseteada a 0");
}

long StepperDriver::mmToSteps(float mm, float mmPerRevolution) {
  return (long)((mm / mmPerRevolution) * stepsPerRevolution);
}

float StepperDriver::stepsToMm(long steps, float mmPerRevolution) {
  return ((float)steps / stepsPerRevolution) * mmPerRevolution;
}
