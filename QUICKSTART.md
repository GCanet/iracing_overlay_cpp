# 🚀 Quick Start Guide

Guía paso a paso para compilar y ejecutar el overlay en **5 minutos**.

## ✅ Pre-requisitos

1. **Windows 10/11** (64-bit)
2. **Visual Studio 2019+** (Community Edition gratis)
3. **Git** instalado
4. **CMake** instalado

## 📦 Paso 1: Descargar el proyecto

```bash
git clone https://github.com/tu-usuario/iracing_overlay_cpp.git
cd iracing_overlay_cpp
```

## 🔧 Paso 2: Setup dependencias

### Opción A: Script automático (fácil)

```bash
setup_deps.bat
```

Esto descarga GLFW e ImGui automáticamente.

### Opción B: Manual

```bash
cd external

# GLFW
git clone --depth 1 --branch 3.3.8 https://github.com/glfw/glfw.git

# ImGui
git clone --depth 1 --branch v1.90.1 https://github.com/ocornut/imgui.git
```

### Descargar GLAD (requerido)

1. Ve a: https://glad.dav1d.de/
2. Configuración:
   - Profile: **Core**
   - API gl: **Version 3.3**
3. Click **GENERATE**
4. Descarga y extrae en `external/glad/`

Estructura final:
```
external/
├── glfw/
├── imgui/
└── glad/
    ├── include/
    └── src/
```

## 🏗️ Paso 3: Compilar

### Opción A: Script automático

```bash
build.bat
```

### Opción B: Manual

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

## ▶️ Paso 4: Ejecutar

```bash
cd build\bin\Release
iRacingOverlay.exe
```

## 🎮 Paso 5: Usar con iRacing

1. **Ejecuta el overlay primero**
2. **Abre iRacing**
3. **Entra a una sesión** (práctica, carrera, etc.)
4. El overlay se conectará automáticamente

## 🎨 Personalización

- **Click derecho** en ventanas para configurar
- **ESC** para toggle demo window
- **Q** para salir

## ❓ Problemas comunes

### "CMake not found"
```bash
# Descarga de: https://cmake.org/download/
# Agrega al PATH
```

### "Visual Studio not found"
```bash
# Descarga VS Community: https://visualstudio.microsoft.com/
# Durante instalación, selecciona "Desktop development with C++"
```

### "GLAD no encontrado"
```bash
# Lee: external/GLAD_INSTRUCTIONS.md
# O descarga de: https://glad.dav1d.de/
```

### "No se conecta a iRacing"
- ✅ iRacing debe estar corriendo
- ✅ Debes estar en una sesión activa
- ✅ No funciona en menús principales

## 🚀 Siguiente paso

Lee el [README.md](README.md) completo para más detalles.

---

**¿Algo no funciona?** Abre un Issue en GitHub
