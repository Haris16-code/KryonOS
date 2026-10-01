#include "WebManager.h"
#include <WiFi.h>
#include <SD.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "../FileSystem/FileSystem.h"
#include "filemanager_html.h"
#include "../Kernel/TimeManager.h"
#include "../Kernel/WiFiManager.h"

static AsyncWebServer *server = nullptr;
static String s_adminUser = "admin";
static String s_adminPass = "";

struct SessionEntry {
    char token[33];
    time_t expiresAt;
    bool active;
};

static SessionEntry s_sessions[3] = {0};
static int s_failedAttempts = 0;
static time_t s_lockoutUntil = 0;

static void loadWebCredentials() {
    Preferences prefs;
    if (prefs.begin("kryon_web", false)) {
        s_adminUser = prefs.getString("user", "admin");
        s_adminPass = prefs.getString("pass", "");
        if (s_adminPass.length() == 0) {
            uint32_t r = esp_random();
            char buf[12];
            snprintf(buf, sizeof(buf), "%08lx", (unsigned long)r);
            s_adminPass = String(buf);
            prefs.putString("pass", s_adminPass);
        }
        prefs.end();
    } else {
        s_adminUser = "admin";
        s_adminPass = "admin123";
    }
}

String WebManager::getAdminUsername() {
    if (s_adminPass.length() == 0) loadWebCredentials();
    return s_adminUser;
}

String WebManager::getAdminPassword() {
    if (s_adminPass.length() == 0) loadWebCredentials();
    return s_adminPass;
}

void WebManager::setAdminCredentials(const String& username, const String& password) {
    s_adminUser = username;
    s_adminPass = password;
    Preferences prefs;
    if (prefs.begin("kryon_web", false)) {
        prefs.putString("user", s_adminUser);
        prefs.putString("pass", s_adminPass);
        prefs.end();
    }
}

static String generateSessionToken() {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();
    char buf[33];
    snprintf(buf, sizeof(buf), "%08lx%08lx%08lx%08lx", 
             (unsigned long)r1, (unsigned long)r2, (unsigned long)r3, (unsigned long)r4);
    return String(buf);
}

static String createSession(bool remember = false) {
    time_t now = time(nullptr);
    int slot = 0;
    time_t oldest = now + 999999999ULL;
    for (int i = 0; i < 3; i++) {
        if (!s_sessions[i].active || s_sessions[i].expiresAt < now) {
            slot = i;
            break;
        }
        if (s_sessions[i].expiresAt < oldest) {
            oldest = s_sessions[i].expiresAt;
            slot = i;
        }
    }
    String tok = generateSessionToken();
    strncpy(s_sessions[slot].token, tok.c_str(), 32);
    s_sessions[slot].token[32] = '\0';
    s_sessions[slot].expiresAt = remember ? (now + 315360000ULL) : (now + 86400); // 10 years if remember me, else 24 hours
    s_sessions[slot].active = true;
    return tok;
}

static bool isValidSession(const String& tok) {
    if (tok.length() != 32) return false;
    time_t now = time(nullptr);
    for (int i = 0; i < 3; i++) {
        if (s_sessions[i].active && s_sessions[i].expiresAt >= now) {
            if (strncmp(s_sessions[i].token, tok.c_str(), 32) == 0) {
                return true;
            }
        }
    }
    return false;
}

static void invalidateSession(const String& tok) {
    for (int i = 0; i < 3; i++) {
        if (s_sessions[i].active && strncmp(s_sessions[i].token, tok.c_str(), 32) == 0) {
            s_sessions[i].active = false;
        }
    }
}

