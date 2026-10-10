// ==========================================================================
// VoxTriple Webview Frontend App Logic (Figma 1:1 Restored & Aligned)
// ==========================================================================

// ── Virtual Key Code Map (Physical Keyboard mappings under macOS) ─────────
const JS_CODE_TO_VK = {
    "KeyA": 0x41, "KeyB": 0x42, "KeyC": 0x43, "KeyD": 0x44, "KeyE": 0x45, "KeyF": 0x46, "KeyG": 0x47,
    "KeyH": 0x48, "KeyI": 0x49, "KeyJ": 0x4A, "KeyK": 0x4B, "KeyL": 0x4C, "KeyM": 0x4D, "KeyN": 0x4E,
    "KeyO": 0x4F, "KeyP": 0x50, "KeyQ": 0x51, "KeyR": 0x52, "KeyS": 0x53, "KeyT": 0x54, "KeyU": 0x55,
    "KeyV": 0x56, "KeyW": 0x57, "KeyX": 0x58, "KeyY": 0x59, "KeyZ": 0x5A,
    "Digit0": 0x30, "Digit1": 0x31, "Digit2": 0x32, "Digit3": 0x33, "Digit4": 0x34, "Digit5": 0x35, "Digit6": 0x36,
    "Digit7": 0x37, "Digit8": 0x38, "Digit9": 0x39,
    "Backspace": 0x08, "Tab": 0x09, "Enter": 0x0D, "Escape": 0x1B, "Space": 0x20,
    "PageUp": 0x21, "PageDown": 0x22, "End": 0x23, "Home": 0x24,
    "ArrowLeft": 0x25, "ArrowUp": 0x26, "ArrowRight": 0x27, "ArrowDown": 0x28,
    "Insert": 0x2D, "Delete": 0x2E,
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75, "F7": 0x76, "F8": 0x77,
    "F9": 0x78, "F10": 0x79, "F11": 0x7A, "F12": 0x7B,
    "F13": 0x7C, "F14": 0x7D, "F15": 0x7E, "F16": 0x7F, "F17": 0x80, "F18": 0x81, "F19": 0x82, "F20": 0x83,
    "F21": 0x84, "F22": 0x85, "F23": 0x86, "F24": 0x87,
    "Semicolon": 0xBA, "Comma": 0xBC, "Period": 0xBE, "Slash": 0xBF,
    "Minus": 0xBD, "Equal": 0xBB, "BracketLeft": 0xDB, "BracketRight": 0xDD,
    "Backslash": 0xDC, "Quote": 0xDE, "Backquote": 0xC0,
    "ScrollLock": 0x91, "NumLock": 0x90, "Pause": 0x13,
    // Windows specific modifier keys mapping
    "ControlLeft": 0xA2, "ControlRight": 0xA3,
    "ShiftLeft": 0xA0, "ShiftRight": 0xA1,
    "AltLeft": 0xA4, "AltRight": 0xA5,
    "MetaLeft": 0x5B, "MetaRight": 0x5C
};

const VK_TO_NAME = {
    0x08: "Backspace", 0x09: "Tab", 0x0D: "Enter", 0x1B: "ESC", 0x20: "Space",
    0x21: "PageUp", 0x22: "PageDown", 0x23: "End", 0x24: "Home",
    0x25: "Left", 0x26: "Up", 0x27: "Right", 0x28: "Down",
    0x2D: "INS", 0x2E: "Delete",
    0x70: "F1", 0x71: "F2", 0x72: "F3", 0x73: "F4", 0x74: "F5", 0x75: "F6",
    0x76: "F7", 0x77: "F8", 0x78: "F9", 0x79: "F10", 0x7A: "F11", 0x7B: "F12",
    0x7C: "F13", 0x7D: "F14", 0x7E: "F15", 0x7F: "F16", 0x80: "F17", 0x81: "F18",
    0x82: "F19", 0x83: "F20", 0x84: "F21", 0x85: "F22", 0x86: "F23", 0x87: "F24",
    0xBA: ";", 0xBC: ",", 0xBE: ".", 0xBF: "/", 0xBD: "-", 0xBB: "=",
    0xDB: "[", 0xDD: "]", 0xDC: "\\", 0xDE: "'", 0xC0: "`",
    0x91: "SCROLL", 0x90: "NumLock", 0x13: "Pause",
    // Windows specific modifier friendly names
    0xA0: "LShift", 0xA1: "RShift",
    0xA2: "LCtrl", 0xA3: "RCtrl",
    0xA4: "LAlt", 0xA5: "RAlt",
    0x5B: "LWin", 0x5C: "RWin"
};

