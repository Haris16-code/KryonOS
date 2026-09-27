// KryonOS Hardware Benchmark App
// Tests FastMath hardware floating point unit and Kryon3D rasterization

var SW = System.screenWidth();
var SH = System.screenHeight();

// Colors
var C_BLACK = 0x0000;
var C_WHITE = 0xFFFF;
var C_GREEN = 0x07E0;
var C_RED   = 0xF800;
var C_CYAN  = 0x07FF;
var C_YELLOW = 0xFFE0;

var scoreMath = 0;
var score3D = 0;
var score2D = 0;

function drawHeader(title) {
    System.fillRect(0, 0, SW, 30, 0x18C3);
    System.setTextColor(C_WHITE, 0x18C3);
    System.drawString(title, 10, 10, 2);
}

function runMathBench() {
    System.fillScreen(C_BLACK);
    drawHeader("1/3: FastMath FPU");
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("Running heavy math...", 10, 50, 2);
    
    var start = System.millis();
    var sum = 0;
    // 50,000 iterations of hardware trig and sqrt
    for(var i=0; i<50000; i++) {
        var a = FastMath.sin(i * 0.01);
        var b = FastMath.cos(i * 0.01);
        var c = FastMath.sqrt(i);
        sum += (a * b) + c;
    }
    
    var end = System.millis();
    var duration = end - start;
    if (duration === 0) duration = 1;
    scoreMath = Math.floor(1000000 / duration); // Higher is better
    
    System.fillScreen(C_BLACK);
    drawHeader("1/3: FastMath FPU");
    System.drawString("Math Done! Time: " + duration + "ms", 10, 80, 2);
    System.drawString("Score: " + scoreMath, 10, 100, 2);
    System.delay(1000);
}

function run2DBench() {
    System.fillScreen(C_BLACK);
    drawHeader("2/3: 2D DMA Fill");
    var start = System.millis();
    
    // 2,000 filled rectangles
    for(var i=0; i<2000; i++) {
        var x = FastMath.randomRange(0, SW);
        var y = FastMath.randomRange(30, SH);
        var w = FastMath.randomRange(10, 60);
        var h = FastMath.randomRange(10, 60);
        System.fillRect(x, y, w, h, FastMath.randomRange(0, 0xFFFF));
    }
    
    var end = System.millis();
    var duration = end - start;
    if (duration === 0) duration = 1;
    score2D = Math.floor(1000000 / duration);
    
    System.fillScreen(C_BLACK);
    drawHeader("2/3: 2D DMA Fill");
    System.drawString("2D Done! Time: " + duration + "ms", 10, 80, 2);
    System.drawString("Score: " + score2D, 10, 100, 2);
    System.delay(1000);
}

function run3DBench() {
    System.fillScreen(C_BLACK);
    drawHeader("3/3: Kryon3D Engine");
    
    // Allocate 160x160 8-bit double buffer to ensure it fits in RAM
    if (!Kryon3D.begin(160, 160, 8)) {
        System.drawString("Failed to alloc 3D buffer!", 10, 50, 2);
        System.delay(2000);
        return;
    }
    
    Kryon3D.setViewport(0, 0, 160, 160);
    Kryon3D.setCamera(0, 0, -4.5, 0, 0, 0, 60);
    Kryon3D.setLight(0.5, 1.0, -0.5, 0.3, 0.7);
    
    var start = System.millis();
    var f = 0;
    var angle = 0;
    
    // Render 150 frames as fast as possible with dense mesh
    while (f < 150) {
        Kryon3D.clear(0x0000);
        angle += 0.15;
        
        // Draw 9 spinning cubes to stress hardware culling, transformations, and lighting
        for(var cx=-1; cx<=1; cx++) {
            for(var cy=-1; cy<=1; cy++) {
                Kryon3D.drawCube(cx*1.2, cy*1.2, 0, 0.8, 0.8, 0.8, angle, angle*0.5, angle*0.2, 0x07E0, true);
            }
        }
        
        // Push 160x160 buffer to center of screen
        Kryon3D.render(40, 60);
        f++;
        // Notice we do NOT call System.delay() here. We want to test maximum unthrottled throughput.
    }
    
    var end = System.millis();
    var duration = end - start;
    if (duration === 0) duration = 1;
    var fps = Math.floor(1000 * 150 / duration);
    score3D = fps * 150; // Weighted 3D score
    
    Kryon3D.end();
    
    System.fillScreen(C_BLACK);
    drawHeader("3/3: Kryon3D Engine");
    System.drawString("3D Done! Avg FPS: " + fps, 10, 80, 2);
    System.drawString("Score: " + score3D, 10, 100, 2);
    System.delay(1000);
}

function showResults() {
    System.fillScreen(C_BLACK);
    drawHeader("Benchmark Results");
    
    var total = scoreMath + score2D + score3D;
    
    System.setTextColor(C_CYAN, C_BLACK);
    System.drawString("FastMath FPU:", 10, 60, 2);
    System.drawString(scoreMath + "", 150, 60, 2);
    
    System.setTextColor(C_GREEN, C_BLACK);
    System.drawString("2D Graphics:", 10, 90, 2);
    System.drawString(score2D + "", 150, 90, 2);
    
    System.setTextColor(C_RED, C_BLACK);
    System.drawString("Kryon3D Engine:", 10, 120, 2);
    System.drawString(score3D + "", 150, 120, 2);
    
    System.drawFastHLine(10, 150, 220, C_WHITE);
    
    System.setTextColor(C_YELLOW, C_BLACK);
    System.drawString("TOTAL SCORE:", 10, 170, 2);
    System.drawString(total + "", 150, 170, 2);
    
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("Touch screen to exit...", 10, 250, 2);
}

// Start sequence
System.fillScreen(C_BLACK);

runMathBench();
run2DBench();
run3DBench();
showResults();

// Wait for exit
while (true) {
    var touch = System.getTouch();
    if (touch.touched) {
        break;
    }
    System.delay(50);
}