// Standalone secure login gateway served to unauthenticated clients
static const char login_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>KryonOS — Web Server</title>
<style>
  :root {
    --bg-canvas: #1e1e1e;
    --bg-surface: #252526;
    --bg-elevated: #2d2d2d;
    --bg-hover: #3c3c3c;
    --border-subtle: #3c3c3c;
    --border-focus: #007acc;
    --text-primary: #ffffff;
    --text-secondary: #cccccc;
    --text-muted: #858585;
    --accent-primary: #007acc;
    --accent-fg: #ffffff;
    --accent-cyan: #38bdf8;
    --status-online: #10b981;
    --danger-fg: #f87171;
    --danger-border: #7f1d1d;
    --danger-bg: #1c1012;
    --font-sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    --font-mono: Consolas, "Courier New", monospace;
  }

  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
  }

  body {
    background-color: var(--bg-canvas);
    color: var(--text-primary);
    font-family: var(--font-sans);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 24px;
  }

  .auth-container {
    width: 100%;
    max-width: 380px;
  }

  .auth-panel {
    background-color: var(--bg-surface);
    border: 1px solid var(--border-subtle);
    border-radius: 12px;
    padding: 28px;
    box-shadow: 0 20px 40px rgba(0, 0, 0, 0.6);
  }

  .auth-panel.shake {
    animation: panelShake 0.35s cubic-bezier(0.36, 0.07, 0.19, 0.97) both;
    border-color: var(--danger-border);
  }

  @keyframes panelShake {
    10%, 90% { transform: translateX(-2px); }
    20%, 80% { transform: translateX(4px); }
    30%, 50%, 70% { transform: translateX(-6px); }
    40%, 60% { transform: translateX(6px); }
  }

  .panel-header {
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    margin-bottom: 24px;
    padding-bottom: 20px;
    border-bottom: 1px solid var(--border-subtle);
  }

  .brand-group {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .brand-icon {
    width: 38px;
    height: 38px;
    background-color: var(--bg-elevated);
    border: 1px solid var(--border-subtle);
    border-radius: 8px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--accent-cyan);
    flex-shrink: 0;
  }

  .brand-icon svg {
    width: 18px;
    height: 18px;
  }

  .brand-meta h1 {
    font-size: 16px;
    font-weight: 600;
    letter-spacing: -0.02em;
    color: var(--text-primary);
    line-height: 1.2;
  }

  .brand-meta p {
    font-size: 12px;
    color: var(--text-secondary);
    margin-top: 3px;
  }

  .ver-tag {
    font-family: var(--font-mono);
    font-size: 11px;
    font-weight: 500;
    color: var(--accent-cyan);
    background-color: var(--bg-elevated);
    border: 1px solid var(--border-subtle);
    padding: 3px 8px;
    border-radius: 6px;
  }

  .alert-banner {
    display: none;
    background-color: var(--danger-bg);
    border: 1px solid var(--danger-border);
    border-radius: 8px;
    padding: 10px 12px;
    margin-bottom: 20px;
    align-items: flex-start;
    gap: 10px;
    color: var(--danger-fg);
    font-size: 12.5px;
    line-height: 1.45;
  }

  .alert-banner.visible {
    display: flex;
  }

  .alert-banner svg {
    width: 16px;
    height: 16px;
    flex-shrink: 0;
    margin-top: 1px;
  }

  .field-group {
    margin-bottom: 16px;
  }

  .field-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 6px;
  }

  .field-label {
    font-size: 12px;
    font-weight: 500;
    color: var(--text-secondary);
  }

  .input-wrap {
    position: relative;
    display: flex;
    align-items: center;
  }

  .input-icon {
    position: absolute;
    left: 12px;
    width: 15px;
    height: 15px;
    color: var(--text-muted);
    pointer-events: none;
    transition: color 0.15s ease;
  }

  .field-input {
    width: 100%;
    height: 40px;
    background-color: var(--bg-canvas);
    border: 1px solid var(--border-subtle);
    border-radius: 8px;
    padding: 0 38px 0 36px;
    color: var(--text-primary);
    font-size: 13.5px;
    font-family: var(--font-sans);
    outline: none;
    transition: border-color 0.15s ease, background-color 0.15s ease;
  }

  .field-input.mono-input {
    font-family: var(--font-mono);
    letter-spacing: 0.03em;
  }

  .field-input::placeholder {
    color: var(--text-muted);
  }

  .field-input:hover:not(:disabled) {
    border-color: #555555;
  }

  .field-input:focus {
    border-color: var(--border-focus);
    background-color: #181818;
  }

  .input-wrap:focus-within .input-icon {
    color: var(--text-primary);
  }

  .field-input:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .visibility-btn {
    position: absolute;
    right: 6px;
    width: 28px;
    height: 28px;
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    color: var(--text-muted);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: color 0.15s ease, background-color 0.15s ease;
  }

  .visibility-btn:hover {
    color: var(--text-primary);
    background-color: var(--bg-elevated);
  }

  .visibility-btn svg {
    width: 15px;
    height: 15px;
  }

  .submit-btn {
    width: 100%;
    height: 40px;
    margin-top: 8px;
    background-color: var(--accent-primary);
    color: var(--accent-fg);
    border: 1px solid var(--accent-primary);
    border-radius: 8px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    transition: background-color 0.15s ease, opacity 0.15s ease;
  }

  .submit-btn:hover:not(:disabled) {
    background-color: #0098ff;
    border-color: #0098ff;
  }

  .submit-btn:disabled {
    background-color: var(--bg-elevated);
    border-color: var(--border-subtle);
    color: var(--text-muted);
    cursor: not-allowed;
  }

  .kbd-tag {
    font-family: var(--font-mono);
    font-size: 10px;
    font-weight: 600;
    padding: 1px 5px;
    border-radius: 4px;
    background-color: rgba(255, 255, 255, 0.2);
    color: inherit;
  }

  .spinner {
    display: none;
    width: 14px;
    height: 14px;
    border: 2px solid rgba(255, 255, 255, 0.3);
    border-top-color: #ffffff;
    border-radius: 50%;
    animation: spin 0.65s linear infinite;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  .panel-footer {
    margin-top: 22px;
    padding-top: 16px;
    border-top: 1px solid var(--border-subtle);
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 11px;
    color: var(--text-muted);
  }

  .help-link {
    background: none;
    border: none;
    color: var(--accent-cyan);
    font-size: 11.5px;
    font-family: var(--font-sans);
    cursor: pointer;
    text-decoration: underline;
    text-underline-offset: 3px;
    display: inline-flex;
    align-items: center;
    gap: 4px;
  }

  .help-link:hover {
    color: #7dd3fc;
  }

  /* Instructions Modal */
  .modal-overlay {
    display: none;
    position: fixed;
    top: 0;
    left: 0;
    width: 100vw;
    height: 100vh;
    background: rgba(0, 0, 0, 0.75);
    backdrop-filter: blur(4px);
    z-index: 9999;
    align-items: center;
    justify-content: center;
    padding: 16px;
  }

  .modal-overlay.open {
    display: flex;
  }

  .modal-card {
    background: var(--bg-surface);
    border: 1px solid var(--border-subtle);
    border-radius: 12px;
    padding: 24px;
    width: 100%;
    max-width: 380px;
    box-shadow: 0 20px 40px rgba(0, 0, 0, 0.7);
  }

  .modal-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 16px;
    padding-bottom: 12px;
    border-bottom: 1px solid var(--border-subtle);
  }

  .modal-header h2 {
    font-size: 15px;
    font-weight: 600;
    color: var(--text-primary);
  }

  .modal-close {
    background: none;
    border: none;
    color: var(--text-muted);
    font-size: 18px;
    cursor: pointer;
    line-height: 1;
    padding: 2px 6px;
    border-radius: 4px;
  }

  .modal-close:hover {
    color: var(--text-primary);
    background: var(--bg-elevated);
  }

  .modal-step {
    display: flex;
    gap: 12px;
    margin-bottom: 14px;
    font-size: 13px;
    line-height: 1.45;
    color: var(--text-secondary);
  }

  .step-num {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: var(--bg-elevated);
    border: 1px solid var(--border-subtle);
    color: var(--accent-cyan);
    font-size: 11px;
    font-weight: 600;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
  }

  .modal-btn {
    width: 100%;
    height: 38px;
    margin-top: 10px;
    background: var(--accent-primary);
    color: var(--accent-fg);
    border: none;
    border-radius: 6px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
  }

  .modal-btn:hover {
    background: #0098ff;
  }

  .options-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-top: 14px;
    margin-bottom: 4px;
  }

  .remember-label {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    font-size: 13px;
    color: var(--text-secondary);
    cursor: pointer;
    user-select: none;
  }

  .remember-label input[type="checkbox"] {
    position: absolute;
    opacity: 0;
    width: 0;
    height: 0;
    pointer-events: none;
  }

  .custom-checkbox {
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid var(--border-subtle);
    background: var(--bg-surface);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: all 0.15s ease;
    flex-shrink: 0;
  }

  .custom-checkbox svg {
    width: 11px;
    height: 11px;
    stroke: var(--accent-fg);
    stroke-width: 3;
    display: none;
  }

  .remember-label input[type="checkbox"]:checked + .custom-checkbox {
    background: var(--accent-primary);
    border-color: var(--accent-primary);
  }

  .remember-label input[type="checkbox"]:checked + .custom-checkbox svg {
    display: block;
  }

  .remember-label:hover .custom-checkbox {
    border-color: var(--border-focus);
  }
