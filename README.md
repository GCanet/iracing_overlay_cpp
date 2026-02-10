# 🏎️ iRacing Overlay

**A professional, transparent overlay for iRacing with real-time telemetry and race position tracking.**

---

## 🎯 Features

### Widgets Implemented

#### 1. **Relative** (Posiciones relativas)
- **Smart visualization system:**
  - If P1-P3: Shows you + up to 8 behind
  - If P4+: Shows 4 ahead + you + 4 behind (centered)
  - If in last positions: Auto-adjusts intelligently
- **Race header info:**
  - Series name (from SessionInfo YAML)
  - Completed laps/Total or time remaining
  - SOF (Strength of Field) calculated in real-time
- **Per driver shows:**
  - Position
  - Car number + Real name (from SessionInfo)
  - Safety Rating with license-based color:
    - R (red) = 0.0-0.99
    - D (orange) = 1.0-1.99
    - C (yellow) = 2.0-2.99
    - B (green) = 3.0-3.99
    - A (blue) = 4.0+
  - Real iRating (from SessionInfo)
  - iRating projection (+/- in green/red)
  - Car brand logo (BMW, Mercedes, Audi, Porsche, Ferrari, Lamborghini, Aston Martin, McLaren, Ford, Chevrolet, Toyota, Mazda)
  - Last lap time
  - Gap relative to player
- **Player row highlighted with green background**
- **Drivers in pits shown grayed out**
- **Configurable transparency**

#### 2. **Telemetry**
- Horizontal history graphs (3 seconds)
- Throttle (green)
- Brake (red)
- Real-time updates at 60Hz
- Optimized rendering (single draw list)

---

## 🛠️ Requirements

### Software
- **Windows 10/11** (64-bit)
- **iRacing** installed and running
- **Visual Studio 2019+** or **MinGW-w64** (to compile)
- **CMake 3.15+**
- **Git**

---

## 🎮 Controls

| Key | Action |
|-----|--------|
| **Q** | Quit overlay |
| **L** | Toggle Lock (enable/disable click-through) |
| **Drag** | Move widgets (when unlocked) |

---

## 📦 Compilation

### 1. Clone repository

```bash
git clone https://github.com/your-user/iracing_overlay_cpp.git
cd iracing_overlay_cpp
```

### 2. Compile

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

Or simply run:
```bash
build.bat
```

### 3. Run

```bash
cd build\bin\Release
iRacingOverlay.exe
```

---

## 🎮 Usage

1. **Run the overlay first**
2. **Open iRacing**
3. **Enter a session** (practice, race, etc.)
4. The overlay connects automatically and displays real data

### First Time Setup

1. Start the overlay (unlocked by default)
2. Drag widgets to your preferred positions
3. Press **L** to lock (enable click-through)
4. Positions auto-save when you quit

---

## 🗂️ Project Structure

```
iracing_overlay_cpp/
├── src/
│   ├── main.cpp              # Entry point
│   ├── ui/                   # Interface
│   │   ├── overlay_window.*  # Main window + click-through
│   │   ├── relative_widget.* # Relative widget
│   │   └── telemetry_widget.*# Telemetry widget
│   ├── data/                 # Data logic
│   │   ├── irsdk_manager.*   # SDK wrapper
│   │   ├── relative_calc.*   # Relative calculations + parsing
│   │   └── irating_calc.*    # iRating projection
│   └── utils/
│       ├── config.*          # INI config system
│       └── yaml_parser.*     # SessionInfo parser
├── include/
│   └── irsdk/
│       └── irsdk_defines.h   # iRacing SDK headers
├── external/                 # Dependencies (not in git)
│   ├── imgui/
│   ├── glfw/
│   └── glad/
├── assets/                   # Optional assets
│   └── car_brands/           # PNG logos
├── .gitignore
├── CMakeLists.txt            # Build system
├── build.bat                 # Build script
├── config.ini.example        # Example config
└── README.md                 # This file
```

---

## 🔧 Dependencies

- **Dear ImGui** 1.90.1 - UI framework
- **GLFW** 3.3.8 - Window management (with native Win32 access)
- **GLAD** - OpenGL loader
- **iRacing SDK** - Official telemetry API

---

## 🎨 Optional Assets

### Car Brand Logos

To display brand logos, place PNG images (128x128 recommended) in:

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

---

## 🐛 Troubleshooting

### Overlay not showing above iRacing
- Make sure you run the overlay BEFORE starting iRacing
- Press `L` to ensure it's locked (always-on-top)

### Widgets not saving positions
- Make sure the overlay has write permissions in its directory
- Check that `config.ini` is being created on exit

### Click-through not working
- Only works on Windows
- Make sure you pressed `L` to lock
- You should see "🔒 LOCKED" indicator

---

## ⚠️ Disclaimer

This project uses only the official iRacing SDK API.
Does not modify game files or use memory injection.
**100% allowed** according to iRacing terms of service.


---

## 📜 License

MIT License - Free for personal and commercial use

---

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repo
2. Create a feature branch
3. Submit a pull request

---

## 🙏 Credits

- iRacing SDK by iRacing.com
- Dear ImGui by Omar Cornut
- GLFW by Marcus Geelnard & Camilla Löwy