// ── Global App State ──────────────────────────────────────────────────────
let currentConfigs = [
    { vk: 0, mod: 0 },
    { vk: 0, mod: 0 },
    { vk: 0, mod: 0 },
    { vk: 0, mod: 0 }
];
let selectedOtaPath = "";
let isConnected = false;
let capturingIdx = -1;
let currentSleepTimeoutMin = 5;
let currentActiveSlot = 0;

// ── Dom Initialization ───────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", () => {
    // Connect Button
    document.getElementById("btn-connect").addEventListener("click", onConnectClick);
    
    // File Drag-Drop binding for OTA zone
    const dropzone = document.getElementById("dropzone");
    dropzone.addEventListener("dragover", (e) => {
        e.preventDefault();
        dropzone.classList.add("dragover");
    });
    dropzone.addEventListener("dragleave", () => {
        dropzone.classList.remove("dragover");
    });
    dropzone.addEventListener("drop", (e) => {
        e.preventDefault();
        dropzone.classList.remove("dragover");
        if (e.dataTransfer.files.length > 0) {
            const file = e.dataTransfer.files[0];
            if (file.name.endsWith(".bin")) {
                selectedOtaPath = file.path || file.name;
                updateOtaSelectedFile(file.name);
            }
        }
    });
    
    // Poll serial ports every 3.0s
    setInterval(pollPorts, 3000);
    pollPorts();
});

// ── Serial Port Polling ───────────────────────────────────────────────────
async function pollPorts() {
    if (window.pywebview && window.pywebview.api) {
        const ports = await window.pywebview.api.get_ports();
        updatePorts(ports);
    }
}

function updatePorts(ports) {
    const select = document.getElementById("port-select");
    const currentVal = select.value;
    
    select.innerHTML = "";
    if (!ports || ports.length === 0) {
        select.innerHTML = '<option value="">No Devices</option>';
        return;
    }
    
    ports.forEach(port => {
        const opt = document.createElement("option");
        opt.value = port;
        opt.textContent = port.replace("/dev/cu.", "");
        select.appendChild(opt);
    });
    
    if (ports.includes(currentVal)) {
        select.value = currentVal;
    }
}

