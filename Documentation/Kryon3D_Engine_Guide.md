# Kryon3D Engine - Hardware-Accelerated 3D Graphics Guide

Welcome to the **Kryon3D Engine Guide** for KryonOS. Kryon3D is a native C++ 3D rasterization and transformation engine exposed to the JavaScript runtime. It delivers high-framerate, flicker-free, double-buffered 3D graphics for microcontrollers.

---

## 1. Hardware Architecture & PSRAM Memory Notice

### ⚠️ Critical Memory & Hardware Notice
Rendering 3D scenes with double-buffering requires allocating an off-screen frame buffer in RAM:
- **Full-Screen 240x320 16-bit RGB565 Buffer:** `~153.6 KB RAM`
- **Standard Game Viewport 240x210 16-bit Buffer:** `~100.8 KB RAM`
- **Compact Viewport 160x120 16-bit Buffer:** `~38.4 KB RAM`

> [!IMPORTANT]
> **PSRAM Recommended**: For full 16-bit high-resolution 3D gaming (e.g., 240x320 or 240x210), it is **strongly recommended to run on the ESP32-S3 DevKitC-1 N16R8** or boards equipped with 8MB/2MB Octal/Quad PSRAM.
>
> **Standard ESP32 (Non-PSRAM)**: If running on a board without external PSRAM (e.g. `esp32doit-devkit-v1`), use compact viewport sizes such as `Kryon3D.begin(160, 120, 16)` or 8-bit color depth `Kryon3D.begin(240, 160, 8)` to stay safely within internal SRAM boundaries.

---

## 2. Coordinate System & Camera Model

Kryon3D uses a standard **Right-Handed 3D World Coordinate System**:
- **+X**: Right
- **+Y**: Up
- **+Z**: Towards the viewer (out of screen)
- **-Z**: Forward (into the screen / away from camera)

Euler rotations $(\theta_x, \theta_y, \theta_z)$ are specified in radians.

---

## 3. Core API Reference

The engine is accessible globally via `Kryon3D` (or `System.graphics3d`).

### 3.1 Lifecycle & Double-Buffering (Flicker-Free)

#### `Kryon3D.begin(width, height, colorDepth)`
- **Parameters:**
  - `width` (Integer): Viewport width in pixels (e.g. `240` or `160`).
  - `height` (Integer): Viewport height in pixels (e.g. `210` or `120`).
  - `colorDepth` *(optional, default 16)*: `16` (RGB565) or `8` (8-bit color).
- **Returns:** `Boolean` (`true` if buffer allocation succeeded).
- **Description:** Allocates an off-screen double buffer in PSRAM / SRAM.

#### `Kryon3D.clear(color)`
- **Parameters:** `color` (16-bit RGB565 integer, default `0x0000`).
- **Description:** Clears the 3D buffer to the specified background color.

#### `Kryon3D.directDraw(enabled)`
- **Parameters:** `enabled` (Boolean).
- **Description:** Controls whether standard 2D `System` drawing commands write directly to the physical display glass (`true`) or into the off-screen 3D sprite buffer (`false`, default). Ideal for painting static HUDs or on-screen touch controls once directly to the glass.

#### `Kryon3D.render(destX, destY)` or `Kryon3D.flush(destX, destY)`
- **Parameters:**
  - `destX` *(optional, default 0)*: X screen position on display glass.
  - `destY` *(optional, default 0)*: Y screen position on display glass.
- **Description:** Flushes the entire off-screen 3D framebuffer directly to the TFT display glass via high-speed SPI/DMA in a single burst. **Guarantees zero screen tearing and zero flickering.**

#### `Kryon3D.end()`
- **Description:** Frees the allocated 3D frame buffer memory.

---

### 3.2 Camera, Viewport, Lighting & Fog

#### `Kryon3D.setViewport(x, y, width, height)`
- **Parameters:**
  - `x, y` (Integers): Top-left coordinate offset within the double buffer.
  - `width, height` (Integers): Viewport resolution for 3D projection calculations.
- **Description:** Configures a sub-rect region of the double buffer for 3D rendering (e.g. `setViewport(0, 0, 240, 210)` for a top 3D window with a 2D HUD below it in a full 240x320 buffer). Defaults to full buffer dimensions on `begin()`.

