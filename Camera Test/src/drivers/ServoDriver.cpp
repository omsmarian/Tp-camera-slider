#include "drivers/ServoDriver.h"
#include <esp_task_wdt.h>

ServoDriver::ServoDriver(int servoPin) 
  : pin(servoPin), currentAngle(90), defaultSpeed(50), isMoving(false),
    servoAttached(false) {
  commandQueue = nullptr;
  taskHandle = nullptr;
  mutex = nullptr;
}

ServoDriver::~ServoDriver() {
  if (taskHandle != nullptr) {
    vTaskDelete(taskHandle);
  }
  if (commandQueue != nullptr) {
    vQueueDelete(commandQueue);
  }
  if (mutex != nullptr) {
    vSemaphoreDelete(mutex);
  }
  if (servoAttached) {
    servo.detach();
  }
}

bool ServoDriver::begin() {
  // Configurar servo
  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servoAttached = false;
  
  // Crear mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) {
    Serial.println("❌ ServoDriver: Error creando mutex");
    return false;
  }
  
  // Crear cola de comandos
  commandQueue = xQueueCreate(10, sizeof(ServoCommand));
  if (commandQueue == nullptr) {
    Serial.println("❌ ServoDriver: Error creando queue");
    return false;
  }
  
  // Crear task
  BaseType_t result = xTaskCreatePinnedToCore(
    servoTask,
    "ServoTask",
    8192,  // Aumentar stack a 8KB
    this,
    2,
    &taskHandle,
    1  // Core 1
  );
  
  if (result != pdPASS) {
    Serial.println("❌ ServoDriver: Error creando task");
    return false;
  }
  
  Serial.printf("✅ ServoDriver inicializado en pin %d\n", pin);
  return true;
}

void ServoDriver::servoTask(void* parameter) {
  ServoDriver* driver = static_cast<ServoDriver*>(parameter);
  ServoCommand cmd;
  
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

void ServoDriver::processCommand(ServoCommand cmd) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = true;
  xSemaphoreGive(mutex);
  
  int targetAngle = constrain(cmd.targetAngle, 0, 180);
  int speed = (cmd.speed < 0) ? defaultSpeed : cmd.speed;
  speed = constrain(speed, 1, 100);

  if (!servoAttached) {
    servo.attach(pin, 500, 2400);
    servo.write(targetAngle);
    currentAngle = targetAngle;
    servoAttached = true;

    xSemaphoreTake(mutex, portMAX_DELAY);
    isMoving = false;
    xSemaphoreGive(mutex);

    Serial.printf("✅ Servo activado tras primer comando: %d°\n", currentAngle);
    return;
  }
  
  // Calcular delay basado en velocidad (1-100% -> 20ms-1ms)
  int delayTime = map(speed, 0, 100, 20, 1);
  
  // Mover suavemente
  int steps = abs(targetAngle - currentAngle);
  int direction = (targetAngle > currentAngle) ? 1 : -1;
  
  for (int i = 0; i < steps; i++) {
    currentAngle += direction;
    servo.write(currentAngle);
    
    // Usar vTaskDelay para no bloquear el watchdog
    if (delayTime > 0) {
      vTaskDelay(pdMS_TO_TICKS(delayTime));
    } else {
      vTaskDelay(1); // Mínimo 1 tick para dar chance al scheduler
    }
  }
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = false;
  xSemaphoreGive(mutex);
  
  Serial.printf("✅ Servo en posición: %d°\n", currentAngle);
}

bool ServoDriver::moveTo(int angle, int speed, bool wait) {
  ServoCommand cmd;
  cmd.targetAngle = angle;
  cmd.speed = speed;
  cmd.waitCompletion = wait;
  
  if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ ServoDriver: Queue llena");
    return false;
  }
  
  if (wait) {
    // Esperar a que termine el movimiento
    while (isMoving) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  return true;
}

void ServoDriver::setDefaultSpeed(int speed) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  defaultSpeed = constrain(speed, 1, 100);
  xSemaphoreGive(mutex);
}

void ServoDriver::stop() {
  // Limpiar la cola de comandos
  xQueueReset(commandQueue);
  xSemaphoreTake(mutex, portMAX_DELAY);
  isMoving = false;
  xSemaphoreGive(mutex);
}