</style>
</head>
<body>

<div class="auth-container">
  <main class="auth-panel" id="authPanel">
    <header class="panel-header">
      <div class="brand-group">
        <div class="brand-icon" aria-hidden="true">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="4" y="4" width="16" height="16" rx="2" ry="2"/>
            <rect x="9" y="9" width="6" height="6"/>
            <line x1="9" y1="1" x2="9" y2="4"/>
            <line x1="15" y1="1" x2="15" y2="4"/>
            <line x1="9" y1="20" x2="9" y2="23"/>
            <line x1="15" y1="20" x2="15" y2="23"/>
            <line x1="20" y1="9" x2="23" y2="9"/>
            <line x1="20" y1="14" x2="23" y2="14"/>
            <line x1="1" y1="9" x2="4" y2="9"/>
            <line x1="1" y1="14" x2="4" y2="14"/>
          </svg>
        </div>
        <div class="brand-meta">
          <h1>KryonOS Web Server</h1>
          <p>Authenticate to manage device storage</p>
        </div>
      </div>
      <span class="ver-tag">v)rawliteral" KRYONOS_VERSION R"rawliteral(</span>
    </header>

    <div class="alert-banner" id="alertBanner" role="alert">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <circle cx="12" cy="12" r="10"/>
        <line x1="12" y1="8" x2="12" y2="12"/>
        <line x1="12" y1="16" x2="12.01" y2="16"/>
      </svg>
      <span id="alertMessage">Authentication failed. Verify credentials on device screen.</span>
    </div>

    <form id="loginForm" autocomplete="on" novalidate>
      <div class="field-group">
        <div class="field-header">
          <label class="field-label" for="username">Username</label>
        </div>
        <div class="input-wrap">
          <input
            type="text"
            id="username"
            name="username"
            class="field-input"
            placeholder="Enter username"
            maxlength="32"
            autocomplete="username"
            spellcheck="false"
            required
            autofocus
          >
          <svg class="input-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/>
            <circle cx="12" cy="7" r="4"/>
          </svg>
        </div>
      </div>

      <div class="field-group">
        <div class="field-header">
          <label class="field-label" for="password">Password</label>
        </div>
        <div class="input-wrap">
          <input
            type="password"
            id="password"
            name="password"
            class="field-input mono-input"
            placeholder="Enter password"
            maxlength="64"
            autocomplete="current-password"
            required
          >
          <svg class="input-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="3" y="11" width="18" height="11" rx="2" ry="2"/>
            <path d="M7 11V7a5 5 0 0 1 10 0v4"/>
          </svg>
          <button type="button" class="visibility-btn" id="pwToggle" title="Toggle password visibility">
            <svg id="eyeIcon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
              <circle cx="12" cy="12" r="3"/>
            </svg>
          </button>
        </div>
      </div>

      <div class="options-row">
        <label class="remember-label" for="rememberMe">
          <input type="checkbox" id="rememberMe" name="rememberMe" checked>
          <span class="custom-checkbox">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round">
              <polyline points="20 6 9 17 4 12"></polyline>
            </svg>
          </span>
          <span>Remember me</span>
        </label>
      </div>

      <button type="submit" class="submit-btn" id="submitBtn">
        <div class="spinner" id="btnSpinner"></div>
        <span id="btnText">Authenticate</span>
        <span class="kbd-tag">&#9166;</span>
      </button>
    </form>

    <footer class="panel-footer" style="justify-content: center;">
      <button type="button" class="help-link" id="helpBtn">
        <svg style="width:13px;height:13px;" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <circle cx="12" cy="12" r="10"/>
          <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/>
          <line x1="12" y1="17" x2="12.01" y2="17"/>
        </svg>
        Where do I find credentials?
      </button>
    </footer>
  </main>
</div>

<!-- Instructions Dialog Modal -->
<div class="modal-overlay" id="helpModal">
  <div class="modal-card">
    <div class="modal-header">
      <h2>Device Credentials Guide</h2>
      <button type="button" class="modal-close" id="modalCloseBtn">&times;</button>
    </div>
    <div class="modal-step">
      <span class="step-num">1</span>
      <span>Turn on your KryonOS device screen.</span>
    </div>
    <div class="modal-step">
      <span class="step-num">2</span>
      <span>From Launcher, open <strong>Settings</strong> &rarr; <strong>Wifi Options</strong> &rarr; <strong>Web Server</strong>.</span>
    </div>
    <div class="modal-step">
      <span class="step-num">3</span>
      <span>View your active <strong>User</strong> and <strong>Pass</strong> displayed directly under <em>Credentials</em>.</span>
    </div>
    <div class="modal-step">
      <span class="step-num">4</span>
      <span>You can tap <strong>[ Set User ]</strong> or <strong>[ Set Pass ]</strong> on the device to customize them anytime.</span>
    </div>
    <button type="button" class="modal-btn" id="modalGotItBtn">Got It</button>
  </div>
</div>

<script>
(function() {
  const form = document.getElementById('loginForm');
  const panel = document.getElementById('authPanel');
  const userInp = document.getElementById('username');
  const passInp = document.getElementById('password');
  const pwToggle = document.getElementById('pwToggle');
  const eyeIcon = document.getElementById('eyeIcon');
  const submitBtn = document.getElementById('submitBtn');
  const btnSpinner = document.getElementById('btnSpinner');
  const btnText = document.getElementById('btnText');
  const alertBanner = document.getElementById('alertBanner');
  const alertMessage = document.getElementById('alertMessage');
  const helpBtn = document.getElementById('helpBtn');
  const helpModal = document.getElementById('helpModal');
  const modalCloseBtn = document.getElementById('modalCloseBtn');
  const modalGotItBtn = document.getElementById('modalGotItBtn');

  let lockoutInterval = null;

  // Help Modal handlers
  function openHelp() { helpModal.classList.add('open'); }
  function closeHelp() { helpModal.classList.remove('open'); }
  helpBtn.addEventListener('click', openHelp);
  modalCloseBtn.addEventListener('click', closeHelp);
  modalGotItBtn.addEventListener('click', closeHelp);
  helpModal.addEventListener('click', (e) => { if (e.target === helpModal) closeHelp(); });

  // Check persisted client-side lockout
  const lockUntilStr = localStorage.getItem('kryon_lockout_until');
  if (lockUntilStr) {
    const lockUntil = parseInt(lockUntilStr, 10);
    const now = Math.floor(Date.now() / 1000);
    if (lockUntil > now) {
      enforceRateLimitLockout(lockUntil - now);
    } else {
      localStorage.removeItem('kryon_lockout_until');
    }
  }

  pwToggle.addEventListener('click', () => {
    const isPassword = passInp.type === 'password';
    passInp.type = isPassword ? 'text' : 'password';
    eyeIcon.innerHTML = isPassword
      ? '<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/>'
      : '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>';
    passInp.focus();
  });

  function sanitizeInput(val) {
    return val.replace(/[\x00-\x1F\x7F]/g, '').trim();
  }

  function triggerError(msg) {
    alertMessage.textContent = msg;
    alertBanner.classList.add('visible');
    panel.classList.remove('shake');
    void panel.offsetWidth;
    panel.classList.add('shake');
  }

  function enforceRateLimitLockout(seconds) {
    clearInterval(lockoutInterval);
    const lockUntil = Math.floor(Date.now() / 1000) + seconds;
    localStorage.setItem('kryon_lockout_until', lockUntil.toString());

    userInp.disabled = true;
    passInp.disabled = true;
    submitBtn.disabled = true;

    let remaining = seconds;
    triggerError(`Rate limit exceeded. Try again in ${remaining}s.`);
    btnText.textContent = `Locked (${remaining}s)`;

    lockoutInterval = setInterval(() => {
      remaining--;
      if (remaining <= 0) {
        clearInterval(lockoutInterval);
        lockoutInterval = null;
        localStorage.removeItem('kryon_lockout_until');
        userInp.disabled = false;
        passInp.disabled = false;
        submitBtn.disabled = false;
        alertBanner.classList.remove('visible');
        btnText.textContent = 'Authenticate';
        passInp.focus();
      } else {
        alertMessage.textContent = `Rate limit exceeded. Try again in ${remaining}s.`;
        btnText.textContent = `Locked (${remaining}s)`;
      }
    }, 1000);
  }

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    if (lockoutInterval) return;

    // Check client-side rate limit lock
    const lockUntilStr = localStorage.getItem('kryon_lockout_until');
    if (lockUntilStr) {
      const lockUntil = parseInt(lockUntilStr, 10);
      const now = Math.floor(Date.now() / 1000);
      if (lockUntil > now) {
        enforceRateLimitLockout(lockUntil - now);
        return;
      }
    }

    alertBanner.classList.remove('visible');
    const username = sanitizeInput(userInp.value);
    const password = sanitizeInput(passInp.value);
    const rememberMe = document.getElementById('rememberMe') ? document.getElementById('rememberMe').checked : false;

    if (!username || !password) {
      triggerError('Username and password are required.');
      return;
    }

    submitBtn.disabled = true;
    btnSpinner.style.display = 'block';
    btnText.textContent = 'Authenticating...';

    try {
      const res = await fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'same-origin',
        body: JSON.stringify({ username, password, rememberMe })
      });

      if (res.ok) {
        localStorage.removeItem('kryon_lockout_until');
        localStorage.removeItem('kryon_failed_attempts');
        btnText.textContent = 'Session Active';
        window.location.reload();
        return;
      }

      if (res.status === 429) {
        const payload = await res.json().catch(() => ({}));
        enforceRateLimitLockout(payload.retry_after || 30);
        return;
      }

      // Track local failed attempts
      let attempts = parseInt(localStorage.getItem('kryon_failed_attempts') || '0', 10) + 1;
      localStorage.setItem('kryon_failed_attempts', attempts.toString());
      if (attempts >= 5) {
        localStorage.removeItem('kryon_failed_attempts');
        enforceRateLimitLockout(30);
        return;
      }

      const payload = await res.json().catch(() => ({}));
      triggerError(payload.error || 'Invalid credentials. Check Web Server app on device.');
      passInp.value = '';
      passInp.focus();
    } catch (err) {
      triggerError('Device unreachable. Check Wi-Fi connection.');
    } finally {
      if (!lockoutInterval) {
        submitBtn.disabled = false;
        btnText.textContent = 'Authenticate';
      }
      btnSpinner.style.display = 'none';
    }
  });
})();
</script>

