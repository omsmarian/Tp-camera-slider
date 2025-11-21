#include "drivers/StepperDriver.h"
#include <esp_task_wdt.h>

// Constructor actualizado
StepperDriver::StepperDriver(int pul, int dir, int ena, int lim1, int lim2)
  : pinPUL(pul), pinDIR(dir), pinENA(ena), pinLimit1(lim1), pinLimit2(lim2),
    currentPosition(0), targetPosition(0), currentSpeed(1000),
    isMoving(false), isEnabled(false), shouldAbort(false),
    stepsPerRevolution(200), maxSpeed(2000), acceleration(500) {
  
  emergencyStopFlag = nullptr;
  emergencyMutex = nullptr;
  commandQueue = nullptr;
  taskHandle = nullptr;
  mutex = nullptr;
  portMUX_INITIALIZE(&abortMux);
}

StepperDriver::~StepperDriver() {
  disable();
  if (taskHandle != nullptr) vTaskDelete(taskHandle);
  if (commandQueue != nullptr) vQueueDelete(commandQueue);
  if (mutex != nullptr) vSemaphoreDelete(mutex);
}

bool StepperDriver::begin(int stepsPerRev) {
  stepsPerRevolution = stepsPerRev;
  
  // Configurar pines Motor
  pinMode(pinPUL, OUTPUT);
  pinMode(pinDIR, OUTPUT);
  if (pinENA >= 0) {
    pinMode(pinENA, OUTPUT);
    digitalWrite(pinENA, HIGH); 
  }

  // NUEVO: Configurar Finales de Carrera (INPUT_PULLUP)
  // NC a GND -> LOW = Cerrado (OK), HIGH = Abierto (Tope)
  pinMode(pinLimit1, INPUT_PULLDOWN);
  pinMode(pinLimit2, INPUT_PULLDOWN);
  
  digitalWrite(pinPUL, LOW);
  digitalWrite(pinDIR, LOW);
  
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) return false;
  
  commandQueue = xQueueCreate(10, sizeof(StepperCommand));
  if (commandQueue == nullptr) return false;
  
  BaseType_t result = xTaskCreatePinnedToCore(
    stepperTask, "StepperTask", 8192, this, 1, &taskHandle, 0
  );
  
  if (result != pdPASS) return false;
  
  return true;
}

void StepperDriver::stepperTask(void* parameter) {
  StepperDriver* driver = static_cast<StepperDriver*>(parameter);
  StepperCommand cmd;
  esp_task_wdt_add(NULL);
  
  while (true) {
    esp_task_wdt_reset();
    if (xQueueReceive(driver->commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      driver->processCommand(cmd);
    }
  }
}

void StepperDriver::processCommand(StepperCommand cmd) {
  if (!isEnabled) return;
  
  esp_task_wdt_reset();
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = true;
  shouldAbort = false;

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
}

void StepperDriver::stepMotor(long steps, int speed) {
  bool forward = steps > 0;
  digitalWrite(pinDIR, forward ? HIGH : LOW);
  
  long absSteps = abs(steps);
  unsigned long delayMicros = 1000000 / speed;
  const int FEED_WDT_EVERY = 100;
  
  for (long i = 0; i < absSteps; i++) {
    // === PROTECCIÓN DE FINALES DE CARRERA ===
    // Leemos sensores. Si es NC, HIGH significa que chocó.
    bool hitLimit1 = digitalRead(pinLimit1) == LOW; 
    bool hitLimit2 = digitalRead(pinLimit2) == LOW;

    // Si voy hacia atrás (Start) y toco Limit1 -> Parar
    if (!forward && hitLimit1) {
       Serial.println("⛔ LIMITE 1 (Inicio) Alcanzado");
       break; 
    }
    // Si voy hacia adelante (Fin) y toco Limit2 -> Parar
    if (forward && hitLimit2) {
       Serial.println("⛔ LIMITE 2 (Final) Alcanzado");
       break; 
    }
    // ========================================

    if (shouldAbort) break;
    
    digitalWrite(pinPUL, HIGH);
    delayMicroseconds(5);
    digitalWrite(pinPUL, LOW);
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (forward) currentPosition++;
    else currentPosition--;
    xSemaphoreGive(mutex);
    
    if (delayMicros > 10000) {
      vTaskDelay(pdMS_TO_TICKS(delayMicros / 1000));
    } else {
      delayMicroseconds(delayMicros);
    }
    
    if (i % FEED_WDT_EVERY == 0) vTaskDelay(1);
  }
}

// === RESTO DE FUNCIONES IGUALES ===

void StepperDriver::setEmergencyFlag(volatile bool* flag, portMUX_TYPE* mutex) {
  emergencyStopFlag = flag;
  emergencyMutex = mutex;
}

bool StepperDriver::moveTo(long position, int speed, bool wait) {
  StepperCommand cmd = {position, speed, false, wait};
  if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  if (wait) while (isMoving) vTaskDelay(pdMS_TO_TICKS(10));
  return true;
}

bool StepperDriver::moveRelative(long steps, int speed, bool wait) {
  StepperCommand cmd = {steps, speed, true, wait};
  if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  if (wait) while (isMoving) vTaskDelay(pdMS_TO_TICKS(10));
  return true;
}

void StepperDriver::stop() {
  portENTER_CRITICAL(&abortMux);
  shouldAbort = true;
  portEXIT_CRITICAL(&abortMux);
  xQueueReset(commandQueue);
}

void StepperDriver::enable() {
  if (pinENA >= 0) digitalWrite(pinENA, LOW);
  xSemaphoreTake(mutex, portMAX_DELAY);
  isEnabled = true;
  xSemaphoreGive(mutex);
}

void StepperDriver::disable() {
  if (pinENA >= 0) digitalWrite(pinENA, HIGH);
  xSemaphoreTake(mutex, portMAX_DELAY);
  isEnabled = false;
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
}

long StepperDriver::mmToSteps(float mm, float mmPerRevolution) {
  return (long)((mm / mmPerRevolution) * stepsPerRevolution);
}

float StepperDriver::stepsToMm(long steps, float mmPerRevolution) {
  return ((float)steps / stepsPerRevolution) * mmPerRevolution;
}