# 🔌 Diagrama de Conexiones - Camera Slider

## Conexión Completa del Sistema

```
┌─────────────────────────────────────────────────────────────────┐
│                           ESP32                                  │
│                                                                  │
│  GPIO 13 ────────────────────────────┐                         │
│                                       │                          │
│  GPIO 14 ────────┐                   │                          │
│  GPIO 27 ────────┼───────────────┐   │                          │
│  GPIO 26 ────────┼───────┐       │   │                          │
│                  │       │       │   │                          │
│  5V ─────────────┼───────┼───────┼───┼─────────────────┐       │
│  GND ────────────┼───────┼───────┼───┼─────────────┐   │       │
│                  │       │       │   │             │   │       │
└──────────────────┼───────┼───────┼───┼─────────────┼───┼───────┘
                   │       │       │   │             │   │
                   │       │       │   │             │   │
┌──────────────────▼───────▼───────▼───┤             │   │
│           TB6600 Driver               │             │   │
│  ┌─────────────────────────┐          │             │   │
│  │  PUL+  DIR+  ENA+  VCC │          │             │   │
│  │   14    27    26        │          │             │   │
│  │                         │          │             │   │
│  │  PUL-  DIR-  ENA-  GND │◄─────────┼─────────────┘   │
│  └─────────────────────────┘          │                 │
│                                        │                 │
│  ┌────────────────┐                   │                 │
│  │  12-24V DC     │                   │                 │
│  │  Power Input   │                   │                 │
│  └────────────────┘                   │                 │
│                                        │                 │
│  ┌────────────────┐                   │                 │
│  │  A+  A-       │ ◄─────────────────┘                 │
│  │  B+  B-       │   Stepper Motor                     │
│  └────────────────┘                                     │
└─────────────────────────────────────────────────────────┘

                   ┌──────────────────┐
                   │   Servo Motor    │
                   │                  │
                   │  Signal ◄────────┼─── GPIO 13
                   │  VCC    ◄────────┼─── 5V
                   │  GND    ◄────────┘─── GND
                   └──────────────────┘
```

---

## Tabla de Conexiones

### ESP32 → TB6600

| ESP32 Pin | TB6600 Terminal | Función |
|-----------|----------------|---------|
| GPIO 14 | PUL+ | Pulsos (Steps) |
| GPIO 27 | DIR+ | Dirección |
| GPIO 26 | ENA+ | Enable/Disable |
| GND | PUL-, DIR-, ENA- | Tierra común |

### TB6600 → Motor Stepper

| TB6600 Terminal | Stepper Wire | Notas |
|-----------------|--------------|-------|
| A+ | Bobina A+ | Cable rojo o negro |
| A- | Bobina A- | Cable verde o amarillo |
| B+ | Bobina B+ | Cable azul o blanco |
| B- | Bobina B- | Cable naranja o marrón |

**⚠️ Importante:** Si el motor gira al revés, intercambia A+ con A- o B+ con B-.

### TB6600 Alimentación

| Terminal | Conexión | Voltaje |
|----------|----------|---------|
| VCC | Fuente + | 12-24V DC |
| GND | Fuente - | 0V |

**Consumo típico:** 1-3A según motor

### ESP32 → Servo

| ESP32 Pin | Servo Wire | Color Típico |
|-----------|------------|--------------|
| GPIO 13 | Signal | Naranja/Amarillo |
| 5V | VCC | Rojo |
| GND | GND | Marrón/Negro |

**⚠️ Alimentación:** Si usas servo grande (>9g), alimenta desde fuente externa de 5V.

---

## Configuración DIP Switches TB6600

### SW1-SW3: Microstepping

| SW1 | SW2 | SW3 | Microstepping | Steps/Rev (motor 200) |
|-----|-----|-----|---------------|----------------------|
| ON | ON | ON | 1 (Full) | 200 |
| OFF | ON | ON | 1/2 | 400 |
| ON | OFF | ON | 1/4 | 800 |
| OFF | OFF | ON | 1/8 | 1600 |
| ON | ON | OFF | 1/16 | 3200 |
| OFF | ON | OFF | 1/32 | 6400 |

**Recomendado:** 1/4 step (SW1:ON, SW2:OFF, SW3:ON) para suavidad sin perder torque.

### SW4-SW6: Corriente Pico

| SW4 | SW5 | SW6 | Corriente Pico |
|-----|-----|-----|---------------|
| ON | ON | ON | 0.5A |
| OFF | ON | ON | 1.0A |
| ON | OFF | ON | 1.5A |
| OFF | OFF | ON | 2.0A |
| ON | ON | OFF | 2.5A |
| OFF | ON | OFF | 2.8A |
| ON | OFF | OFF | 3.0A |
| OFF | OFF | OFF | 3.5A |

**⚠️ Configurar según tu motor:** Revisar datasheet y ajustar al 70-80% de la corriente nominal.

---

## Lista de Materiales (BOM)

### Componentes Principales

