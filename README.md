# 🏎️ iRacing Overlay

Overlay profesional de alto rendimiento para iRacing con consumo mínimo de recursos.

## 🎯 Características

### Widgets Implementados

#### 1. **Relative** (Posiciones relativas)
- Muestra 4 coches por delante y 4 por detrás del jugador
- Header con información de carrera:
  - Nombre de la serie
  - Vueltas completadas / Total o tiempo restante
  - SOF (Strength of Field)
- Por cada piloto muestra:
  - Posición
  - Número de coche + Nombre (griseado si está en pits)
  - Safety Rating (con color según licencia: R/D/C/B/A)
  - iRating
  - Proyección de iRating (+/- en verde/rojo)
  - Logo de marca de coche (si existe el asset)
  - Tiempo de última vuelta
  - Gap de distancia relativo al jugador
- La fila del jugador se destaca con fondo verde
- Transparencia del 60% en el fondo de la ventana

#### 2. **Telemetría**
- Gráficos de histórico horizontal (3 segundos)
- Throttle (verde)
- Brake (rojo)
- Actualización en tiempo real

## 🛠️ Requisitos

### Software
- **Windows 10/11** (64-bit)
- **iRacing** instalado y corriendo
- **Visual Studio 2019+** o **MinGW-w64** (para compilar)
- **CMake 3.15+**
- **Git**

### Controles

- **Q**: Salir
- **Drag**: Mover ventanas

## 📦 Compilación

### 1. Descargar dependencias

```bash
cd external

# GLFW
git clone --depth 1 --branch 3.3.8 https://github.com/glfw/glfw.git

# ImGui
git clone --depth 1 --branch v1.90.1 https://github.com/ocornut/imgui.git
```

### 2. Descargar GLAD

1. Ve a: https://glad.dav1d.de/
2. Configuración:
   - Profile: **Core**
   - API gl: **Version 3.3**
3. Click **GENERATE**
4. Descarga y extrae en `external/glad/`

### 3. Compilar

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### 4. Ejecutar

```bash
cd build\bin\Release
iRacingOverlay.exe
```

## 🎮 Uso

1. **Ejecuta el overlay primero**
2. **Abre iRacing**
3. **Entra a una sesión** (práctica, carrera, etc.)
4. El overlay se conectará automáticamente

## 📊 Actualización de datos

- **Telemetría**: Actualización en tiempo real (60 Hz)
- **Relative**: Actualización cada vez que iRacing envía nuevos datos (~60 Hz)

## 🗂️ Estructura del Proyecto

```
iracing_overlay_cpp/
├── src/
│   ├── main.cpp              # Entry point
│   ├── ui/                   # Interfaz
│   │   ├── overlay_window.*  # Ventana principal
│   │   ├── relative_widget.* # Widget relativo
│   │   └── telemetry_widget.*# Widget telemetría
│   ├── data/                 # Lógica datos
│   │   ├── irsdk_manager.*   # Wrapper SDK
│   │   ├── relative_calc.*   # Cálculos relativo
│   │   └── irating_calc.*    # Proyección iRating
│   └── utils/
│       └── config.*          # Configuración
├── include/
│   └── irsdk/                # Headers iRacing SDK
│       └── irsdk_defines.h   # Definiciones SDK
├── external/                 # Dependencias
│   ├── imgui/
│   ├── glfw/
│   └── glad/
└── CMakeLists.txt
```

## 🔧 Dependencias

- **Dear ImGui** 1.90.1 - UI framework
- **GLFW** 3.3.8 - Window management
- **GLAD** - OpenGL loader
- **iRacing SDK** - Official telemetry API (solo requiere irsdk_defines.h)

## 🎨 Assets Opcionales

### Logos de Marcas de Coches
Si quieres mostrar los logos de las marcas de coches, coloca las imágenes en:
```
assets/car_brands/
├── bmw.png
├── mercedes.png
├── audi.png
├── porsche.png
├── ferrari.png
├── lamborghini.png
├── aston_martin.png
├── mclaren.png
├── ford.png
└── chevrolet.png
```

**Nota**: Si no existen los assets, el overlay funcionará igual pero no mostrará el logo (espacio vacío).

## 📝 Notas sobre iRacing SDK

El archivo `include/irsdk/irsdk_defines.h` contiene todas las definiciones necesarias para comunicarse con iRacing:
- Estructuras de datos (header, buffers)
- Nombres de memoria compartida
- Tipos de variables
- Constantes

No se necesita ningún otro archivo del SDK oficial de iRacing.

## ⚠️ Disclaimer

Este proyecto usa únicamente la API oficial de iRacing SDK.
No modifica archivos del juego ni usa memory injection.
100% permitido según términos de servicio de iRacing.

## 📜 Licencia

MIT License - Libre para uso personal y comercial