</body>
</html>
)rawliteral";

// Helper to extract session token from Cookie header
static String extractCookieToken(AsyncWebServerRequest *request) {
    if (request->hasHeader("Cookie")) {
        String cookieHeader = request->getHeader("Cookie")->value();
        int idx = cookieHeader.indexOf("kryon_session=");
        if (idx >= 0) {
            int start = idx + 14;
            int end = cookieHeader.indexOf(';', start);
            if (end == -1) end = cookieHeader.length();
            String tok = cookieHeader.substring(start, end);
            tok.trim();
            return tok;
        }
    }
    return "";
}

// Helper to check if request is authenticated
static bool isAuthenticated(AsyncWebServerRequest *request) {
    // 1. Check Session Cookie
    String cookieTok = extractCookieToken(request);
    if (cookieTok.length() > 0 && isValidSession(cookieTok)) {
        return true;
    }

    // 2. Check Authorization Bearer Token
    if (request->hasHeader("Authorization")) {
        String authHeader = request->getHeader("Authorization")->value();
        if (authHeader.startsWith("Bearer ")) {
            String bearerTok = authHeader.substring(7);
            bearerTok.trim();
            if (isValidSession(bearerTok)) return true;
        }
    }

    // 3. Check HTTP Basic Auth
    String u = WebManager::getAdminUsername();
    String p = WebManager::getAdminPassword();
    if (request->authenticate(u.c_str(), p.c_str())) {
        return true;
    }

    return false;
}

