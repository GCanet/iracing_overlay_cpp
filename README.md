# 🏎️ iRacing Overlay

## 🎯 Características

### Widgets Implementados

#### 1. **Relative** (Posiciones relativas)
- **Sistema inteligente de visualización:**
  - Si vas P1-P3: Muestra todos los disponibles (tú + hasta 8 detrás)
  - Si vas P4+: Muestra 4 delante + tú + 4 detrás (centrado)
  - Si vas en las últimas: Ajusta automáticamente
- **Header con información de carrera:**
  - Nombre de la serie (desde SessionInfo YAML)
  - Vueltas completadas/Total o tiempo restante
  - SOF (Strength of Field) calculado en tiempo real
- **Por cada piloto muestra:**
  - Posición
  - Número de coche + Nombre real (desde SessionInfo)
  - Safety Rating con color según licencia:
    - R (rojo) = 0.0-0.99
    - D (naranja) = 1.0-1.99
    - C (amarillo) = 2.0-2.99
    - B (verde) = 3.0-3.99
    - A (azul) = 4.0+
  - iRating real (desde SessionInfo)
  - Proyección de iRating (+/- en verde/rojo)
  - Logo de marca de coche (BMW, Mercedes, Audi, Porsche, Ferrari, Lamborghini, Aston Martin, McLaren, Ford, Chevrolet, Toyota, Mazda)
  - Tiempo de última vuelta
  - Gap de distancia relativo al jugador
- **La fila del jugador se destaca con fondo verde**
- **Pilotos en pits aparecen griseados**
- **Transparencia del 60% en el fondo**

#### 2. **Telemetría**
- Gráficos de histórico horizontal (3 segundos)
- Throttle (verde)
- Brake (rojo)
- Actualización en tiempo real a 60Hz

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

### 1. Clonar el repositorio

```bash
git clone https://github.com/tu-usuario/iracing_overlay_cpp.git
cd iracing_overlay_cpp
```

### 2. Descargar dependencias

```bash
cd external

# GLFW
git clone --depth 1 --branch 3.3.8 https://github.com/glfw/glfw.git

# ImGui
git clone --depth 1 --branch v1.90.1 https://github.com/ocornut/imgui.git
```

### 3. Compilar

#### Windows (Visual Studio):
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

#### Windows (MinGW):
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
```

O simplemente ejecuta:
```bash
build.bat
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
4. El overlay se conectará automáticamente y mostrará datos reales

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
│   │   ├── relative_calc.*   # Cálculos relativo + parsing
│   │   └── irating_calc.*    # Proyección iRating
│   └── utils/
│       ├── config.*          # Configuración (placeholder)
│       └── yaml_parser.*     # Parser SessionInfo ✨ NUEVO
├── include/
│   └── irsdk/
│       └── irsdk_defines.h   # Headers iRacing SDK
├── external/                 # Dependencias
│   ├── imgui/
│   ├── glfw/
│   └── glad/
├── assets/                   # Assets opcionales
│   └── car_brands/           # Logos PNG
├── .gitignore                # ✨ NUEVO
├── CMakeLists.txt            # Build system actualizado
├── build.bat                 # Script compilación
└── README.md                 # Esta documentación
```

## 🔧 Dependencias

- **Dear ImGui** 1.90.1 - UI framework
- **GLFW** 3.3.8 - Window management
- **GLAD** - OpenGL loader
- **iRacing SDK** - Official telemetry API

## 🎨 Assets Opcionales

### Logos de Marcas de Coches

Para mostrar los logos de las marcas, coloca las imágenes PNG (128x128 recomendado) en:

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
├── chevrolet.png
├── toyota.png          
└── mazda.png           
```

## 🚀 Roadmap

- [ ] Cargar texturas de logos de PNG
- [ ] Modo "ghost" (click-through)

## ⚠️ Disclaimer

Este proyecto usa únicamente la API oficial de iRacing SDK.
No modifica archivos del juego ni usa memory injection.
**100% permitido** según términos de servicio de iRacing.

## 📜 Licencia

MIT License - Libre para uso personal y comercial
