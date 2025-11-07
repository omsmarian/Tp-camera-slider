# 📐 Camera Slider Control System - Documentación

## 🏗️ Arquitectura FreeRTOS

### Estructura de Drivers

El sistema está organizado en 3 drivers principales que se ejecutan en tasks independientes:

```
┌─────────────────────────────────────────────────┐
│                   MAIN TASK                      │
│              (Bluetooth Manager)                 │
└────────────┬────────────────────────────────────┘
             │
       ┌─────┴────────┐
       │              │
┌──────▼──────┐  ┌───▼──────────┐
│  CORE 0     │  │   CORE 1     │
│             │  │              │
│ StepperTask │  │  ServoTask   │
│ Priority: 3 │  │  Priority: 2 │
└─────────────┘  └──────────────┘
       │                 │
       └────────┬────────┘
                │
      ┌─────────▼──────────┐
      │  SequenceManager   │
      │   (Ejecutor de     │
      │    movimientos)    │
      └────────────────────┘
```

---

## 🔧 Componentes del Sistema

### 1. **ServoDriver** (`include/drivers/ServoDriver.h`)

Controla el servo del ángulo de cámara usando FreeRTOS.

**Características:**
- Task dedicada en Core 1 (prioridad 2)
- Queue para comandos asíncronos
- Movimiento suave con control de velocidad
- Mutex para acceso seguro a variables compartidas

**API Principal:**
```cpp
ServoDriver servo(SERVO_PIN);
servo.begin();
servo.moveTo(angle, speed, waitComplete);
servo.setDefaultSpeed(speed);
servo.stop();
```

**Funcionamiento:**
- Recibe comandos vía queue (no bloqueante)
- Ejecuta movimientos paso a paso
- Velocidad: 0-100% → 20ms-1ms por paso
- Rango: 0-180°

---

### 2. **StepperDriver** (`include/drivers/StepperDriver.h`)

Controla el motor stepper con driver TB6600.

**Características:**
- Task dedicada en Core 0 (prioridad 3 - alta para timing preciso)
- Control de posición absoluta y relativa
- Conversión mm ↔ steps
- Enable/Disable del motor

**API Principal:**
```cpp
StepperDriver stepper(PUL_PIN, DIR_PIN, ENA_PIN);
stepper.begin(stepsPerRevolution);
stepper.enable();
stepper.moveTo(position, speed, wait);
stepper.moveRelative(steps, speed, wait);
stepper.zero();  // Reset posición
```

**Conexión TB6600:**
```
ESP32 GPIO 14 → PUL+ (Pulsos/Steps)
ESP32 GPIO 27 → DIR+ (Dirección)
ESP32 GPIO 26 → ENA+ (Enable)
GND → PUL-, DIR-, ENA-
```

**Configuración TB6600:**
- Microstepping: Configurable por DIP switches
- Velocidad: 100-2000 steps/segundo
- Timing: 5µs pulse width (compatible TB6600)

---

### 3. **SequenceManager** (`include/drivers/SequenceManager.h`)

Gestiona y ejecuta secuencias de movimientos programados.

**Características:**
- Almacena múltiples secuencias
- Movimientos simultáneos o secuenciales
- Pausa/Resume/Stop
- Loop y repeat count

**Estructura de Movimiento:**
```cpp
Movement {
  float horizontalDistance;  // mm
  int horizontalSpeed;       // 0-100%
  int angle;                 // 0-180°
  int angleSpeed;            // 0-100%
  bool simultaneous;         // Mover ambos a la vez
  int pauseAfter;           // ms
}
```

**API Principal:**
```cpp
SequenceManager seqMgr(&servo, &stepper);
seqMgr.begin();
int seqIdx = seqMgr.createSequence("Mi Secuencia");
seqMgr.addMovement(seqIdx, movement);
seqMgr.executeSequence(seqIdx);
seqMgr.pause() / resume() / stop();
```

---

## 🌐 Interfaz Web - Endpoints REST

### Control Manual

#### Servo
```
GET /servo?angle=90&speed=50
Response: {"success":true,"angle":90,"speed":50}
```