#### `Kryon3D.setCamera(posX, posY, posZ, targetX, targetY, targetZ, fov)`
- **Parameters:**
  - `posX, posY, posZ` (Floats): Camera position in 3D world space.
  - `targetX, targetY, targetZ` (Floats): Point the camera is looking at.
  - `fov` *(optional, default 60.0)*: Vertical Field of View in degrees.

#### `Kryon3D.setLight(dirX, dirY, dirZ, ambient, diffuse)`
- **Parameters:**
  - `dirX, dirY, dirZ` (Floats): Direction vector pointing towards the light source.
  - `ambient` *(optional, default 0.3)*: Ambient light coefficient (`0.0` to `1.0`).
  - `diffuse` *(optional, default 0.7)*: Diffuse directional lighting coefficient (`0.0` to `1.0`).

#### `Kryon3D.setFog(enabled, fogColor, nearDist, farDist)`
- **Parameters:**
  - `enabled` (Boolean): Enable/disable distance fog.
  - `fogColor` (16-bit color): Color to blend with distance.
  - `nearDist`, `farDist` (Floats): Distance range where fog blends from 0% to 100%.

---

### 3.3 3D Geometry & Rasterization Primitives

#### `Kryon3D.drawLine(x0, y0, z0, x1, y1, z1, color)`
- **Description:** Renders a 3D line connecting two world space coordinates. Automatically clips if behind camera.

#### `Kryon3D.drawTriangle(x0,y0,z0, x1,y1,z1, x2,y2,z2, color)`
- **Description:** Draws a 3D wireframe triangle.

#### `Kryon3D.fillTriangle(x0,y0,z0, x1,y1,z1, x2,y2,z2, color, enableLighting)`
- **Parameters:**
  - `x0..z2` (Floats): 3D coordinates for vertex 0, 1, and 2.
  - `color` (16-bit integer): Base surface color.
  - `enableLighting` *(optional, default true)*: When `true`, automatically calculates face normal, performs **Backface Culling** (discards triangles facing away from camera), and applies directional flat shading.

#### `Kryon3D.drawCube(x, y, z, sizeX, sizeY, sizeZ, rotX, rotY, rotZ, color, enableLighting)`
- **Description:** High-speed hardware primitive rendering a solid shaded 3D box with individual Euler rotation angles $(\theta_x, \theta_y, \theta_z)$ in radians.

#### `Kryon3D.drawBillboard(x, y, z, width, height, color)`
- **Description:** Renders a camera-facing 2D billboard sprite in 3D world space, automatically scaling width and height with camera distance.

---

### 3.4 High-Performance Mesh Batch Rendering

#### `Kryon3D.drawMesh(verticesArray, facesArray, modelMatrix, baseColor, enableLighting)`
- **Parameters:**
  - `verticesArray`: Flat array of vertex floats `[x0, y0, z0, x1, y1, z1, ...]`.
  - `facesArray`: Array of triangle face indices `[[i0, i1, i2, (optionalColor)], ...]`.
  - `modelMatrix` *(optional)*: 16-element 4x4 Transformation Matrix (calculated via `FastMath.mat4Rotate` or `FastMath.mat4Multiply`).
  - `baseColor` *(optional, default 0xFFFF)*: Fallback color if face doesn't specify one.
  - `enableLighting` *(optional, default true)*: Enables backface culling and directional shading.
- **Description:** Offloads an entire 3D model (spaceships, characters, terrain, objects) to native C++. In a single function call, all vertices are transformed, culled, shaded, and rasterized to the off-screen buffer at maximum hardware speed.

### 3.5 Composite 2D + 3D Rendering (Zero-Flicker HUD, UI & Weapons)

When `Kryon3D` is active, **all standard 2D `System` graphics primitives** automatically draw directly into the active off-screen double buffer:
- `System.fillRect(x, y, w, h, color)`
- `System.drawRect(x, y, w, h, color)`
- `System.drawLine(x0, y0, x1, y1, color)`
- `System.drawCircle(x, y, r, color)` / `System.fillCircle(x, y, r, color)`
- `System.drawTriangle(x0, y0, ...)` / `System.fillTriangle(x0, y0, ...)`
- `System.drawRoundRect(x, y, ...)` / `System.fillRoundRect(x, y, ...)`
- `System.drawString(text, x, y, font)`
- `System.drawPixel(x, y, color)`