// Helper to strictly enforce Authentication on API endpoints (returns 403 Forbidden on failure)
static bool checkAuth(AsyncWebServerRequest *request) {
    if (isAuthenticated(request)) {
        return true;
    }
    AsyncWebServerResponse *res = request->beginResponse(403, "application/json", "{\"error\":\"Forbidden: Authentication required\"}");
    res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    res->addHeader("Pragma", "no-cache");
    res->addHeader("Expires", "0");
    request->send(res);
    return false;
}

// Helper to sanitize path strings (reject traversal .. and \)
static String normalizePath(const String& path) {
    if (path.indexOf("..") >= 0 || path.indexOf("\\") >= 0) return "__BLOCKED__";
    String p = path;
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    return p;
}

// Helper to check if a path is protected from web access
static bool isProtectedPath(const String& path) {
    if (path == "__BLOCKED__" || path.indexOf("..") >= 0 || path.indexOf("\\") >= 0) return true;
    if (path.indexOf("/system/") >= 0 || path.indexOf("/system") == 0 || 
        path.startsWith("/local/system") || path.startsWith("/sd/system") ||
        path.indexOf("wifi_credentials") >= 0 || path.indexOf("known_networks") >= 0 ||
        path.indexOf("app_permissions") >= 0) {
        return true;
    }
    return false;
}