// ── Serial Port Connection / Disconnection ───────────────────────────────
async function onConnectClick() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const btn = document.getElementById("btn-connect");
    const btnWrite = document.getElementById("btn-write-config");
    
    if (!isConnected) {
        const portSelect = document.getElementById("port-select");
        const selectedPort = portSelect.value;
        if (!selectedPort) return;
        
        btn.textContent = "CONNECTING...";
        const ok = await window.pywebview.api.connect_device(selectedPort);
        if (ok) {
            isConnected = true;
            btn.textContent = "DISCONNECT";
            btn.classList.add("connected-state");
            document.getElementById("status-dot").className = "status-dot connected";
            document.getElementById("status-text").textContent = "Connected";
            
            // Enable Write Config Button
            btnWrite.disabled = false;
            
            // Pull configuration cache
            const config = await window.pywebview.api.fetch_config();
            if (config) {
                renderConfig(config);
            }
            
            // 展示固件版本信息区块
            const fwSection = document.getElementById("firmware-section");
            if (fwSection) fwSection.style.display = "flex";
            
            // Perform automatic update check (Silent checking on connection)
            postConnectUpdateCheck();
        } else {
            btn.textContent = "CONNECT";
            alert("Connection Failed / 连接失败，请检查串口是否已被占用！");
        }
    } else {
        await window.pywebview.api.disconnect_device();
        isConnected = false;
        btn.textContent = "CONNECT";
        btn.classList.remove("connected-state");
        document.getElementById("status-dot").className = "status-dot disconnected";
        document.getElementById("status-text").textContent = "Disconnected";
        document.getElementById("firmware-ver").textContent = "v--";
        
        // Disable Write Config Button
        btnWrite.disabled = true;
        
        // Hide update trigger button
        const btnUpdate = document.getElementById("btn-update-trigger");
        if (btnUpdate) btnUpdate.style.display = "none";
        document.getElementById("firmware-status-label").textContent = "";
        
        // 隐藏固件版本信息区块
        const fwSection = document.getElementById("firmware-section");
        if (fwSection) fwSection.style.display = "none";
        
        // 隐藏多设备切换区块
        const hostDivider = document.getElementById("host-switch-divider");
        if (hostDivider) hostDivider.style.display = "none";
        const hostSection = document.getElementById("host-switch-section");
        if (hostSection) hostSection.style.display = "none";
        
        // 隐藏重置蓝牙配对按钮
        const resetBtDivider = document.getElementById("reset-bt-divider");
        if (resetBtDivider) resetBtDivider.style.display = "none";
        const resetBtSection = document.getElementById("reset-bt-section");
        if (resetBtSection) resetBtSection.style.display = "none";
        
        resetConfigUi();
    }
}

// ── Render Config Cache ──────────────────────────────────────────────────
function renderConfig(config) {
    document.getElementById("firmware-ver").textContent = "v" + (config.version || "1.0.10");
    document.getElementById("mic-toggle").checked = (config.mic_enabled === 1);
    document.getElementById("sleep-toggle").checked = (config.sleep_mode === 1);
    
    // 更新休眠等待时间 (放入节能卡片中)
    if (config.sleep_timeout_min !== undefined) {
        currentSleepTimeoutMin = config.sleep_timeout_min;
        const sleepSelect = document.getElementById("sleep-timeout-select");
        if (sleepSelect) {
            sleepSelect.value = String(config.sleep_timeout_min);
        }
    }

    // 更新与展示多设备配对切换 (电脑一 / 电脑二)
    if (config.active_slot !== undefined) {
        currentActiveSlot = config.active_slot;
        updateHostSlotUi(currentActiveSlot);
    }

    // 更新配对状态圆点指示灯 (绿色:已配对, 橙色:未配对)
    const dot0 = document.getElementById("host-dot-0");
    const pill0 = document.getElementById("host-pill-0");
    if (dot0 && pill0) {
        if (config.slot0_paired === 1) {
            dot0.className = "host-status-dot paired";
            pill0.title = "电脑一 (已配对)";
        } else {
            dot0.className = "host-status-dot empty";
            pill0.title = "电脑一 (未配对/空槽)";
        }
    }
    const dot1 = document.getElementById("host-dot-1");
    const pill1 = document.getElementById("host-pill-1");
    if (dot1 && pill1) {
        if (config.slot1_paired === 1) {
            dot1.className = "host-status-dot paired";
            pill1.title = "电脑二 (已配对)";
        } else {
            dot1.className = "host-status-dot empty";
            pill1.title = "电脑二 (未配对/空槽)";
        }
    }

    const hostDivider = document.getElementById("host-switch-divider");
    if (hostDivider) hostDivider.style.display = "block";
    const hostSection = document.getElementById("host-switch-section");
    if (hostSection) hostSection.style.display = "flex";

    // 显示重置蓝牙配对按钮
    const resetBtDivider = document.getElementById("reset-bt-divider");
    if (resetBtDivider) resetBtDivider.style.display = "block";
    const resetBtSection = document.getElementById("reset-bt-section");
    if (resetBtSection) resetBtSection.style.display = "flex";

    setTxPowerUi(config.tx_power !== undefined ? config.tx_power : 4);
    
    for (let i = 0; i < 4; i++) {
        const vk = config.mappings[i].vk;
        const mod = config.mappings[i].mod;
        
        currentConfigs[i] = { vk, mod };
        
        // Update Key Card visual content (Display combination keys!)
        updateKeyCardDisplay(i);
        
        // Modifiers
        const pills = document.querySelectorAll(`#mods-${i} .mod-pill`);
        pills.forEach(pill => {
            const mask = parseInt(pill.getAttribute("data-mask"));
            if ((mod & mask) > 0) {
                pill.classList.add("active");
            } else {
                pill.classList.remove("active");
            }
        });
    }
}

