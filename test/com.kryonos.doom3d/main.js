// =============================================================================
// DOOM 3D - Advanced Hardware-Accelerated 3D Engine for KryonOS
// 100% Zero-Flicker Double-Buffered Pipeline with Frame Pacing
// Powered by Kryon3D & FastMath Hardware Acceleration Engine
// =============================================================================

var SW = 240;
var SH = 320;
var VIEW_W = 240;
var VIEW_H = 160;

// Colors (RGB565)
var C_BLACK     = 0x0000;
var C_WHITE     = 0xFFFF;
var C_RED       = 0xF800;
var C_DARKRED   = 0x8000;
var C_GREEN     = 0x07E0;
var C_BLUE      = 0x001F;
var C_YELLOW    = 0xFFE0;
var C_ORANGE    = 0xFD20;
var C_CYAN      = 0x07FF;
var C_GREY      = 0x7BEF;
var C_DARKGREY  = 0x39E7;
var C_FLOOR     = 0x2104;
var C_SKY       = 0x18C3;
var C_WALL1     = 0xA145; // Brown brick
var C_WALL2     = 0x2A19; // Tech blue
var C_WALL3     = 0x9800; // Blood red wall

// World Map (12x12)
var MAP_W = 12;
var MAP_H = 12;
var MAP = [
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 2, 0, 2, 0, 1, 0, 3, 3, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1,
    1, 0, 2, 0, 2, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
    1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 1, 0, 2, 2, 0, 2, 0, 1,
    1, 0, 3, 0, 1, 0, 2, 0, 0, 2, 0, 1,
    1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 2, 0, 0, 2, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
];

// Player State
var posX = 2.5;
var posY = 2.5;
var dirX = 1.0;
var dirY = 0.0;
var playerAngle = 0.0;

var health = 100;
var lastHealth = -1;
var ammo = 40;
var lastAmmo = -1;
var kills = 0;
var lastKills = -1;
var lastFPS = -1;
var score = 0;
var muzzleFlash = 0;
var painFlash = 0;

// Enemies in 3D space
var enemies = [
    { x: 4.5, y: 3.5, hp: 40, alive: 1, rot: 0, type: 1 },
    { x: 9.5, y: 2.5, hp: 40, alive: 1, rot: 0, type: 1 },
    { x: 3.5, y: 8.5, hp: 60, alive: 1, rot: 0, type: 2 },
    { x: 8.5, y: 9.5, hp: 80, alive: 1, rot: 0, type: 2 }
];

// 3D Blood & Spark Particles
var particles = [];

// Initialize 3D Double Buffer in PSRAM (240x210, 16-bit color)
if (!Kryon3D.begin(VIEW_W, VIEW_H, 16)) {
    Kryon3D.begin(VIEW_W, VIEW_H, 8);
}

// Configure 3D Viewport in top 240x210
Kryon3D.setViewport(0, 0, VIEW_W, VIEW_H);

// Setup Directional Lighting
Kryon3D.setLight(0.5, 1.0, -0.7, 0.35, 0.65);

// Render Static UI (HUD Background + Touch Controls) Directly to Screen Glass ONCE
Kryon3D.directDraw(true);
System.fillScreen(C_BLACK);
drawStaticHUD();
drawStaticControls();
Kryon3D.directDraw(false);

// Main Game Loop
var running = true;
var lastFrameTime = System.millis();
var fps = 0;
var frameCount = 0;
var fpsTimer = System.millis();