// Helper to get FS based on path
static fs::FS* getFSFromPath(String& path) {
    if (path.startsWith("/sd")) {
        if (!FileSystem::isSDMounted()) return nullptr;
        path = path.substring(3);
        if (path == "") path = "/";
        return &SD;
    } else if (path.startsWith("/littlefs") || path.startsWith("/local")) {
        if (path.startsWith("/littlefs")) {
            path = path.substring(9);
        } else if (path.startsWith("/local")) {
            path = path.substring(6);
        }
        if (path == "") path = "/";
        return &LittleFS;
    }
    return nullptr;
}

// Helper to ensure each parent directory level exists sequentially in LittleFS/SD
static void ensureParentDirectories(fs::FS* fs, const String& path) {
    if (!fs) return;
    int pos = 0;
    while ((pos = path.indexOf('/', pos + 1)) > 0) {
        String dirPath = path.substring(0, pos);
        if (dirPath.length() > 0 && !fs->exists(dirPath)) {
            fs->mkdir(dirPath);
        }
    }
}

bool WebManager::startServer() {
    if (server != nullptr) {
        return true;
    }

    if (!WiFiManager::isConnected()) {
        Serial.println("[WebManager] Cannot start Async Web Server: WiFi is not connected.");
        return false;
    }

    loadWebCredentials();
    server = new AsyncWebServer(80);

    // Root page - serves file manager HTML UI only when authenticated; otherwise serves clean standalone login gateway with strict anti-caching
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response;
        if (isAuthenticated(request)) {
            response = request->beginResponse(200, "text/html", filemanager_html);
        } else {
            response = request->beginResponse(200, "text/html", login_html);
        }
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    // Session Login Endpoint (Rate-Limited & Sanitized)
    server->on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        time_t now = time(nullptr);
        if (now < s_lockoutUntil) {
            int remaining = (int)(s_lockoutUntil - now);
            if (remaining <= 0) remaining = 30;
            AsyncWebServerResponse *res = request->beginResponse(429, "application/json", "{\"error\":\"Too many failed login attempts\",\"retry_after\":" + String(remaining) + "}");
            res->addHeader("Retry-After", String(remaining));
            res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
            res->addHeader("Pragma", "no-cache");
            res->addHeader("Expires", "0");
            request->send(res);
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            AsyncWebServerResponse *res = request->beginResponse(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
            res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
            request->send(res);
            return;
        }

        String user = doc["username"] | "";
        String pass = doc["password"] | "";
        bool remember = doc["rememberMe"] | false;

        if (user == WebManager::getAdminUsername() && pass == WebManager::getAdminPassword()) {
            s_failedAttempts = 0;
            String tok = createSession(remember);
            uint32_t maxAge = remember ? 315360000 : 86400;

            AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\",\"token\":\"" + tok + "\"}");
            response->addHeader("Set-Cookie", "kryon_session=" + tok + "; HttpOnly; SameSite=Strict; Path=/; Max-Age=" + String(maxAge));
            response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
            response->addHeader("Pragma", "no-cache");
            response->addHeader("Expires", "0");
            request->send(response);
        } else {
            s_failedAttempts++;
            if (s_failedAttempts >= 5) {
                s_lockoutUntil = now + 30; // 30 second cooldown
                AsyncWebServerResponse *res = request->beginResponse(429, "application/json", "{\"error\":\"Too many failed login attempts\",\"retry_after\":30}");
                res->addHeader("Retry-After", "30");
                res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
                res->addHeader("Pragma", "no-cache");
                res->addHeader("Expires", "0");
                request->send(res);
                return;
            }
            AsyncWebServerResponse *res = request->beginResponse(401, "application/json", "{\"error\":\"Invalid username or password\",\"attempts_left\":" + String(5 - s_failedAttempts) + "}");
            res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
            res->addHeader("Pragma", "no-cache");
            res->addHeader("Expires", "0");
            request->send(res);
        }
    });

    // Session Logout Endpoint
    server->on("/api/logout", HTTP_POST, [](AsyncWebServerRequest *request){
        String cookieTok = extractCookieToken(request);
        if (cookieTok.length() > 0) {
            invalidateSession(cookieTok);
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"logged_out\"}");
        response->addHeader("Set-Cookie", "kryon_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    server->on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        JsonDocument doc;
        
        // LittleFS
        uint64_t lfsTotal = LittleFS.totalBytes();
        uint64_t lfsUsed = LittleFS.usedBytes();
        uint64_t lfsFree = (lfsTotal > lfsUsed) ? (lfsTotal - lfsUsed) : 0;
        JsonObject lfs = doc["littlefs"].to<JsonObject>();
        lfs["mounted"] = true;
        lfs["total"] = lfsTotal;
        lfs["used"] = lfsUsed;
        lfs["free"] = lfsFree;

        // SD Card
        bool sdMounted = FileSystem::isSDMounted();
        JsonObject sd = doc["sd"].to<JsonObject>();
        sd["mounted"] = sdMounted;
        if (sdMounted) {
            uint64_t sdTotal = SD.totalBytes();
            uint64_t sdUsed = SD.usedBytes();
            uint64_t sdFree = (sdTotal > sdUsed) ? (sdTotal - sdUsed) : 0;
            sd["total"] = sdTotal;
            sd["used"] = sdUsed;
            sd["free"] = sdFree;
        } else {
            sd["total"] = 0;
            sd["used"] = 0;
            sd["free"] = 0;
        }

        // System specs
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["heapFree"] = ESP.getFreeHeap();
        sys["heapTotal"] = ESP.getHeapSize();
#if defined(BOARD_HAS_PSRAM)
        bool psramOk = psramFound();
        sys["hasPsram"] = psramOk;
        sys["psramFree"] = psramOk ? ESP.getFreePsram() : 0;
        sys["psramTotal"] = psramOk ? ESP.getPsramSize() : 0;
#else
        sys["hasPsram"] = false;
        sys["psramFree"] = 0;
        sys["psramTotal"] = 0;
#endif
        sys["version"] = KRYONOS_VERSION;
        sys["chip"] = ESP.getChipModel();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server->on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("dir")) {
            request->send(400, "text/plain", "Missing dir parameter");
            return;
        }
        String origPath = normalizePath(request->getParam("dir")->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: System Path Protected");
            return;
        }

        String path = origPath;
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File dir = fs->open(path);
        if (!dir || !dir.isDirectory()) {
            request->send(404, "text/plain", "Not a directory");
            return;
        }

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        File file = dir.openNextFile();
        while (file) {
            String name = String(file.name());
            if (name != "system" && !name.startsWith("system/") && name != "wifi_credentials.enc" && name != "app_permissions.json") {
                JsonObject item = array.add<JsonObject>();
                item["name"] = name;
                item["type"] = file.isDirectory() ? "dir" : "file";
                item["size"] = file.size();
            }
            file.close();
            file = dir.openNextFile();
        }
        dir.close();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server->on("/api/edit", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String origPath = normalizePath(request->getParam("path")->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String path = origPath;
        fs::FS* fs = getFSFromPath(path);
        if (!fs || !fs->exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        request->send(*fs, path, "text/plain");
    });

    server->on("/api/edit", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("path", true) || !request->hasParam("content", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String origPath = normalizePath(request->getParam("path", true)->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String path = origPath;
        String content = request->getParam("content", true)->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File f = fs->open(path, FILE_WRITE);
        if (f) {
            f.print(content);
            f.close();
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to write file");
        }
    });

    // Direct raw streaming save endpoint (prevents OOM on large files)
    server->on("/api/save", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->_tempFile) {
            request->_tempFile.close();
        }
        request->send(200, "text/plain", "OK");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!checkAuth(request)) return;
        if (!request->hasParam("path")) return;
        
        String origPath = normalizePath(request->getParam("path")->value());
        if (isProtectedPath(origPath)) return;
        
        String path = origPath;
        fs::FS* fs = getFSFromPath(path);
        if (!fs) return;
        
        if (index == 0) {
            ensureParentDirectories(fs, path);
            request->_tempFile = fs->open(path, FILE_WRITE);
        }
        
        if (request->_tempFile) {
            if (len > 0) {
                request->_tempFile.write(data, len);
            }
            if (index + len >= total) {
                request->_tempFile.close();
            }
        }
    });

    server->on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String origPath = normalizePath(request->getParam("path")->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String path = origPath;
        fs::FS* fs = getFSFromPath(path);
        if (!fs || !fs->exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(*fs, path, "application/octet-stream", true);
        request->send(response);
    });

    server->on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("path", true)) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String origPath = normalizePath(request->getParam("path", true)->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String path = origPath;
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File f = fs->open(path);
        bool isDir = false;
        if (f) {
            isDir = f.isDirectory();
            f.close();
        }

        if (isDir) {
            fs->rmdir(path);
        } else {
            fs->remove(path);
        }
        request->send(200, "text/plain", "OK");
    });

    server->on("/api/create", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("path", true) || !request->hasParam("type", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String origPath = normalizePath(request->getParam("path", true)->value());
        if (isProtectedPath(origPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String path = origPath;
        String type = request->getParam("type", true)->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        if (type == "folder") {
            ensureParentDirectories(fs, path);
            fs->mkdir(path);
        } else {
            ensureParentDirectories(fs, path);
            File f = fs->open(path, FILE_WRITE);
            if (f) f.close();
        }
        request->send(200, "text/plain", "OK");
    });

    server->on("/api/rename", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkAuth(request)) return;

        if (!request->hasParam("oldPath", true) || !request->hasParam("newPath", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String oldPath = normalizePath(request->getParam("oldPath", true)->value());
        String newPath = normalizePath(request->getParam("newPath", true)->value());
        
        if (isProtectedPath(oldPath) || isProtectedPath(newPath)) {
            request->send(403, "text/plain", "Access Denied: Protected File");
            return;
        }

        String oldFsPath = oldPath;
        String newFsPath = newPath;
        fs::FS* fs1 = getFSFromPath(oldFsPath);
        fs::FS* fs2 = getFSFromPath(newFsPath);
        
        if (fs1 != fs2 || !fs1) {
            request->send(400, "text/plain", "Cannot rename across different storages or invalid");
            return;
        }

        ensureParentDirectories(fs1, newFsPath);
        if (fs1->rename(oldFsPath, newFsPath)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Rename failed");
        }
    });

    // Handle file & folder uploads with sequential LittleFS directory creation
    server->on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->_tempFile) {
            request->_tempFile.close();
        }
        request->send(200, "text/plain", "Upload Complete");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!checkAuth(request)) return;

        String path = normalizePath(filename); 
        if (isProtectedPath(path)) return;
        
        // Intercept app uploads and respect default installation location
        if (path.startsWith("/local/apps/") || path.startsWith("/sd/apps/")) {
            bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
            int appsIndex = path.indexOf("/apps/");
            String relativePath = path.substring(appsIndex + 6);
            
            if (defaultSD && FileSystem::exists("/sd/")) {
                path = "/sd/apps/" + relativePath;
            } else {
                path = "/local/apps/" + relativePath;
            }
        }
        
        fs::FS* fs = getFSFromPath(path);
        if (!fs) return;

        if (!index) {
            ensureParentDirectories(fs, path);
            request->_tempFile = fs->open(path, FILE_WRITE);
        }
        if (request->_tempFile) {
            if (len) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                request->_tempFile.close();
            }
        }
    });

    server->begin();
    Serial.println("[WebManager] Async Web Server successfully listening on port 80 (Authenticated)");
    return true;
}

void WebManager::stopServer() {
    if (server) {
        server->end();
        delete server;
        server = nullptr;
        Serial.println("[WebManager] Async Web Server stopped.");
    }
}

bool WebManager::isServerRunning() {
    return server != nullptr;
}

bool WebManager::init() {
    WiFiManager::init();

    if (WiFiManager::isConnected()) {
        Serial.println("[WebManager] WiFi connected via WiFiManager!");
        Serial.print("[WebManager] IP Address: ");
        Serial.println(WiFiManager::getIP());

        // Sync NTP Time
        TimeManager::syncNTP();

        // Check if Web Server is enabled by user
        if (FileSystem::exists("/local/web_on.txt")) {
            startServer();
        } else {
            Serial.println("[WebManager] Web Server is disabled by default (web_on.txt not set).");
        }
        return true;
    } else {
        Serial.println("[WebManager] WiFi is not connected at boot.");
        return false;
    }
}

bool WebManager::isActive() {
    return WiFiManager::isConnected();
}

String WebManager::getIPAddress() {
    return WiFiManager::getIP();
}