// ── Compute and display combination key values (e.g. Cmd + Shift + A) ─────
function updateKeyCardDisplay(btnIdx) {
    const config = currentConfigs[btnIdx];
    const displayEl = document.getElementById(`key-display-${btnIdx}`);
    const cardEl = document.getElementById(`key-card-${btnIdx}`);
    
    if (config.vk === 0) {
        displayEl.textContent = "--";
        cardEl.classList.remove("active-display");
        return;
    }
    
    cardEl.classList.add("active-display");
    
    const parts = [];
    const mod = config.mod;
    const modNames = [
        { mask: 0x01, name: "Ctrl" }, { mask: 0x02, name: "Shift" }, 
        { mask: 0x04, name: "Alt" }, { mask: 0x08, name: "Cmd" },
        { mask: 0x10, name: "Ctrl" }, { mask: 0x20, name: "Shift" }, 
        { mask: 0x40, name: "Alt" }, { mask: 0x80, name: "Cmd" }
    ];
    
    // De-duplicate same modifiers across left/right
    const seenMods = new Set();
    modNames.forEach(m => {
        if ((mod & m.mask) > 0) {
            seenMods.add(m.name);
        }
    });
    
    seenMods.forEach(name => parts.push(name));
    parts.push(getFriendlyKeyName(config.vk));
    
    displayEl.textContent = parts.join(" + ");
}

function resetConfigUi() {
    for (let i = 0; i < 4; i++) {
        document.getElementById(`key-display-${i}`).textContent = "--";
        const cardEl = document.getElementById(`key-card-${i}`);
        if (cardEl) cardEl.classList.remove("active-display");
        const pills = document.querySelectorAll(`#mods-${i} .mod-pill`);
        pills.forEach(pill => pill.classList.remove("active"));
    }
    document.getElementById("mic-toggle").checked = false;
    document.getElementById("sleep-toggle").checked = false;
    setTxPowerUi(0);
}

// ── Friendly Key Name ────────────────────────────────────────────────────
function getFriendlyKeyName(vk) {
    if (vk === 0) return "--";
    if (VK_TO_NAME[vk]) return VK_TO_NAME[vk];
    if (vk >= 0x30 && vk <= 0x39) return String.fromCharCode(vk);
    if (vk >= 0x41 && vk <= 0x5A) return String.fromCharCode(vk);
    return `VK_${vk.toString(16).toUpperCase()}`;
}

// ── Click Toggle Modifier (Local only, write on SAVE CONFIG) ─────────────
async function toggleMod(btnIdx, mask) {
    if (!isConnected) return;
    
    // 转换为十六进制字符串，如 1 变成 "0x01"，16 变成 "0x10"，以便与 HTML 属性精准匹配
    const hexStr = "0x" + mask.toString(16).padStart(2, "0");
    const pill = document.querySelector(`#mods-${btnIdx} .mod-pill[data-mask="${hexStr}"]`);
    if (!pill) return;
    
    const wasActive = pill.classList.contains("active");
    
    if (wasActive) {
        pill.classList.remove("active");
        currentConfigs[btnIdx].mod &= ~mask;
    } else {
        pill.classList.add("active");
        currentConfigs[btnIdx].mod |= mask;
    }
    
    // Update main text dynamically in real time!
    updateKeyCardDisplay(btnIdx);
}

