# Assets Directory Structure

## Car Brand Logos

Place your car brand logos in this directory.

Recommended specs:
- Format: PNG with transparency
- Size: 128x128 pixels (or 64x64)
- Background: Transparent
- Style: White/monochrome logos work best

### Required files:

```
assets/car_brands/
├── aston_martin.png
├── audi.png
├── bmw.png
├── chevrolet.png
├── ferrari.png
├── ford.png
├── lamborghini.png
├── mazda.png          # NEW
├── mclaren.png
├── mercedes.png
├── porsche.png
└── toyota.png         # NEW
```

### Where to get logos:

1. **Official brand websites** (check copyright/usage rights)
2. **Open source icon packs** (e.g., Simple Icons)
3. **Create your own** simple logos
4. **iRacing community resources**

### Note:

If a logo file doesn't exist, the overlay will display `[brand]` as text instead.
The overlay will still work perfectly without any logos.

### How they're used:

The overlay detects car brands from the `CarPath` string in SessionInfo.
For example:
- "bmwm4gt3" → matches "bmw" → loads `bmw.png`
- "porsche992gt3cup" → matches "porsche" → loads `porsche.png`
- "mazdaroadstermx5cup" → matches "mazda" → loads `mazda.png`

### Testing without logos:

The overlay shows brand names in brackets: `[bmw]`, `[porsche]`, etc.
This lets you verify the brand detection is working before adding actual logos.