| Cantidad | Componente | Especificaciones | Precio Aprox. |
|----------|------------|------------------|---------------|
| 1 | ESP32 DevKit | 30 pines, WiFi+BLE | $5-10 |
| 1 | Driver TB6600 | 0.5-4A, 9-42V | $8-15 |
| 1 | Motor Stepper NEMA 17 | 200 steps, 1.5A, 42mm | $10-20 |
| 1 | Servo SG90 o MG996R | 9g o 55g, 180° | $2-8 |
| 1 | Fuente 12V 3A | Para TB6600 + motor | $8-12 |
| 1 | Fuente 5V 2A | Para servo (opcional) | $5-8 |
| - | Cables Dupont | M-M, M-F | $3-5 |

### Materiales Estructura (Ejemplo)

| Cantidad | Componente | Notas |
|----------|------------|-------|
| 1 | Perfil de aluminio 40x40 | 1 metro, riel principal |
| 1 | Correa GT2 | 1-2 metros |
| 2 | Poleas GT2 | 20 dientes, eje 5mm |
| 4 | Rodamientos lineales | LM8UU (8mm) |
| 2 | Varillas lisas 8mm | 1 metro c/u |
| 1 | Plataforma móvil | Impresión 3D o acrílico |

---

## Esquema de Alimentación

```
┌─────────────────┐
│  Fuente 12V 3A  │
└───────┬─────────┘
        │
        ├──────────► TB6600 (VCC/GND)
        │            │
        │            └──────► Motor Stepper
        │
        │  ┌──────────────┐
        └──┤ Buck DC-DC   │ (Opcional: si alimentas todo desde 12V)
           │ 12V → 5V 3A  │
           └──────┬───────┘
                  │
                  ├──────────► Servo Motor
                  │
                  └──────────► ESP32 (VIN)
```

**Opción simple:** 
- Fuente 12V → TB6600
- USB → ESP32
- Pin 5V ESP32 → Servo (solo si es pequeño)

---

## Tips de Instalación

### 1. Orden de Conexión
1. ✅ Conectar todo SIN alimentación
2. ✅ Verificar continuidad con multímetro
3. ✅ Configurar DIP switches TB6600
4. ✅ Conectar alimentación del TB6600
5. ✅ Conectar USB al ESP32
6. ✅ Subir código y probar

### 2. Verificación de Motores

**Stepper:**
```cpp
// Test rápido en setup()
stepperDriver->enable();
stepperDriver->moveRelative(200, 500, true);  // 1 revolución
```

**Servo:**
```cpp
// Test rápido en setup()
servoDriver->moveTo(0, 50, true);
delay(1000);
servoDriver->moveTo(180, 50, true);
```

### 3. Seguridad

- ⚠️ Nunca desconectar motor stepper con TB6600 energizado
- ⚠️ Verificar polaridad antes de energizar
- ⚠️ Usar fuente con protección de cortocircuito
- ⚠️ No tocar cables del motor en movimiento (voltaje inducido)

### 4. Calibración

1. **Determinar mm/revolución:**
   - Marcar posición inicial
   - Girar motor 10 revoluciones
   - Medir distancia recorrida
   - Calcular: distancia / 10

2. **Ajustar en código:**
   ```cpp
   // En SequenceManager.cpp línea ~160
   long steps = stepperDriver->mmToSteps(distance, TU_VALOR_AQUI);
   ```

---

## Troubleshooting Eléctrico

### Motor Stepper no gira

1. ✅ Verificar LED de power en TB6600
2. ✅ Verificar voltaje en VCC (12-24V)
3. ✅ Verificar que ENA+ esté en LOW (habilitado)
4. ✅ Probar con velocidad más baja
5. ✅ Verificar corriente configurada (DIP switches)

### Motor vibra pero no avanza

1. ✅ Intercambiar cables A con B
2. ✅ Probar microstepping diferente
3. ✅ Aumentar corriente (DIP switches)
4. ✅ Reducir velocidad en código

### Servo tiembla

1. ✅ Usar fuente externa de 5V
2. ✅ Capacitor 100µF cerca del servo
3. ✅ Cable de señal corto (<30cm)
4. ✅ Verificar GND común

### ESP32 se reinicia

1. ✅ Alimentar servo desde fuente externa
2. ✅ GND común entre todas las fuentes
3. ✅ Capacitor 100µF en VIN del ESP32
4. ✅ No alimentar motores desde pin 5V del ESP32

---

## Mejoras de Hardware Opcionales

### 1. Finales de Carrera
```
GPIO 25 ──┬─── [Switch] ─── GND  (Límite izquierdo)
GPIO 26 ──┴─── [Switch] ─── GND  (Límite derecho)
```

### 2. Display OLED
```
GPIO 21 ─── SDA
GPIO 22 ─── SCL
```

### 3. Encoder Rotativo
```
GPIO 32 ─── CLK
GPIO 33 ─── DT
GPIO 34 ─── SW (Button)
```

### 4. Botones Físicos
```
GPIO 35 ─── [Button] ─── GND  (Start/Stop)
GPIO 36 ─── [Button] ─── GND  (Emergency Stop)
```

---

**¡Sistema completo y listo para usar! 🚀**