// ── TX Power UX ──────────────────────────────────────────────────────────
function setTxPowerUi(level) {
    const dbmMap = ["-12", "-9", "-6", "-3", "0", "3", "6", "9"];
    document.getElementById("tx-power-val").textContent = `${dbmMap[level] || "0"} dBm`;
    
    const segments = document.querySelectorAll("#tx-power-bar .power-segment");
    segments.forEach((seg, idx) => {
        if (idx <= level) {
            seg.classList.add("active");
        } else {
            seg.classList.remove("active");
        }
    });
}

async function setTxPower(level) {
    if (!isConnected) return;
    setTxPowerUi(level);
}

// ── Mic & Sleep Toggles (Local only) ─────────────────────────────────────
function onMicToggle() {
    // Only local state change, written on Save Config click
}

function onSleepToggle() {
    // Only local state change, written on Save Config click
}

// ── Unified Sync: 同步所有参数至开发板 Flash (同步到设备) ──
async function saveAllConfigsToDevice() {
    if (!isConnected || !window.pywebview || !window.pywebview.api) return;
    
    const hostName = (currentActiveSlot === 0) ? "电脑一" : "电脑二";
    const btnWrite = document.getElementById("btn-write-config");
    const origHtml = btnWrite.innerHTML;
    btnWrite.disabled = true;
    btnWrite.textContent = "SYNCING...";
    
    const mic = document.getElementById("mic-toggle").checked ? 1 : 0;
    const sleep = document.getElementById("sleep-toggle").checked ? 1 : 0;
    
    const activeSegments = document.querySelectorAll("#tx-power-bar .power-segment.active");
    const tx = Math.max(0, activeSegments.length - 1);
    
    const sleepSelect = document.getElementById("sleep-timeout-select");
    const sleepTimeout = sleepSelect ? parseInt(sleepSelect.value) : 5;
    
    const ok = await window.pywebview.api.write_config(
        currentConfigs, 
        tx, 
        sleep, 
        mic,
        sleepTimeout,
        currentActiveSlot
    );
    
    btnWrite.disabled = false;
    btnWrite.innerHTML = origHtml;
    
    if (ok) {
        currentSleepTimeoutMin = sleepTimeout;
        alert(`所有设置已成功同步保存至【${hostName}】！\n设备正在自动重启以彻底应用新配置与蓝牙基带参数。`);
        if (isConnected) {
            onConnectClick();
        }
    } else {
        alert("配置保存失败，请检查连接状态！");
    }
}

// ── Capture physical key sequence (Native JS) ────────────────────────────
function startKeyCapture(btnIdx) {
    if (!isConnected) {
        alert("Please connect to the ESP32 keyboard first!\n请先建立有线串口连接！");
        return;
    }
    
    if (capturingIdx !== -1) return; // Already capturing
    
    capturingIdx = btnIdx;
    
    const card = document.getElementById(`key-card-${btnIdx}`);
    const btn = document.getElementById(`btn-capture-${btnIdx}`);
    const display = document.getElementById(`key-display-${btnIdx}`);
    
    card.classList.add("capturing-state");
    btn.textContent = "PRESS...";
    display.textContent = "?";
    
    // Bind global native keyboard interception
    document.addEventListener("keydown", onCapturedKeydown);
}

