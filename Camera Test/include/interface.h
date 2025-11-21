#ifndef INTERFACE_H
#define INTERFACE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>

// Forward declarations
class ServoDriver;
class StepperDriver;
class SequenceManager;

// Función para inicializar el servidor web
void setupWebServer();

// Función para manejar el disparo de foto (callback)
void setPhotoCallback(void (*callback)());

// Función para obtener el estado de conexión BLE
void updateBLEStatus(bool connected);

#endif