#ifndef ENDSTOP_DRIVER_H
#define ENDSTOP_DRIVER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class EndstopDriver {
private:
  // Pines de los endstops
  int pinMin;           // Fin de carrera mínimo (NC - Normally Closed)
  int pinMax;           // Fin de carrera máximo (NC - Normally Closed)
  
  // Estado
  volatile bool minTriggered;
  volatile bool maxTriggered;
  
  // Callbacks
  void (*onMinTriggerCallback)();
  void (*onMaxTriggerCallback)();
  
  // FreeRTOS
  TaskHandle_t taskHandle;
  SemaphoreHandle_t mutex;
  
  // Debounce
  unsigned long lastMinTrigger;
  unsigned long lastMaxTrigger;
  const unsigned long debounceDelay = 50; // 50ms debounce
  
  static void endstopTask(void* parameter);
  void checkEndstops();
  
public:
  EndstopDriver(int pinMin, int pinMax);
  ~EndstopDriver();
  
  // Inicializar
  bool begin();
  
  // Estado
  bool isMinTriggered() const { return minTriggered; }
  bool isMaxTriggered() const { return maxTriggered; }
  bool isAnyTriggered() const { return minTriggered || maxTriggered; }
  
  // Callbacks
  void setMinTriggerCallback(void (*callback)());
  void setMaxTriggerCallback(void (*callback)());
  
  // Control
  void reset(); // Resetear estado (útil después de movimiento manual)
};

#endif