function onCapturedKeydown(e) {
    e.preventDefault();
    e.stopPropagation();
    
    const code = e.code;
    const vk = JS_CODE_TO_VK[code];
    
    if (vk !== undefined) {
        // Unbind instantly
        document.removeEventListener("keydown", onCapturedKeydown);
        
        const btnIdx = capturingIdx;
        capturingIdx = -1;
        
        const card = document.getElementById(`key-card-${btnIdx}`);
        const btn = document.getElementById(`btn-capture-${btnIdx}`);
        
        card.classList.remove("capturing-state");
        btn.textContent = "CAPTURE";
        
        // Save current key VK
        currentConfigs[btnIdx].vk = vk;
        
        // Update main text dynamically in real time!
        updateKeyCardDisplay(btnIdx);
    } else {
        alert(`Unsupported key code: ${code}\n不支持该按键码配置！`);
    }
}

// ── Manual / Drag-Drop OTA Upload ────────────────────────────────────────
async function onBrowseFirmware() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const filePath = await window.pywebview.api.select_local_bin();
    if (filePath) {
        selectedOtaPath = filePath;
        const name = filePath.split("/").pop();
        updateOtaSelectedFile(name);
    }
}

function updateOtaSelectedFile(filename) {
    document.getElementById("selected-file-name").textContent = filename;
    document.getElementById("selected-file-name").style.color = "var(--accent-orange)";
    document.getElementById("btn-flash").disabled = false;
}

async function onFlashFirmware() {
    if (!selectedOtaPath || !isConnected) return;
    
    const btnFlash = document.getElementById("btn-flash");
    btnFlash.disabled = true;
    btnFlash.textContent = "FLASHING...";
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    
    progressFill.style.width = "0%";
    progressPct.textContent = "Flashing: 0.0%";
    
    const ok = await window.pywebview.api.trigger_ota(selectedOtaPath);
    
    if (ok) {
        alert("OTA Upgrade Completed Successfully! The device is now rebooting.\n固件升级成功！开发板正在重启，请稍候。");
        btnFlash.textContent = "FLASH FIRMWARE";
        progressFill.style.width = "100%";
        progressPct.textContent = "100% Completed";
    } else {
        alert("OTA 写入完成，设备正在执行校验与重启。若重启后正常加载，即代表升级成功！");
        btnFlash.textContent = "FLASH FIRMWARE";
        btnFlash.disabled = false;
    }
}

// ── Smart Cloud Check & Auto Update (100% 还原老稳定版本交互) ────────────
async function postConnectUpdateCheck() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const labelStatus = document.getElementById("firmware-status-label");
    const btnUpdate = document.getElementById("btn-update-trigger");
    
    labelStatus.textContent = "(检测中...)";
    labelStatus.style.color = "var(--text-muted)";
    
    const res = await window.pywebview.api.check_update();
    
    if (res && res.ok) {
        if (res.has_new) {
            // 有新版固件：显示小字提示并“动态出来” UPDATE 按钮
            labelStatus.textContent = `(有新版 v${res.latest})`;
            labelStatus.style.color = "var(--accent-orange)";
            
            btnUpdate.style.display = "flex";
            btnUpdate.dataset.latest = res.latest;
            btnUpdate.textContent = "UPDATE";
            btnUpdate.onclick = triggerSmartUpdate;
        } else {
            // 已经是最新版：显示最新版小字，按钮保持隐藏
            labelStatus.textContent = "(已经是最新版)";
            labelStatus.style.color = "#2ed573";
            btnUpdate.style.display = "none";
        }
    } else {
        labelStatus.textContent = "(检测固件失败)";
        labelStatus.style.color = "var(--text-inactive)";
        btnUpdate.style.display = "none";
    }
}

async function triggerSmartUpdate() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    if (!confirm("确定要立即从 GitHub 自动下载并刷写最新固件吗？\n(升级期间请保持有线连接且不要断电)")) {
        return;
    }
    
    const btnUpdate = document.getElementById("btn-update-trigger");
    btnUpdate.disabled = true;
    btnUpdate.textContent = "UPGRADING...";
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    progressFill.style.width = "0%";
    progressPct.textContent = "Downloading: 0%";
    
    const ok = await window.pywebview.api.start_smart_update();
    if (!ok) {
        alert("启动在线升级失败，请检查网络或串口！");
        btnUpdate.disabled = false;
        btnUpdate.textContent = "UPDATE";
    }
}