#### Stepper
```
GET /stepper?distance=100&speed=50
Response: {"success":true,"distance":100,"speed":50}

GET /stepper/enable?value=true
GET /stepper/zero
```

### Gestión de Secuencias

#### Crear secuencia
```
POST /sequence/create
Body: name=MiSecuencia
Response: {"success":true,"index":0}
```

#### Agregar movimiento
```
POST /sequence/add
Body: seq=0&distance=100&speed=50&angle=90&angleSpeed=50&simultaneous=false&pause=1000
```

#### Ejecutar secuencia
```
GET /sequence/execute?index=0
GET /sequence/pause
GET /sequence/resume
GET /sequence/stop
```

#### Consultar secuencias
```
GET /sequence/list
Response: [array de secuencias]

GET /sequence/get?index=0
Response: {secuencia con todos sus movimientos}
```

### Estado del sistema
```
GET /status
Response: {"connected":true/false}  // Estado BLE
```

### Control de cámara
```
GET /photo
Response: {"success":true}  // Dispara foto vía BLE
```

---

## 📱 Interfaz Web - Funcionalidades

### 1. **Control Manual**
- **Stepper:** Slider de distancia (-500 a +500mm) y velocidad
- **Servo:** Slider de ángulo (0-180°) y velocidad
- Botones para ejecutar movimientos
- Reset de posición del stepper

### 2. **Programación de Secuencias**
- Formulario para agregar movimientos:
  - Distancia horizontal (mm)
  - Velocidad del stepper (%)
  - Ángulo de cámara (°)
  - Velocidad del servo (%)
  - Pausa después del movimiento (ms)
  - Checkbox para movimiento simultáneo
- Lista visual de movimientos agregados
- Botones para ejecutar o limpiar secuencia

### 3. **Control de Cámara**
- Botón para disparar foto (BLE)
- Indicador de estado de conexión Bluetooth

---

## ⚙️ Configuración de Hardware

### Pines por Defecto (main.cpp)

```cpp
// Servo
const int SERVO_PIN = 13;

// Stepper (TB6600)
const int STEPPER_PUL = 14;
const int STEPPER_DIR = 27;
const int STEPPER_ENA = 26;
```

### Alimentación
- **Servo:** 5V (máx 2A) - Usar fuente externa si es >9g
- **Stepper + TB6600:** 12-24V DC (según motor)
- **ESP32:** USB o VIN (5V regulado)

### Especificaciones TB6600
- Corriente: 0.5A - 4A (ajustable)
- Voltaje: 9-42V DC
- Microstepping: 1, 2, 4, 8, 16 (DIP switches)

---

## 🚀 Cómo Usar

### 1. Primera Configuración

```cpp
// En main.cpp, ajusta tus pines si es necesario
const int SERVO_PIN = 13;
const int STEPPER_PUL = 14;
// ...
```

### 2. Compilar y Subir

```bash
# Compilar código
pio run --environment denky32

# Subir filesystem (HTML/CSS/JS)
pio run --target uploadfs --environment denky32

# Subir código
pio run --target upload --environment denky32

# Monitor serial
pio device monitor
```

### 3. Conectar Hardware

1. **Servo:**
   - Signal → GPIO 13
   - VCC → 5V
   - GND → GND

2. **TB6600:**
   - PUL+ → GPIO 14, PUL- → GND
   - DIR+ → GPIO 27, DIR- → GND
   - ENA+ → GPIO 26, ENA- → GND
   - VCC/GND → Fuente 12-24V
   - Motor → A+, A-, B+, B-

3. **Calibración TB6600:**
   - SW1-SW3: Microstepping (ej: ON-ON-OFF = 1/4 step)
   - SW4-SW6: Corriente (según motor)

### 4. Conectarse

1. **Bluetooth:** Emparejar "ESP Camera Slider"
2. **WiFi:** Conectar a red "Mariano"
3. **Web:** Abrir IP mostrada en monitor serial

### 5. Crear una Secuencia

