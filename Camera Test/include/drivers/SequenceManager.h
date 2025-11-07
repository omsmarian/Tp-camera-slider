#ifndef SEQUENCE_MANAGER_H
#define SEQUENCE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class ServoDriver;
class StepperDriver;

// Estructura de un movimiento individual
struct Movement {
  // Movimiento horizontal (stepper)
  float horizontalDistance;  // Distancia en mm
  int horizontalSpeed;       // Velocidad 0-100%
  
  // Movimiento angular (servo)
  int angle;                 // Ángulo objetivo 0-180°
  int angleSpeed;            // Velocidad angular 0-100%
  
  // Control
  bool simultaneous;         // Mover ambos motores simultáneamente
  int pauseAfter;           // Pausa después del movimiento (ms)
};

// Estructura de una secuencia completa
struct Sequence {
  String name;
  std::vector<Movement> movements;
  bool loop;
  int repeatCount;
};

class SequenceManager {
private:
  ServoDriver* servoDriver;
  StepperDriver* stepperDriver;
  
  std::vector<Sequence> sequences;
  int activeSequenceIndex;
  bool isExecuting;
  bool isPaused;
  
  TaskHandle_t executionTask;
  SemaphoreHandle_t mutex;
  
  static void executionTaskFunc(void* parameter);
  void executeMovement(const Movement& movement);
  
public:
  SequenceManager(ServoDriver* servo, StepperDriver* stepper);
  ~SequenceManager();
  
  bool begin();
  
  // Gestión de secuencias
  int createSequence(const String& name);
  bool deleteSequence(int index);
  bool addMovement(int sequenceIndex, const Movement& movement);
  bool removeMovement(int sequenceIndex, int movementIndex);
  bool clearSequence(int sequenceIndex);
  
  // Ejecución
  bool executeSequence(int sequenceIndex);
  void pause();
  void resume();
  void stop();
  
  // Información
  int getSequenceCount() const { return sequences.size(); }
  const Sequence* getSequence(int index) const;
  bool getIsExecuting() const { return isExecuting; }
  bool getIsPaused() const { return isPaused; }
  String getSequenceAsJson(int index) const;
  String getAllSequencesAsJson() const;
};

#endif