while (running) {
    var now = System.millis();
    // Calculate raw dt for FPS counter, but use FIXED dt for smooth physics
    var rawDt = (now - lastFrameTime) * 0.001;
    lastFrameTime = now;
    
    // Fixed physics step (20 FPS) entirely eliminates camera rotation wobble/jitter
    var dt = 0.050;

    frameCount++;
    if (now - fpsTimer >= 1000) {
        fps = frameCount;
        frameCount = 0;
        fpsTimer = now;
    }

    // 1. Process Touch Input
    handleTouchControls(dt);

    // 2. Clear 3D Viewport with Sky & Floor (Inside 240x160 Double Buffer)
    System.fillRect(0, 0, VIEW_W, 80, C_SKY);
    System.fillRect(0, 80, VIEW_W, 80, C_FLOOR);

    // 3. Configure 3D Camera Look-At
    var targetX = posX + dirX * 3.0;
    var targetZ = posY + dirY * 3.0;
    Kryon3D.setCamera(posX, 0.5, posY, targetX, 0.5, targetZ, 65.0);

    // 4. Render 3D Ground Grid Floor (into buffer)
    renderFloorGrid();

    // 5. Render 3D Map Wall Blocks with Hardware Lighting & Culling
    render3DWorld();

    // 6. Render 3D Demons / Enemies
    render3DEnemies(dt);

    // 7. Render 3D Particles
    render3DParticles(dt);

    // 8. Render Center Shotgun & Muzzle Flash (inside buffer)
    renderWeaponOverlay();

    // 9. Render Minimap (inside buffer)
    renderMinimap();

    // Pain flash border overlay (inside buffer)
    if (painFlash > 0) {
        painFlash--;
        System.drawRect(0, 0, VIEW_W, VIEW_H, C_RED);
        System.drawRect(1, 1, VIEW_W - 2, VIEW_H - 2, C_RED);
    }

    // 10. FLUSH 240x160 VIEWPORT BUFFER TO DISPLAY IN ONE ATOMIC BURST (ZERO FLICKER)
    Kryon3D.render(0, 0);

    // 11. Differential HUD update (only redraws changing digits directly on glass)
    updateDynamicHUD();

    // Frame pacing sync (Lock to 20 FPS for absolute rock-solid stutter-free performance)
    var frameDuration = System.millis() - now;
    if (frameDuration < 50) {
        System.delay(50 - frameDuration);
    } else {
        System.delay(1);
    }
}

// -----------------------------------------------------------------------------
// Touch Controls Handler
// -----------------------------------------------------------------------------
function handleTouchControls(dt) {
    var touch = System.getTouch();
    if (!touch.touched) return;

    var tx = touch.x;
    var ty = touch.y;

    var moveSpeed = 3.5 * dt;
    var rotSpeed  = 2.8 * dt;

    if (ty >= 195) {
        // Turn Left [<]
        if (tx < 40) {
            rotatePlayer(-rotSpeed);
        }
        // Move Forward [^]
        else if (tx >= 40 && tx < 80 && ty < 235) {
            movePlayer(dirX * moveSpeed, dirY * moveSpeed);
        }
        // Move Backward [v]
        else if (tx >= 40 && tx < 80 && ty >= 235) {
            movePlayer(-dirX * moveSpeed, -dirY * moveSpeed);
        }
        // Turn Right [>]
        else if (tx >= 80 && tx < 120) {
            rotatePlayer(rotSpeed);
        }
        // Strafe Left [ST]
        else if (tx >= 125 && tx < 165) {
            movePlayer(-dirY * moveSpeed, dirX * moveSpeed);
        }
        // Fire Weapon [FIRE]
        else if (tx >= 170) {
            fireWeapon();
        }
    }
}

function rotatePlayer(angle) {
    playerAngle += angle;
    var cosA = FastMath.cos(angle);
    var sinA = FastMath.sin(angle);

    var oldDirX = dirX;
    dirX = dirX * cosA - dirY * sinA;
    dirY = oldDirX * sinA + dirY * cosA;
}

function movePlayer(dx, dy) {
    var nextX = posX + dx;
    var nextY = posY + dy;

    var mapX = Math.floor(nextX);
    var mapY = Math.floor(posY);
    if (mapX >= 0 && mapX < MAP_W && mapY >= 0 && mapY < MAP_H && MAP[mapY * MAP_W + mapX] === 0) {
        posX = nextX;
    }

    mapX = Math.floor(posX);
    mapY = Math.floor(nextY);
    if (mapX >= 0 && mapX < MAP_W && mapY >= 0 && mapY < MAP_H && MAP[mapY * MAP_W + mapX] === 0) {
        posY = nextY;
    }
}

function fireWeapon() {
    if (ammo <= 0 || muzzleFlash > 0) return;
    ammo--;
    muzzleFlash = 3;

    for (var i = 0; i < enemies.length; i++) {
        var e = enemies[i];
        if (!e.alive) continue;

        var dist = FastMath.vec2Distance(posX, posY, e.x, e.y);
        var ex = e.x - posX;
        var ey = e.y - posY;

        var dot = (ex * dirX + ey * dirY) / dist;
        if (dot > 0.90 && dist < 8.0) {
            e.hp -= 30;
            spawnBlood(e.x, 0.4, e.y);
            if (e.hp <= 0) {
                e.alive = 0;
                kills++;
                score += 150;
            }
            break;
        }
    }
}

function spawnBlood(x, y, z) {
    for (var i = 0; i < 8; i++) {
        particles.push({
            x: x,
            y: y,
            z: z,
            vx: FastMath.randomRange(-1.0, 1.0),
            vy: FastMath.randomRange(0.5, 2.0),
            vz: FastMath.randomRange(-1.0, 1.0),
            life: 12
        });
    }
}

