# KryonOS App Development Guide

Welcome to the KryonOS App Development Guide! Developing apps for KryonOS is simple. Apps are written in JavaScript and use a standard folder structure containing metadata and code.

## 1. App Folder Structure

In KryonOS, an app is no longer just a single `.js` file. Instead, every app is a **Folder** containing all its necessary files. When you upload your app via the Web Dashboard, you simply select your app's folder.

A standard app folder looks like this:
```text
MyAwesomeApp/
├── app.json
└── main.js
```

## 2. The `app.json` File (App Metadata)

The `app.json` file is the heart of your app's identity. The KryonOS Installer and Launcher read this file to securely install, update, launch, and categorize your application.

### Example Format:
```json
{
  "name": "Image Viewer",
  "packageName": "com.kryonos.imageviewer",
  "version": "1.0.0",
  "metaUrl": "https://raw.githubusercontent.com/.../imageviewer/app.json",
  "author": "KryonOS Team",
  "description": "Hardware-accelerated image and photo viewer.",
  "type": "Utility",
  "category": "Media",
  "api": 2,
  "main": "main.js",
  "changelog": "Initial release.",
  "fileAssociations": [".bmp", ".png", ".jpg"],
  "allowCompanionLaunch": true
}
```

### Manifest Fields Specification:

#### Compulsory Fields:
All standard identity and configuration fields are **COMPULSORY**:
- **`name`**: The display name of your app. This is what the user sees in the Launcher / Home.
- **`packageName`**: A globally unique identifier for your app. **Rules: lowercase, dot-separated style, no spaces** (e.g., `com.yourname.appname`). The OS uses this to detect if your app is already installed.
- **`version`**: Semantic versioning (e.g. `1.0.0`, `1.2.1`). If a user uploads an app with the same `packageName` but a higher version number, the OS will smartly prompt them to "Update" rather than "Install".
- **`author`**: Your name or studio. Protects your app from being overwritten by unauthorized developers.
- **`description`**: A short summary of your app, displayed to the user when installing.
- **`type`**: Broad classification (e.g., `App`, `Game`, `Utility`).
- **`category`**: **COMPULSORY** app categorization (e.g., `Utility`, `Media`, `Hardware`, `Security`, `System`, `Games`).
- **`api`**: Target KryonOS API level integer (e.g., `2`).
- **`main`**: The entry point JavaScript filename (e.g., `"main.js"`).
- **`metaUrl`**: (Optional for local, required for App Store) The raw URL to the `app.json` on the internet for auto-updates.
- **`changelog`**: A brief string detailing what changed on update.

#### Optional Fields:
- **`fileAssociations`** *(Array of Strings, Optional, Default: `[]`)*:
  Tells the OS which file extensions this app can open (e.g., `[".bmp", ".png", ".jpg"]` or `[".txt", ".log", ".json"]`). When another app calls `System.ipc.openFile(path)`, the OS inspects this registry to automatically launch your app with the target file path.
- **`allowCompanionLaunch`** *(Boolean, Optional, Default: `true`)*:
  Specifies whether other third-party apps are permitted to launch your app via `System.ipc.launch()`.
  - If **omitted**, it automatically **defaults to `true`**.
  - If explicitly set to **`false`**, external companion launch requests will be restricted by the OS.

> [!NOTE]
> **On-Demand Storage Permissions (No `permissions` field needed in `app.json`)**:
> KryonOS applications execute inside an isolated sandbox directory (their own package folder on LittleFS or SD Card) with unrestricted access to their own files. Standard hardware and network APIs require no permissions. If an app attempts to access external storage outside its folder (e.g., cross-storage LittleFS &harr; SD Card or shared paths), KryonOS automatically pauses and displays a native on-demand prompt (`Allow Once`, `Always Allow`, or `Deny`). Granted permissions can be reviewed and revoked anytime in **Settings &rarr; Permissions Manager**.

## 3. The `main.js` File (App Logic)

The `main.js` file is the entry point of your application. When a user clicks your app in the Launcher, the OS loads and executes this JavaScript file.

Because KryonOS handles the underlying C++ translation, you can write simple, high-level JavaScript to draw graphics, read files, and trigger UI components.

### Your First App (`main.js`)
Here is a simple example that turns the screen blue, prints "Hello KryonOS!", waits 3 seconds, and then gracefully exits back to the Launcher:

```javascript
// Clear the screen
Graphics.fillScreen(Graphics.COLOR_BLUE);

// Draw some text in the center
Graphics.setTextColor(Graphics.COLOR_WHITE);
Graphics.drawString("Hello KryonOS!", 120, 160, 2);

// Wait for 3 seconds
System.delay(3000);

// Close the app and return to the OS Launcher
System.exit();
```

> [!IMPORTANT]  
> To see everything you can do in `main.js`, please check out the full **[JS API Guide](JS_API_Guide.md)**! It contains all the documentation you need for Graphics, GPIO pins, File Systems, UI Components, and more.