This allows you to render 3D scenes first (e.g. terrain, 3D meshes, demons), then draw 2D game HUD elements, animated weapons, crosshairs, and radar directly on top in the same buffer before calling `Kryon3D.render(0, 0)`.

```text
┌────────────────────────────────────────────────────────┐
│                   Off-Screen PSRAM Framebuffer          │
│  1. Kryon3D Scene (Cubes, Meshes, Shaded Triangles)   │
│  2. 2D Weapon & Crosshair (System.fillTriangle, ...)   │
│  3. 2D HUD & Status Text (System.drawString, ...)      │
│  4. 2D Radar & Minimap (System.fillRect, ...)          │
└───────────────────────────┬────────────────────────────┘
                            │ Single DMA SPI Burst (Kryon3D.render)
                            ▼
               ┌─────────────────────────┐
               │    TFT Display Glass    │  ==> 100% Zero Flickering!
               └─────────────────────────┘
```

---

## 4. Complete Code Example: Spinning Shaded 3D Cube with 2D HUD

```javascript
// Initialize 3D Viewport in PSRAM (240x320, 16-bit color)
if (!Kryon3D.begin(240, 320, 16)) {
    System.print("Failed to allocate 3D buffer. Low RAM!");
}

// Setup Camera and Directional Light
Kryon3D.setCamera(0, 2.5, -4.5, 0, 0, 0, 60);
Kryon3D.setLight(0.5, 1.0, -0.7, 0.25, 0.75);

var angle = 0;

while (true) {
    // 1. Clear off-screen 3D buffer
    Kryon3D.clear(0x0841); // Dark navy background

    // 2. Draw Ground Grid Lines
    for (var x = -5; x <= 5; x += 1) {
        Kryon3D.drawLine(x, -1.0, -5, x, -1.0, 5, 0x39E7);
        Kryon3D.drawLine(-5, -1.0, x, 5, -1.0, x, 0x39E7);
    }

    // 3. Draw Rotating Shaded 3D Cube with Hardware Lighting
    angle += 0.04;
    Kryon3D.drawCube(0, 0.5, 0, 1.5, 1.5, 1.5, angle, angle * 0.7, 0, 0xFD20, true);

    // 4. Draw Floating 3D Billboard
    Kryon3D.drawBillboard(2.0, 1.2, 0.5, 0.6, 0.6, 0xF800);

    // 5. Draw 2D HUD / Status Bar directly into buffer
    System.fillRect(0, 280, 240, 40, 0x0000);
    System.setTextColor(0xFFFF, 0x0000);
    System.drawString("KRYON 3D HARDWARE ENGINE", 10, 290, 2);

    // 6. Push the entire composite frame to display glass (Zero Flickering!)
    Kryon3D.render(0, 0);

    // Delay for OS Garbage Collection & 60FPS sync
    System.delay(10);
}
```

---

## 5. Performance Tips for 3D Game Developers
1. **Always use Backface Culling (`enableLighting = true`)**: Culling triangles that face away from the camera doubles rendering speed.
2. **Batch Draw Meshes with `Kryon3D.drawMesh()`**: Instead of calling `fillTriangle` individually in a JS loop, pass arrays to `drawMesh` so the C++ Kernel can loop with zero interpreter overhead.
3. **Use FastMath for Game Logic**: Combine `Kryon3D` with `FastMath` for player physics, ray-casts, collision detection, and matrix transformations.
4. **Layer 2D on top of 3D**: Draw all 2D HUD elements and controls after 3D geometry and before calling `Kryon3D.render(0, 0)` for completely tear-free, unified rendering.
5. **Frame Pacing (Eliminate Stuttering)**: Instead of a fixed `System.delay(10)`, dynamically calculate the delay to lock the framerate (e.g. 30 FPS). This eliminates physics and rendering stutter:
   ```javascript
   var frameDuration = System.millis() - now;
   if (frameDuration < 33) {
       System.delay(33 - frameDuration); // Sleep exact time needed for 30 FPS
   } else {
       System.delay(1); // GC yield
   }
   ```
6. **Minimize Viewport Height to Eliminate Tearing**: The smaller the `VIEW_H`, the faster the DMA burst completes, meaning less chance of tearing ( crossing the LCD scanline). Use `160` instead of `210` or `240` if tearing is visible.
