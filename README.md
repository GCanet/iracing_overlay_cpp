# 🏎️ iRacing Overlay

Overlay profesional de alto rendimiento para iRacing con consumo mínimo de recursos.

## ✨ Características

- **Relativo avanzado**: Muestra pilotos cercanos con gaps en tiempo real
- **Telemetría**: Gráficos de throttle/brake tipo osciloscopio
- **Proyección iRating**: Calcula cambio estimado de iRating
- **Ultra-ligero**: <20 MB RAM, <1% CPU
- **100% Legal**: Solo usa iRacing SDK oficial
- **Transparente**: Overlay con ventana click-through

## 🎯 Requisitos

### Software
- **Windows 10/11** (64-bit)
- **iRacing** instalado y corriendo
- **Visual Studio 2019+** o **MinGW-w64** (para compilar)
- **CMake 3.15+**
- **Git**

### Hardware
- GPU con soporte OpenGL 3.3+
- 50 MB espacio libre

## 📦 Instalación

### Opción 1: Ejecutable Pre-compilado (Próximamente)

1. Descarga `iRacingOverlay.zip`
2. Extrae en cualquier carpeta
3. Ejecuta `iRacingOverlay.exe`

### Opción 2: Compilar desde código

#### 1. Clonar dependencias

```bash
cd iracing_overlay_cpp

# Clonar GLFW
git clone --depth 1 --branch 3.3.8 https://github.com/glfw/glfw.git external/glfw

# Clonar ImGui
git clone --depth 1 --branch v1.90.1 https://github.com/ocornut/imgui.git external/imgui

# Descargar GLAD
# Ve a: https://glad.dav1d.de/
# Profile: Core
# API gl: Version 3.3
# Generate, descarga y extrae en external/glad/
```

#### 2. Compilar con CMake

**Windows (Visual Studio):**

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**Windows (MinGW):**

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

#### 3. Ejecutar

```bash
cd build/bin
./iRacingOverlay.exe
```

## 🚀 Uso

1. **Inicia el overlay** primero
2. **Abre iRacing** y entra a una sesión
3. El overlay se conectará automáticamente

### Controles

- **ESC**: Toggle demo window (para testear)
- **Q**: Salir
- **Click derecho** en widgets: Configuración
- **Drag**: Mover ventanas

## 📊 Widgets Disponibles

### Relativo
- Posiciones arriba/abajo del jugador
- Gap en tiempo real
- iRating de cada piloto
- Última vuelta
- Indicador de pits

### Telemetría
- Gráfico throttle (verde)
- Gráfico brake (rojo)
- Velocidad actual
- Historial últimos 3 segundos

## ⚙️ Configuración

### Personalizar Rango Relativo

Click derecho en ventana "RELATIVE":
- **Ahead**: Coches por delante (1-10)
- **Behind**: Coches por detrás (1-10)
- **Show iRating**: Toggle mostrar iRating

### Posición de Ventanas

Las ventanas se pueden arrastrar libremente.
Posiciones se guardan automáticamente.

## 🛠️ Estructura del Proyecto

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
- **iRacing SDK** - Official telemetry API

## 📝 Roadmap

### v1.1 (Próximo)
- [ ] Sistema de skins (Trading Paints style)
- [ ] Fuel calculator
- [ ] Damage indicator
- [ ] Configuración GUI completa

### v1.2
- [ ] Track map
- [ ] Delta timing
- [ ] Stint tracker
- [ ] Web API integration (iRating real)

### v2.0
- [ ] Multi-monitor support
- [ ] Themes personalizables
- [ ] Plugin system
- [ ] Cloud sync de configuración

## 🐛 Troubleshooting

### "No se conecta a iRacing"
- ✅ Verifica que iRacing esté corriendo
- ✅ Entra a una sesión (no funciona en menús)
- ✅ Reinicia ambos programas

### "Ventanas no se ven"
- ✅ Presiona ESC para ver demo window
- ✅ Verifica que iRacing esté en ventana borderless
- ✅ Comprueba drivers GPU actualizados

### "Performance bajo"
- ✅ Cierra otros overlays
- ✅ Reduce historial telemetría
- ✅ Desactiva widgets no usados

## 🤝 Contribuir

1. Fork el proyecto
2. Crea feature branch (`git checkout -b feature/amazing`)
3. Commit cambios (`git commit -m 'Add feature'`)
4. Push a branch (`git push origin feature/amazing`)
5. Abre Pull Request

## 📜 Licencia

MIT License - Libre para uso personal y comercial

## ⚠️ Disclaimer

Este proyecto usa únicamente la API oficial de iRacing SDK.
No modifica archivos del juego ni usa memory injection.
100% permitido según términos de servicio de iRacing.

## 🙏 Créditos

- iRacing SDK - Datos telemetría
- Dear ImGui - UI framework
- GLFW - Window management
- Comunidad iRacing

## 📞 Soporte

- **Issues**: GitHub Issues
- **Discord**: [Link a servidor]
- **Email**: support@example.com

---

**Hecho con ❤️ para la comunidad iRacing**

🏁 Happy Racing! 🏁
