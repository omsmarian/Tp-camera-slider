#ifndef STEPPER_DRIVER_H
#define STEPPER_DRIVER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class LimitSwitchDriver;

struct StepperCommand {
  long targetPosition;  
  int speed;            
  bool relative;        
  bool waitCompletion;  
};

class StepperDriver {
private:
  // Pines TB6600
  int pinPUL;    
  int pinDIR;    
  int pinENA;    

  // NUEVO: Pines Finales de Carrera
  int pinLimit1;
  int pinLimit2;
  
  // Pin LED verde (indica movimiento)
  int pinLedGreen;
  
  // Referencia al flag de emergencia externo
  volatile bool* emergencyStopFlag;
  portMUX_TYPE* emergencyMutex;
  
  // Estado
  long currentPosition;
  long targetPosition;
  int currentSpeed;
  bool isMoving;
  bool isEnabled;
  volatile bool shouldAbort;
  portMUX_TYPE abortMux;
  
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
  
  friend class LimitSwitchDriver;

public:
  // Constructor actualizado: recibe los dos limites y el LED verde
  StepperDriver(int pul, int dir, int ena, int lim1, int lim2, int ledGreen);
  ~StepperDriver();
  
  void setEmergencyFlag(volatile bool* flag, portMUX_TYPE* mutex);
  
  bool begin(int stepsPerRev = 200);
  
  bool moveTo(long position, int speed = -1, bool wait = false);
  bool moveRelative(long steps, int speed = -1, bool wait = false);
  void stop();
  void enable();
  void disable();
  
  void setSpeed(int speed);
  void setMaxSpeed(int speed);
  void setAcceleration(int accel);
  void setStepsPerRevolution(int steps);
  void zero(); 
  
  long getCurrentPosition() const { return currentPosition; }
  bool getIsMoving() const { return isMoving; }
  bool getIsEnabled() const { return isEnabled; }
  
  long mmToSteps(float mm, float mmPerRevolution);
  float stepsToMm(long steps, float mmPerRevolution);
};

#endif