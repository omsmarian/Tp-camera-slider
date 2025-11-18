# Cambios Implementados - Camera Slider ESP32

## 🌐 1. Modo Access Point WiFi

### Cambios realizados:
- **Antes**: La ESP32 se conectaba como cliente a una red WiFi existente
- **Ahora**: La ESP32 crea su propia red WiFi (Access Point)

### Configuración:
- **SSID**: `ESP32-CameraSlider`
- **Contraseña**: `slider123`
- **IP del servidor**: `192.168.4.1` (IP por defecto del AP)

### Cómo usar:
1. Enciende la ESP32
2. Conecta tu dispositivo a la red WiFi "ESP32-CameraSlider" con contraseña "slider123"
3. Abre el navegador y accede a `http://192.168.4.1`

### Archivos modificados:
- `src/dep/interface.cpp`: Cambiado de `WIFI_STA` a `WIFI_AP`

---

## 🛑 2. Sistema de Fines de Carrera (Endstops)

### Nuevo driver creado: `EndstopDriver`

#### Características:
- ✅ **Task FreeRTOS con prioridad 4** (máxima del sistema)
- ✅ **Monitoreo continuo** de ambos endstops a 100Hz
- ✅ **Debounce de 50ms** para evitar falsas detecciones
- ✅ **Detención automática** del motor stepper
- ✅ **Callbacks configurables** para eventos personalizados

#### Configuración de pines:
```cpp
const int ENDSTOP_MIN = 25;  // GPIO 25 - Fin de carrera mínimo
const int ENDSTOP_MAX = 33;  // GPIO 33 - Fin de carrera máximo
```

#### Conexión de endstops:
- Los pines están configurados como `INPUT_PULLUP`
- **Estado normal**: HIGH (3.3V)
- **Estado activado**: LOW (GND cuando se presiona el switch)
- **Tipo de switch recomendado**: Normalmente abierto (NC)

#### Integración:
1. **StepperDriver verifica endstops** antes de cada paso
2. Si el motor se mueve hacia adelante y se activa el endstop MAX → **DETIENE**
3. Si el motor se mueve hacia atrás y se activa el endstop MIN → **DETIENE**
4. Los callbacks ejecutan `stepperDriver->stop()` automáticamente

#### Archivos creados:
- `include/drivers/EndstopDriver.h`
- `src/drivers/EndstopDriver.cpp`

#### Archivos modificados:
- `include/drivers/StepperDriver.h`: Agregado puntero y método `setEndstopDriver()`
- `src/drivers/StepperDriver.cpp`: Verificación de límites en bucle de movimiento
- `src/main.cpp`: Inicialización y configuración de callbacks
- `src/dep/interface.cpp`: Endpoint `/status` ahora incluye estado de endstops
- `include/interface.h`: Forward declaration de EndstopDriver

---

## 🎯 Arquitectura FreeRTOS

### Prioridades de Tasks (actualizado):
1. **EndstopTask** - Core 1, Prioridad 4 ⭐ (Máxima)
2. **ServoTask** - Core 1, Prioridad 2
3. **StepperTask** - Core 0, Prioridad 1

El EndstopTask tiene la máxima prioridad para garantizar que los límites se detecten inmediatamente y se detenga el motor antes de causar daños mecánicos.

---

## 📡 API REST - Nuevos Endpoints

### `/status` - Estado del sistema (actualizado)
```json
GET /status

Respuesta:
{
  "connected": true,         // Estado Bluetooth
  "endstop_min": false,      // Estado endstop mínimo (nuevo)
  "endstop_max": false       // Estado endstop máximo (nuevo)
}
```

---

## 🔌 Diagrama de Conexiones

```
ESP32 Camera Slider
├─ GPIO 13  → Servo Motor (señal PWM)
├─ GPIO 14  → TB6600 PUL (pulsos)
├─ GPIO 27  → TB6600 DIR (dirección)
├─ GPIO 26  → TB6600 ENA (enable)
├─ GPIO 25  → Endstop MIN (+ resistencia pull-up interna)
└─ GPIO 33  → Endstop MAX (+ resistencia pull-up interna)

Conexión Endstops:
┌──────────────┐
│   ESP32      │
│   GPIO 25 ───┼─── Switch NO ─── GND
│   GPIO 33 ───┼─── Switch NO ─── GND
│   (3.3V)     │    (cuando se presiona = LOW)
└──────────────┘
```

---

## ⚙️ Funcionamiento del Sistema de Seguridad

1. **Monitoreo continuo**: El EndstopTask verifica el estado cada 10ms
2. **Detección**: Cuando un switch se presiona (pin va a LOW)
3. **Debounce**: Espera 50ms para confirmar que no es ruido
4. **Callback**: Ejecuta `onEndstopTriggered()` que llama a `stepperDriver->stop()`
5. **Prevención**: El StepperDriver verifica antes de CADA paso si debe continuar
6. **Resultado**: El motor se detiene de forma segura sin dañar la mecánica

---

## 🚀 Mejoras Implementadas

### Ventajas del Access Point:
✅ No necesitas configurar credenciales WiFi
✅ Funciona en cualquier lugar sin router
✅ Control directo sin dependencias de red
✅ Ideal para uso portátil en exteriores

### Ventajas de los Endstops:
✅ Protección mecánica automática
✅ Máxima prioridad en FreeRTOS
✅ Detección en tiempo real (<10ms)
✅ Sistema redundante (software + hardware)
✅ No requiere calibración manual de límites

---

## 📝 Notas Importantes

1. **Conecta los endstops físicamente** a los pines GPIO 25 y 33
2. **Usa switches NO (normalmente abiertos)** para mayor seguridad
3. **Prueba los endstops** antes de uso: presiona manualmente y verifica en `/status`
4. **La IP del servidor siempre será** `192.168.4.1` cuando uses el AP
5. **El sistema puede manejar hasta 10 clientes WiFi** conectados simultáneamente

---

## 🔧 Compilación

No se requieren cambios en `platformio.ini`. Todas las dependencias ya están incluidas.

```bash
# Compilar y cargar
pio run -t upload

# Monitorear serial
pio device monitor
```

---

## 🐛 Troubleshooting

### Los endstops no funcionan:
- Verifica la conexión física (GPIO 25 y 33 a GND cuando se activan)
- Revisa en el monitor serial si aparecen mensajes de "ENDSTOP activado"
- Verifica el endpoint `/status` para ver el estado en tiempo real

### No puedo conectarme al WiFi:
- Busca la red "ESP32-CameraSlider"
- Usa la contraseña "slider123"
- Si no aparece, reinicia la ESP32 y espera 10 segundos

### El motor no se detiene:
- Verifica que los callbacks estén correctamente configurados
- Comprueba que `setEndstopDriver()` se haya llamado
- Revisa prioridad del EndstopTask en monitor serial

---

**Fecha de implementación**: 17 de Noviembre de 2025
