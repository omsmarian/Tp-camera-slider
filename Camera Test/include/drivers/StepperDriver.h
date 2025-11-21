#ifndef STEPPER_DRIVER_H
#define STEPPER_DRIVER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Estructura para comandos del stepper
struct StepperCommand {
  long targetPosition;  // Posición objetivo en steps
  int speed;            // Velocidad (pasos/segundo)
  bool relative;        // Movimiento relativo o absoluto
  bool waitCompletion;  // Esperar a que termine
};

class StepperDriver {
private:
  // Pines TB6600
  int pinPUL;    // Pin de pulso (STEP)
  int pinDIR;    // Pin de dirección
  int pinENA;    // Pin de enable (opcional)
  
  // Estado
  long currentPosition;
  long targetPosition;
  int currentSpeed;
  bool isMoving;
  bool isEnabled;
  
  // Configuración
  int stepsPerRevolution;
  int maxSpeed;
  int acceleration;
  
  // FreeRTOS
  QueueHandle_t commandQueue;
  TaskHandle_t taskHandle;
  SemaphoreHandle_t mutex;
  
  static void stepperTask(void* parameter);
  void processCommand(StepperCommand cmd);
  void stepMotor(long steps, int speed);
  
public:
  StepperDriver(int pul, int dir, int ena = -1);
  ~StepperDriver();
  
  // Inicializar
  bool begin(int stepsPerRev = 200);
  
  // Control básico
  bool moveTo(long position, int speed = -1, bool wait = false);
  bool moveRelative(long steps, int speed = -1, bool wait = false);
  void stop();
  void enable();
  void disable();
  
  // Configuración
  void setSpeed(int speed);
  void setMaxSpeed(int speed);
  void setAcceleration(int accel);
  void setStepsPerRevolution(int steps);
  void zero(); // Establecer posición actual como 0
  
  // Información
  long getCurrentPosition() const { return currentPosition; }
  bool getIsMoving() const { return isMoving; }
  bool getIsEnabled() const { return isEnabled; }
  
  // Conversiones útiles
  long mmToSteps(float mm, float mmPerRevolution);
  float stepsToMm(long steps, float mmPerRevolution);
};

#endif
