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

The `app.json` file is the heart of your app's identity. The KryonOS Installer reads this file to securely install, update, and categorize your application. 

### Example Format:
```json
{
  "name": "My App",
  "packageName": "com.developer.myapp",
  "version": "1.0.0",
  "metaUrl": "https://raw.githubusercontent.com/.../myapp/app.json",
  "author": "John Doe",
  "description": "A cool app that does awesome things.",
  "type": "App",
  "category": "Utility",
  "api": 1,
  "changelog": "Initial release."
}
```

### Field Details:
- **`name`**: The display name of your app. This is what the user sees in the Home.
- **`packageName`**: A globally unique identifier for your app. **Rules: lowercase, dot-separated style, no spaces** (e.g., `com.yourname.appname`). The OS uses this to detect if your app is already installed.
- **`version`**: Semantic versioning (e.g. `1.0.0`, `1.2.1`). If a user uploads an app with the same `packageName` but a higher version number, the OS will smartly prompt them to "Update" rather than "Install".
- **`metaUrl`**: The raw URL to the `app.json` on the internet (e.g. your GitHub repository). The App Store uses this URL to automatically check for new versions of your app.
- **`author`**: Your name or studio. If someone else tries to upload an app with your `packageName` but a different `author` name, the OS will throw a conflict warning to protect your app from being overwritten by malicious developers.
- **`description`**: A short summary of your app, displayed to the user when they install your app for the first time.
- **`type`**: The broad classification (e.g., `App` or `Game`). You can type any value here without restriction.
- **`category`**: The specific category (e.g., `Benchmark`, `Utility`, `Arcade`). You can type any value here without restriction.
- **`api`**: The KryonOS API level your app targets. Currently, this should be `1`. (This is verified by the system at time of Installing App.).
- **`changelog`**: A brief string detailing what changed. When a user updates your app, this replaces the description and shows up under a "What's New" header!

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