// -----------------------------------------------------------------------------
// 3D Scene Rendering (Kryon3D Hardware Engine into Buffer)
// -----------------------------------------------------------------------------
function renderFloorGrid() {
    var minX = Math.floor(posX) - 5;
    var maxX = Math.floor(posX) + 5;
    var minY = Math.floor(posY) - 5;
    var maxY = Math.floor(posY) + 5;

    for (var x = minX; x <= maxX; x++) {
        Kryon3D.drawLine(x, 0.0, minY, x, 0.0, maxY, 0x2965);
    }
    for (var y = minY; y <= maxY; y++) {
        Kryon3D.drawLine(minX, 0.0, y, maxX, 0.0, y, 0x2965);
    }
}

function render3DWorld() {
    var startX = Math.floor(posX) - 6;
    var endX   = Math.floor(posX) + 6;
    var startY = Math.floor(posY) - 6;
    var endY   = Math.floor(posY) + 6;

    if (startX < 0) startX = 0;
    if (endX >= MAP_W) endX = MAP_W - 1;
    if (startY < 0) startY = 0;
    if (endY >= MAP_H) endY = MAP_H - 1;

    for (var my = startY; my <= endY; my++) {
        for (var mx = startX; mx <= endX; mx++) {
            var block = MAP[my * MAP_W + mx];
            if (block > 0) {
                var wallColor = C_WALL1;
                if (block === 2) wallColor = C_WALL2;
                else if (block === 3) wallColor = C_WALL3;

                Kryon3D.drawCube(mx + 0.5, 0.5, my + 0.5, 1.0, 1.0, 1.0, 0, 0, 0, wallColor, true);
            }
        }
    }
}

function render3DEnemies(dt) {
    for (var i = 0; i < enemies.length; i++) {
        var e = enemies[i];
        if (!e.alive) continue;

        e.rot += 1.5 * dt;
        var eColor = (e.type === 1) ? C_ORANGE : C_RED;

        Kryon3D.drawCube(e.x, 0.35, e.y, 0.45, 0.7, 0.45, 0, e.rot, 0, eColor, true);
        Kryon3D.drawBillboard(e.x, 0.65, e.y, 0.25, 0.25, C_YELLOW);
    }
}

function render3DParticles(dt) {
    for (var i = particles.length - 1; i >= 0; i--) {
        var p = particles[i];
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.vy -= 4.0 * dt;
        p.life--;

        Kryon3D.drawBillboard(p.x, p.y, p.z, 0.12, 0.12, C_RED);

        if (p.life <= 0 || p.y < 0) {
            particles.splice(i, 1);
        }
    }
}

// -----------------------------------------------------------------------------
// Weapon Overlay (Rendered into 3D Sprite Buffer)
// -----------------------------------------------------------------------------
function renderWeaponOverlay() {
    var gunX = 100;
    var gunY = 160;

    System.fillRect(gunX + 15, gunY, 10, 45, C_DARKGREY);
    System.fillRect(gunX + 27, gunY, 10, 45, C_DARKGREY);
    System.fillRect(gunX + 12, gunY + 20, 28, 20, C_GREY);
    System.fillRect(gunX + 18, gunY + 40, 16, 15, C_WALL1);

    if (muzzleFlash > 0) {
        muzzleFlash--;
        System.fillTriangle(gunX + 26, gunY - 30, gunX + 5, gunY + 2, gunX + 47, gunY + 2, C_YELLOW);
        System.fillCircle(gunX + 26, gunY - 10, 10, C_ORANGE);
        System.fillCircle(gunX + 26, gunY - 12, 5, C_WHITE);
    }
}

function renderMinimap() {
    var mapScale = 3;
    var ox = 4;
    var oy = 4;

    System.fillRect(ox - 1, oy - 1, MAP_W * mapScale + 2, MAP_H * mapScale + 2, C_BLACK);
    System.drawRect(ox - 1, oy - 1, MAP_W * mapScale + 2, MAP_H * mapScale + 2, C_GREY);

    for (var y = 0; y < MAP_H; y++) {
        for (var x = 0; x < MAP_W; x++) {
            if (MAP[y * MAP_W + x] > 0) {
                System.fillRect(ox + x * mapScale, oy + y * mapScale, mapScale, mapScale, C_GREY);
            }
        }
    }

    var px = Math.floor(ox + posX * mapScale);
    var py = Math.floor(oy + posY * mapScale);
    System.fillCircle(px, py, 2, C_GREEN);
    System.drawLine(px, py, px + Math.floor(dirX * 5), py + Math.floor(dirY * 5), C_YELLOW);

    for (var i = 0; i < enemies.length; i++) {
        var e = enemies[i];
        if (e.alive) {
            System.fillRect(ox + Math.floor(e.x * mapScale), oy + Math.floor(e.y * mapScale), 2, 2, C_RED);
        }
    }
}