1. En "Programar Secuencia", llenar el formulario
2. Clic en "➕ Agregar Movimiento"
3. Repetir para cada paso
4. Clic en "▶️ Ejecutar"

---

## 🔍 Debug y Monitoreo

### Monitor Serial

El sistema imprime información detallada:

```
╔════════════════════════════════════════╗
║  ESP32 Camera Slider Control System   ║
║         FreeRTOS Architecture          ║
╚════════════════════════════════════════╝

🔧 Inicializando drivers...
  → ServoDriver...
✅ ServoDriver inicializado en pin 13
  → StepperDriver...
✅ StepperDriver inicializado (PUL:14 DIR:27 ENA:26)
  → SequenceManager...
✅ SequenceManager inicializado

📡 Iniciando Bluetooth...
✅ Bluetooth iniciado

🌐 Configurando servidor web...
✅ LittleFS montado correctamente
✅ WiFi conectado!
🌐 IP: http://192.168.1.100

╔════════════════════════════════════════╗
║           ✅ SISTEMA LISTO             ║
╚════════════════════════════════════════╝
```

### Logs de Operación

```
🔧 Moviendo servo de 90° a 45°
✅ Servo en posición: 45°

🚂 Moviendo stepper...
✅ Stepper en posición: 1600 steps

▶️ Ejecutando secuencia: MiSecuencia
📍 Movimiento 1/3
⚙️ Movimiento simultáneo
✅ Secuencia completada
```

---

## 📊 Rendimiento

### Recursos FreeRTOS

| Task | Core | Stack | Prioridad | Función |
|------|------|-------|-----------|---------|
| ServoTask | 1 | 4KB | 2 | Control servo |
| StepperTask | 0 | 4KB | 3 | Control stepper |
| SequenceTask | 1 | 8KB | 1 | Ejecución secuencias |

### Memoria

- **RAM:** ~80KB usada (150KB libres)
- **Flash:** ~800KB programa + ~100KB filesystem
- **Queues:** 10 comandos por driver

---

## 🛠️ Personalización

### Ajustar Velocidades

```cpp
// En main.cpp setup()
servoDriver->setDefaultSpeed(30);  // Más lento
stepperDriver->setMaxSpeed(3000);  // Más rápido
```

### Cambiar mm por Revolución

```cpp
// En SequenceManager.cpp, línea ~160
long steps = stepperDriver->mmToSteps(distance, 8.0);
//                                             ↑ cambiar según tu sistema
```

### Calibración del Servo

```cpp
// En ServoDriver.cpp, línea ~29
servo.attach(pin, 500, 2400);  // Ajustar pulse width min/max
```

---

## 📝 Próximas Mejoras Sugeridas

1. **Persistencia:** Guardar secuencias en LittleFS
2. **Aceleración:** Rampa suave para stepper
3. **Límites:** Detección de fin de carrera
4. **Interfaz:** Guardar/cargar secuencias desde web
5. **Telemetría:** WebSocket para feedback en tiempo real
6. **Autofocus:** Trigger de enfoque antes de foto

---

## 🐛 Troubleshooting

### El servo no se mueve
- Verificar alimentación (externa si es necesario)
- Revisar pin en código
- Probar con `servo.write(90)` directo

### El stepper vibra pero no gira
- Revisar conexiones A+/A-/B+/B- del motor
- Ajustar corriente en TB6600
- Verificar microstepping
- Probar velocidad más baja

### No conecta WiFi
- Cambiar SSID/Password en `interface.cpp`
- Verificar que router esté en 2.4GHz

### Bluetooth no conecta
- Eliminar pairing anterior
- Reiniciar ESP32
- Verificar que BLE esté activo en el dispositivo

### Web no muestra sliders
- Verificar que ejecutaste `uploadfs`
- Revisar archivos en LittleFS (serial log)
- Limpiar caché del navegador

---

## 📄 Licencia

Este proyecto es de código abierto. Úsalo, modifícalo y compártelo.

**Creado con ❤️ usando:**
- ESP32
- FreeRTOS
- PlatformIO
- ESPAsyncWebServer

**Fecha:** Octubre 2025