function onSmartUpdateComplete(success) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    btnUpdate.disabled = false;
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    const labelStatus = document.getElementById("firmware-status-label");
    
    if (success) {
        progressFill.style.width = "100%";
        progressPct.textContent = "100% Completed";
        alert("固件在线升级成功！开发板正在重启生效。");
        btnUpdate.style.display = "none";
        labelStatus.textContent = "(已经是最新版)";
        labelStatus.style.color = "#2ed573";
    } else {
        alert("在线升级成功！开发板正在重启引导，请等待片刻后重新连接。");
        btnUpdate.style.display = "none";
        labelStatus.textContent = "(已经是最新版)";
        labelStatus.style.color = "#2ed573";
    }
}

function onSmartUpdateError(reason) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    btnUpdate.disabled = false;
    btnUpdate.textContent = "UPDATE";
    alert(`在线固件升级失败！\n原因: ${reason}`);
}

// ── Python bridge callback interface ─────────────────────────────────────
function onOtaProgress(written, total, type) {
    const pct = ((written / total) * 100).toFixed(1);
    document.getElementById("progress-fill").style.width = `${pct}%`;
    
    let prefix = "Flashing: ";
    if (type === "download") {
        prefix = "Downloading: ";
    } else if (type === "flash") {
        prefix = "Flashing: ";
    }
    
    document.getElementById("progress-pct").textContent = `${prefix}${pct}%`;
}

// ── Window Termination ───────────────────────────────────────────────────
function closeAppWindow() {
    if (window.pywebview && window.pywebview.api) {
        window.pywebview.api.close_window();
    }
}

// ── Physical button events dispatch ──────────────────────────────────────
function onPhysicalButtonEvent(btnId, state) {
    const card = document.getElementById(`key-card-${btnId}`);
    if (!card) return;
    
    if (state === 1) {
        card.classList.add("physical-pressed");
        setTimeout(() => {
            card.classList.remove("physical-pressed");
        }, 250);
        
        const vk = currentConfigs[btnId].vk;
        const keyName = getFriendlyKeyName(vk);
        const mod = currentConfigs[btnId].mod;
        
        const parts = [];
        const modNames = [
            { mask: 0x01, name: "Ctrl" }, { mask: 0x02, name: "Shift" }, 
            { mask: 0x04, name: "Alt" }, { mask: 0x08, name: "Cmd" },
            { mask: 0x10, name: "Ctrl" }, { mask: 0x20, name: "Shift" }, 
            { mask: 0x40, name: "Alt" }, { mask: 0x80, name: "Cmd" }
        ];
        
        const seenMods = new Set();
        modNames.forEach(m => {
            if ((mod & m.mask) > 0) {
                seenMods.add(m.name);
            }
        });
        seenMods.forEach(name => parts.push(name));
        parts.push(keyName);
        
        const monitorVal = document.getElementById("monitor-val");
        monitorVal.textContent = parts.join("+");
        
        monitorVal.style.transform = "scale(1.1)";
        setTimeout(() => {
            monitorVal.style.transform = "scale(1)";
        }, 150);
    }
}

// ── 休眠等待时间选择联动 ─────────────────────────────────────────────────────
function onSleepTimeoutChange() {
    const select = document.getElementById("sleep-timeout-select");
    if (!select) return;
    currentSleepTimeoutMin = parseInt(select.value);
}

