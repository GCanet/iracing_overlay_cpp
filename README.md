# 🏎️ iRacing Overlay

Overlay profesional de alto rendimiento para iRacing con consumo mínimo de recursos.

## 🎯 Requisitos

### Software
- **Windows 10/11** (64-bit)
- **iRacing** instalado y corriendo
- **Visual Studio 2019+** o **MinGW-w64** (para compilar)
- **CMake 3.15+**
- **Git**

### Controles

- **ESC**: Toggle demo window (para testear)
- **Q**: Salir
- **Drag**: Mover ventanas





### TO DO:

1. CAMBIAR EL RELATIVO A QUE MUESTRE BASADO EN 4 COCHES POR DELANTE Y POR DETRAS DEL PILOTO Y NADA DE SHOW IRATING, AHEAD Y BEHIND, NI DE CLICKS DERECHOS EN VENTANA RELATIVE, TIENE QUE ESTAR TODO EN UNA FILA:

posicion | numero piloto + nombre | nacionalidad | logo marca coche | safety rating | irating | proyeccion irating carrera en curso | tiempo ultima vuelta | gap de distancia según posicion del piloto

2. Indicador de pits ## no se necesita indicador de pits

3. Velocidad actual ## no se necesita velocidad actual
   
4. **Click derecho** en widgets: Configuración ## quitar esta funcion






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
- [ ] Añadir crewchief
- [ ] Configuración GUI completa
- [ ] Web API integration (iRating real)

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