// -----------------------------------------------------------------------------
// Render Static HUD Elements (Drawn ONCE directly to screen glass)
// -----------------------------------------------------------------------------
function drawStaticHUD() {
    // HUD background bar
    System.fillRect(0, 160, SW, 35, C_DARKGREY);
    System.drawFastHLine(0, 160, SW, C_GREY);

    System.setTextColor(C_RED, C_DARKGREY);
    System.drawString("HEALTH", 10, 164, 1);

    System.setTextColor(C_YELLOW, C_DARKGREY);
    System.drawString("AMMO", 80, 164, 1);

    System.setTextColor(C_GREEN, C_DARKGREY);
    System.drawString("KILLS", 140, 164, 1);

    System.setTextColor(C_CYAN, C_DARKGREY);
    System.drawString("FPS", 200, 164, 1);

    // Initial values
    System.setTextColor(C_WHITE, C_DARKGREY);
    System.drawString(health + "%", 10, 176, 2);
    System.drawString(ammo + "", 80, 176, 2);
    System.drawString(kills + "", 140, 176, 2);
    System.drawString(fps + "", 200, 176, 2);
    lastHealth = health;
    lastAmmo = ammo;
    lastKills = kills;
    lastFPS = fps;
}

// -----------------------------------------------------------------------------
// Render Static Touch Controls (Drawn ONCE directly to screen glass)
// -----------------------------------------------------------------------------
function drawStaticControls() {
    // Controls background
    var bgY = 195;
    System.fillRect(0, bgY, SW, SH - bgY, C_BLACK);
    System.drawFastHLine(0, bgY, SW, C_GREY);

    // D-Pad Buttons
    System.fillRoundRect(5, 202, 32, 30, 4, C_DARKGREY);
    System.setTextColor(C_WHITE, C_DARKGREY);
    System.drawString("<", 16, 210, 2);

    System.fillRoundRect(42, 198, 36, 32, 4, C_GREY);
    System.drawString("^", 56, 206, 2);

    System.fillRoundRect(42, 234, 36, 32, 4, C_GREY);
    System.drawString("v", 56, 242, 2);

    System.fillRoundRect(83, 202, 32, 30, 4, C_DARKGREY);
    System.drawString(">", 94, 210, 2);

    System.fillRoundRect(124, 202, 38, 62, 4, C_DARKGREY);
    System.drawString("ST", 132, 226, 2);

    System.fillRoundRect(168, 200, 66, 64, 6, C_RED);
    System.setTextColor(C_WHITE, C_RED);
    System.drawString("FIRE", 182, 224, 2);
}

// -----------------------------------------------------------------------------
// Differential HUD Updates (Only updates changed numbers directly on LCD glass)
// -----------------------------------------------------------------------------
function updateDynamicHUD() {
    if (health === lastHealth && ammo === lastAmmo && kills === lastKills && fps === lastFPS) {
        return;
    }

    Kryon3D.directDraw(true);

    if (health !== lastHealth) {
        lastHealth = health;
        System.fillRect(10, 176, 60, 16, C_DARKGREY);
        System.setTextColor(C_WHITE, C_DARKGREY);
        System.drawString(health + "%", 10, 176, 2);
    }

    if (ammo !== lastAmmo) {
        lastAmmo = ammo;
        System.fillRect(80, 176, 50, 16, C_DARKGREY);
        System.setTextColor(C_WHITE, C_DARKGREY);
        System.drawString(ammo + "", 80, 176, 2);
    }

    if (kills !== lastKills) {
        lastKills = kills;
        System.fillRect(140, 176, 50, 16, C_DARKGREY);
        System.setTextColor(C_WHITE, C_DARKGREY);
        System.drawString(kills + "", 140, 176, 2);
    }

    if (fps !== lastFPS) {
        lastFPS = fps;
        System.fillRect(200, 176, 38, 16, C_DARKGREY);
        System.setTextColor(C_WHITE, C_DARKGREY);
        System.drawString(fps + "", 200, 176, 2);
    }

    Kryon3D.directDraw(false);
}