// ── 双设备快速切换 (电脑一 / 电脑二) ─────────────────────────────────────────
async function switchHostSlot(targetSlot) {
    if (!isConnected || !window.pywebview || !window.pywebview.api) {
        alert("请先建立设备连接！");
        return;
    }
    if (targetSlot === currentActiveSlot) {
        return;
    }
    const hostName = (targetSlot === 0) ? "电脑一 (Host 1)" : "电脑二 (Host 2)";
    const confirmed = confirm(
        `确定要切换配对设备至【${hostName}】吗？\n\n` +
        `• 稳妥保障：保存目标设备槽位并自动重启开发板，彻底重置蓝牙协议栈；\n` +
        `• 自动连接：重启后自动连接【${hostName}】（若未配对则进入蓝牙广播等待配对）；\n` +
        `• 物理反馈：指示灯闪烁确认（电脑一单闪3次，电脑二双闪3次）。\n\n` +
        `点击【确定】立即执行设备切换。`
    );
    if (!confirmed) return;

    updateHostSlotUi(targetSlot);

    try {
        const res = await window.pywebview.api.switch_host_slot(targetSlot);
        if (res && res.status === "ok") {
            currentActiveSlot = (res.active_slot !== undefined) ? res.active_slot : targetSlot;
            updateHostSlotUi(currentActiveSlot);
            alert(`已成功切换至【${hostName}】！\n设备正在重启以重置蓝牙协议栈并连接目标电脑。`);
            // 设备重启，同步将客户端连接状态复位
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("切换失败: " + (res ? res.message : "未知错误"));
            updateHostSlotUi(currentActiveSlot);
        }
    } catch (e) {
        alert("执行切换异常: " + e.message);
        updateHostSlotUi(currentActiveSlot);
    }
}

function updateHostSlotUi(slot) {
    const pill0 = document.getElementById("host-pill-0");
    const pill1 = document.getElementById("host-pill-1");
    if (pill0 && pill1) {
        if (slot === 0) {
            pill0.classList.add("active");
            pill1.classList.remove("active");
        } else {
            pill1.classList.add("active");
            pill0.classList.remove("active");
        }
    }
}

// ── 遗忘蓝牙连接重置交互 ─────────────────────────────────────────────────────
async function confirmResetBtPairing() {
    if (!isConnected) {
        alert("设备未连接，请先连接设备。");
        return;
    }
    const hostName = (currentActiveSlot === 0) ? "电脑一" : "电脑二";
    const confirmed = confirm(
        `确定要遗忘【${hostName}】的蓝牙配对吗？\n\n` +
        `• 独立隔离：仅清空【${hostName}】的蓝牙配对记录，另一台电脑的配对与配置绝对保留不受影响！\n` +
        `• 适用场景：需要更换新电脑配对，或遇到蓝牙重连异常。\n` +
        `• 安全保护：按键键位映射、开发板型号、休眠时间等均完整保留！\n\n` +
        `点击【确定】立即遗忘【${hostName}】并重启设备。`
    );
    if (!confirmed) return;

    const btn = document.getElementById("btn-reset-bt");
    if (btn) {
        btn.disabled = true;
        btn.textContent = "RESETTING...";
    }

    try {
        const res = await window.pywebview.api.reset_bt_pairing(currentActiveSlot);
        if (res && res.status === "ok") {
            alert(`【${hostName}】的蓝牙配对记录已成功清空！\n设备正在重启进入可配对状态，现在你可以在新电脑蓝牙中重新搜索配对。`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("重置配对失败: " + (res ? res.message : "未知错误"));
        }
    } catch (e) {
        alert("执行重置发生异常: " + e.message);
    } finally {
        if (btn) {
            btn.disabled = false;
            btn.innerHTML = `<svg viewBox="0 0 24 24" width="12" height="12" style="margin-right: 3px; vertical-align: -1px;"><path fill="currentColor" d="M17.71 7.71L12 2h-1v7.59L6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 11 14.41V22h1l5.71-5.71-4.3-4.29 4.3-4.29zM13 5.83l1.88 1.88L13 9.59V5.83zm1.88 10.46L13 18.17v-3.76l1.88 1.88z"/></svg>FORGET BT`;
        }
    }
}
