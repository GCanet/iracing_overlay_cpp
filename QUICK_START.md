# 🚀 Quick Start Guide

## Installation & First Run

### 1. Build the Overlay
```bash
# From project root
build.bat
```

### 2. First Launch
```bash
cd build\bin\Release
iRacingOverlay.exe
```

You'll see:
```
✅ Overlay initialized

Controls:
  Q      - Quit
  L      - Toggle Lock (click-through mode)
  Drag   - Move widgets (when unlocked)

⏳ Waiting for iRacing...
```

### 3. Start iRacing
- Launch iRacing normally
- Enter any session (Practice, Qualifying, Race)
- Overlay auto-connects and shows live data

---

## 🎯 Setting Up Your Layout

### Initial Positioning (First Time)

1. **Overlay starts UNLOCKED** (you can click and drag)
2. **Drag widgets** to your preferred screen positions:
   - **Relative widget** → Typically top-right
   - **Telemetry widget** → Typically bottom-right
3. **Press L** to lock (enable click-through)
4. **Verify** you see "🔒 LOCKED" indicator
5. **Test** clicking through to iRacing

Your layout is auto-saved when you quit!

---

## 🔒 Lock vs Unlock

| State | Icon | Mouse Behavior | When to Use |
|-------|------|----------------|-------------|
| **Unlocked** | 🔓 | Click widgets, drag to move | Setting up layout |
| **Locked** | 🔒 | Mouse passes through to game | Racing |

**Toggle with L key**

---

## ⚙️ Configuration

### config.ini Location
```
build/bin/Release/config.ini
```

### Common Tweaks

```ini
[Global]
FontScale=1.2          # Make text bigger
GlobalAlpha=0.5        # More transparent

[Relative]
Alpha=0.8              # Less transparent for this widget

[Telemetry]
Visible=false          # Hide telemetry widget
```

---

## 🎨 Customization Tips

### Transparency
- **0.0** = Invisible
- **0.5** = Very transparent
- **0.7** = Good balance (default)
- **1.0** = Fully opaque

### Font Size
- **0.8** = Smaller
- **1.0** = Normal (default)
- **1.2** = Larger
- **1.5** = Very large

### Reset to Defaults
Delete `config.ini` and restart the overlay.

---

## 🏁 Racing Workflow

### Before Race
1. Start overlay (auto-loads saved layout)
2. Press **L** to lock if not already locked
3. Start iRacing

### During Race
- Overlay shows live data
- Mouse clicks go to game (if locked)
- **Q** to quit if needed

### After Race
- Just close iRacing normally
- Overlay auto-saves config on exit

---

## 🐛 Troubleshooting

### "Overlay blocks my clicks in-game"
→ Press **L** to lock (enable click-through)

### "I can't move widgets"
→ Press **L** to unlock

### "Widgets reset position every time"
→ Make sure you're not deleting `config.ini`

### "Can't see overlay over iRacing"
→ Start overlay BEFORE iRacing (or restart overlay)

---

## 💡 Pro Tips

1. **Dual Monitors**: Put overlay on secondary monitor for more screen space
2. **Start Locked**: Set `ClickThrough=true` in config.ini to start locked
3. **Minimal HUD**: Hide telemetry if you only want relative positions
4. **Test in Practice**: Set up layout in practice session before racing

---

## 📋 Default Positions

| Widget | Default Location | Size |
|--------|-----------------|------|
| Relative | Top-right (520px from right, 20px from top) | 500x auto |
| Telemetry | Bottom-right (340px from right, 180px from bottom) | 320x160 |

These are only used on first run. After that, uses saved positions.

---

## 🎯 Next Steps

1. ✅ Get comfortable with Lock/Unlock (L key)
2. ✅ Position widgets where you like them
3. ✅ Adjust transparency in `config.ini` if needed
4. ✅ Race! 🏎️

**Enjoy your racing! 🏁**
