#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Estructura para comandos del servo
struct ServoCommand {
  int targetAngle;      // Ángulo objetivo (0-180)
  int speed;            // Velocidad del movimiento (0-100%)
  bool waitCompletion;  // Esperar a que termine el movimiento
};

class ServoDriver {
private:
  Servo servo;
  int pin;
  int currentAngle;
  int defaultSpeed;
  bool isMoving;
  
  QueueHandle_t commandQueue;
  TaskHandle_t taskHandle;
  SemaphoreHandle_t mutex;
  
  static void servoTask(void* parameter);
  void processCommand(ServoCommand cmd);
  
public:
  ServoDriver(int servoPin);
  ~ServoDriver();
  
  // Inicializar el driver
  bool begin();
  
  // Enviar comando de movimiento
  bool moveTo(int angle, int speed = -1, bool wait = false);
  
  // Obtener información
  int getCurrentAngle() const { return currentAngle; }
  bool getIsMoving() const { return isMoving; }
  
  // Configuración
  void setDefaultSpeed(int speed);
  
  // Detener movimiento
  void stop();
};

#endif
