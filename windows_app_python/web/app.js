// ==========================================================================
// VoxTriple Webview Frontend App Logic (OpenDesign 2-Panel - Windows Edition)
// ==========================================================================

// ── Virtual Key Code Map (Physical Keyboard mappings under Windows) ──────
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
let selectedKeyIdx = 0;       // 当前左侧聚焦编辑的按键下标 (0..3)
let isConnected = false;
let capturingIdx = -1;
let currentSleepTimeoutMin = 5;
let currentActiveSlot = 0;    // 当前处于活跃状态的电脑槽位 (0: 电脑一, 1: 电脑二)
let currentSlotPaired = [0, 0]; // 记录槽位是否已配对 (0: 未配对, 1: 已配对)
let currentSlotOs = [0, 0];   // 记录槽位配对系统 (1: Apple, 2: Windows, 0: 未知)
let selectedOtaPath = "";

const CURRENT_CLIENT_VERSION = "1.0.18";

// ── DOM Initialization ───────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", () => {
    // 串口连接按钮
    const btnConnect = document.getElementById("btn-connect");
    if (btnConnect) {
        btnConnect.addEventListener("click", onConnectClick);
        btnConnect.addEventListener("mouseenter", () => {
            if (isConnected) {
                const textEl = document.getElementById("btn-connect-text");
                if (textEl) textEl.textContent = "DISCONNECT";
            }
        });
        btnConnect.addEventListener("mouseleave", () => {
            if (isConnected) {
                const textEl = document.getElementById("btn-connect-text");
                if (textEl) textEl.textContent = "CONNECTED";
            }
        });
    }
    
    // 修饰键 Pills 点击委托
    const modPills = document.querySelectorAll(".mod-pill");
    modPills.forEach(pill => {
        pill.addEventListener("click", () => {
            const mask = parseInt(pill.getAttribute("data-mask"));
            toggleModForCurrent(mask);
        });
    });

    // 初始化为未连接重置态（隐藏左右面板、按键显示空白）
    resetConfigUi();

    // ── 焦点与键盘输入保护机制 ──────────────────────────────────────────
    // 1. 鼠标点击任何按钮后立即移除焦点，防止焦点停留
    document.addEventListener("click", () => {
        if (document.activeElement && document.activeElement.tagName === "BUTTON") {
            document.activeElement.blur();
        }
    });
    document.addEventListener("mouseup", () => {
        if (document.activeElement && document.activeElement.tagName === "BUTTON") {
            document.activeElement.blur();
        }
    });

    // 2. 全局捕获阶段拦截键盘事件：
    //    当不在按键录入状态时，ESP32（或物理键盘）发送的 Enter/Space 绝不会触发聚焦在按钮上的意外点击！
    document.addEventListener("keydown", (e) => {
        if (capturingIdx !== -1) return; // 正在录制按键时，交给 onCapturedKeydown 处理

        const active = document.activeElement;
        const isEditable = active && (
            active.tagName === "INPUT" || 
            active.tagName === "TEXTAREA" || 
            active.isContentEditable
        );

        if (!isEditable && (e.key === "Enter" || e.code === "Enter" || e.key === " " || e.code === "Space")) {
            if (active && typeof active.blur === "function") {
                active.blur();
            }
            e.preventDefault();
            e.stopPropagation();
        }
    }, true);

    // 3. 窗口失焦时清理物理按键按下态，防止极端通信丢包导致卡住
    window.addEventListener("blur", clearPhysicalPressedStates);

    // 轮询串口设备 (每 3 秒)
    setInterval(pollPorts, 3000);
    pollPorts();

    // 检查客户端软件在线更新（有新版本才显示左上角更新小按钮）
    checkClientAppUpdate();
    setTimeout(checkClientAppUpdate, 1500);
});

// ── Client App Upgrade Check & Link ───────────────────────────────────────
async function checkClientAppUpdate() {
    if (window.pywebview && window.pywebview.api && window.pywebview.api.check_client_update) {
        try {
            const res = await window.pywebview.api.check_client_update(CURRENT_CLIENT_VERSION);
            const btnUpgrade = document.getElementById("btn-app-upgrade");
            if (btnUpgrade) {
                if (res && res.has_new) {
                    btnUpgrade.style.display = "inline-flex";
                    btnUpgrade.title = `发现客户端新版本 v${res.latest}，点击前往下载更新`;
                } else {
                    btnUpgrade.style.display = "none";
                }
            }
        } catch (e) {
            console.warn("Check client update error:", e);
        }
    }
}

function openClientReleasePage() {
    if (window.pywebview && window.pywebview.api && window.pywebview.api.open_browser_url) {
        window.pywebview.api.open_browser_url("https://github.com/elementchen/VoxTriple_Classic/releases");
    } else {
        window.open("https://github.com/elementchen/VoxTriple_Classic/releases", "_blank");
    }
}

// ── Serial Port Polling ───────────────────────────────────────────────────
async function pollPorts() {
    if (window.pywebview && window.pywebview.api) {
        try {
            const ports = await window.pywebview.api.get_ports();
            updatePorts(ports);
        } catch (err) {
            console.error("Poll ports error:", err);
        }
    }
}

function updatePorts(ports) {
    const select = document.getElementById("port-select");
    if (!select) return;
    const currentVal = select.value;
    
    select.innerHTML = "";
    if (!ports || ports.length === 0) {
        select.innerHTML = '<option value="">No Devices</option>';
        return;
    }
    
    ports.forEach(port => {
        const opt = document.createElement("option");
        opt.value = port;
        opt.textContent = port;
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
    const textEl = document.getElementById("btn-connect-text");
    const btnWrite = document.getElementById("btn-write-config");
    
    if (!isConnected) {
        const portSelect = document.getElementById("port-select");
        const selectedPort = portSelect ? portSelect.value : "";
        if (!selectedPort) {
            alert("请选择有效的设备串口！");
            return;
        }
        
        if (textEl) textEl.textContent = "CONNECTING...";
        const ok = await window.pywebview.api.connect_device(selectedPort);
        if (ok) {
            isConnected = true;
            btn.classList.add("connected-state");
            if (textEl) textEl.textContent = "CONNECTED";
            
            // 展示左侧设置面板
            const settingsPanel = document.getElementById("settings-panel");
            if (settingsPanel) settingsPanel.classList.remove("panel-hidden");

            // 展示产品正下方的保存按钮并启用
            if (btnWrite) {
                btnWrite.classList.remove("panel-hidden");
                btnWrite.disabled = false;
            }
            
            // 读取设备配置
            const config = await window.pywebview.api.fetch_config();
            if (config) {
                renderConfig(config);
            }
            
            // 展示固件版本信息区块（位于底部 OTA 面板中，固定占位）
            const fwSection = document.getElementById("firmware-section");
            if (fwSection) fwSection.classList.remove("panel-hidden");

            // 显示多设备配对切换与重置按钮
            const hostSection = document.getElementById("host-switch-section");
            if (hostSection) hostSection.style.display = "inline-flex";
            const resetBtSection = document.getElementById("reset-bt-section");
            if (resetBtSection) resetBtSection.style.display = "inline-flex";
            
            // 静默检查固件更新
            postConnectUpdateCheck();
        } else {
            if (textEl) textEl.textContent = "CONNECT";
            alert("Connection failed. Please check device serial port or cable connection!");
        }
    } else {
        await window.pywebview.api.disconnect_device();
        applyDisconnectedUi();
    }
}

function applyDisconnectedUi() {
    isConnected = false;
    const btn = document.getElementById("btn-connect");
    const textEl = document.getElementById("btn-connect-text");
    const btnWrite = document.getElementById("btn-write-config");

    if (btn) btn.classList.remove("connected-state");
    if (textEl) textEl.textContent = "CONNECT";
    const fwVer = document.getElementById("firmware-ver");
    if (fwVer) fwVer.textContent = "v--";

    // 隐藏并禁用设置底部的保存按钮
    if (btnWrite) {
        btnWrite.classList.add("panel-hidden");
        btnWrite.disabled = true;
    }

    clearPhysicalPressedStates();

    // 隐藏升级触发按钮与状态
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) btnUpdate.style.display = "none";
    const labelStatus = document.getElementById("firmware-status-label");
    if (labelStatus) labelStatus.textContent = "";

    // 隐藏设备信息胶囊（保持占位隐藏）
    const fwSection = document.getElementById("firmware-section");
    if (fwSection) fwSection.classList.add("panel-hidden");
    const hostSection = document.getElementById("host-switch-section");
    if (hostSection) hostSection.style.display = "none";
    const resetBtSection = document.getElementById("reset-bt-section");
    if (resetBtSection) resetBtSection.style.display = "none";
    resetConfigUi();
}

// ── Render Config Cache ──────────────────────────────────────────────────
function renderConfig(config) {
    if (!config) return;

    // 固件版本
    const fwEl = document.getElementById("firmware-ver");
    if (fwEl) fwEl.textContent = "v" + (config.version || "1.0.18");

    // 麦克风与休眠模式开关
    const micToggle = document.getElementById("mic-toggle");
    if (micToggle) {
        micToggle.checked = (config.mic_enabled === 1);
        onMicToggle();
    }
    const sleepToggle = document.getElementById("sleep-toggle");
    if (sleepToggle) sleepToggle.checked = (config.sleep_mode === 1);

    // 休眠等待时间
    if (config.sleep_timeout_min !== undefined) {
        currentSleepTimeoutMin = config.sleep_timeout_min;
        const sleepSelect = document.getElementById("sleep-timeout-select");
        if (sleepSelect) {
            sleepSelect.value = String(config.sleep_timeout_min);
        }
    }

    // 双设备配对状态 (电脑一 / 电脑二)
    if (config.slot0_paired !== undefined) currentSlotPaired[0] = config.slot0_paired;
    if (config.slot1_paired !== undefined) currentSlotPaired[1] = config.slot1_paired;
    if (config.slot0_os !== undefined) currentSlotOs[0] = config.slot0_os;
    if (config.slot1_os !== undefined) currentSlotOs[1] = config.slot1_os;
    if (config.active_slot !== undefined) {
        currentActiveSlot = config.active_slot;
        updateHostSlotUi(currentActiveSlot);
    }

    // 更新槽位指示圆点 (绿色:已配对, 橙色:未配对) 与系统提示
    const dot0 = document.getElementById("host-dot-0");
    const pill0 = document.getElementById("host-pill-0");
    const os0Name = (config.slot0_os === 1) ? " · macOS" : (config.slot0_os === 2) ? " · Windows" : "";
    if (dot0 && pill0) {
        if (config.slot0_paired === 1) {
            dot0.className = "host-status-dot paired";
            pill0.title = `Pair 1 (Paired${os0Name})`;
        } else {
            dot0.className = "host-status-dot empty";
            pill0.title = "Pair 1 (Unpaired / Empty)";
        }
    }

    const dot1 = document.getElementById("host-dot-1");
    const pill1 = document.getElementById("host-pill-1");
    const os1Name = (config.slot1_os === 1) ? " · macOS" : (config.slot1_os === 2) ? " · Windows" : "";
    if (dot1 && pill1) {
        if (config.slot1_paired === 1) {
            dot1.className = "host-status-dot paired";
            pill1.title = `Pair 2 (Paired${os1Name})`;
        } else {
            dot1.className = "host-status-dot empty";
            pill1.title = "Pair 2 (Unpaired / Empty)";
        }
    }

    // 发射功率等级
    setTxPowerUi(config.tx_power !== undefined ? config.tx_power : 4);
    
    // 更新 4 个按键数据
    if (config.mappings && config.mappings.length >= 4) {
        for (let i = 0; i < 4; i++) {
            currentConfigs[i] = {
                vk: config.mappings[i].vk,
                mod: config.mappings[i].mod
            };
            updateKeyCapVisual(i);
        }
    }

    // 刷新当前选中的编辑卡片
    refreshCurrentKeyEditor();
}

// ── Key Selection & Switch ───────────────────────────────────────────────
function selectKey(idx) {
    if (!isConnected) {
        alert("请先连接硬件设备后再配置按键！");
        return;
    }
    if (idx < 0 || idx > 3) return;
    selectedKeyIdx = idx;

    // 更新 4 个键帽高亮内发光样式
    for (let i = 0; i < 4; i++) {
        const cap = document.getElementById(`key-cap-${i}`);
        if (cap) {
            if (i === idx) {
                cap.classList.add("active");
            } else {
                cap.classList.remove("active");
            }
        }
    }

    // 显示右侧 KEY MAPPING 面板
    const configPanel = document.getElementById("config-panel");
    if (configPanel) {
        configPanel.classList.remove("panel-hidden");
    }

    // 更新按键序号
    const keyIndexEl = document.getElementById("currentKeyIndex");
    if (keyIndexEl) {
        keyIndexEl.textContent = `KEY ${idx + 1}`;
    }

    // 刷新右侧编辑内容
    refreshCurrentKeyEditor();

    // ★ 点击按键后自动进入 Capture 状态，无需再次点击小小的 capture 按钮
    triggerCurrentKeyCapture();
}

function refreshCurrentKeyEditor() {
    const config = currentConfigs[selectedKeyIdx] || { vk: 0, mod: 0 };
    const displayEl = document.getElementById("keyDisplay");
    const subEl = document.getElementById("captureSub");
    const captureView = document.getElementById("captureView");

    if (captureView) {
        captureView.classList.remove("listening");
    }
    if (subEl) {
        subEl.textContent = "Click box to record · Press key";
    }

    if (displayEl) {
        displayEl.textContent = formatComboKeyString(config.vk, config.mod);
    }

    // 刷新修饰键激活状态
    const pills = document.querySelectorAll(".mod-pill");
    pills.forEach(pill => {
        const mask = parseInt(pill.getAttribute("data-mask"));
        if ((config.mod & mask) > 0) {
            pill.classList.add("active");
        } else {
            pill.classList.remove("active");
        }
    });
}

function updateKeyCapVisual(idx) {
    const config = currentConfigs[idx];
    if (!config) return;

    const valEl = document.getElementById(`cap-val-${idx}`);
    const modsEl = document.getElementById(`cap-mods-${idx}`);

    if (!isConnected) {
        // 未连接时：按键表面完全洁白平整空白
        if (valEl) valEl.textContent = "";
        if (modsEl) modsEl.textContent = "";
        return;
    }

    if (valEl) {
        valEl.textContent = config.vk === 0 ? "--" : getFriendlyKeyName(config.vk);
    }
    if (modsEl) {
        modsEl.textContent = formatModifiersShort(config.mod);
    }
}

// ── Modifiers Pill Toggle ─────────────────────────────────────────────────
function toggleModForCurrent(mask) {
    if (!isConnected) {
        alert("请先连接硬件设备！");
        return;
    }

    const currentMod = currentConfigs[selectedKeyIdx].mod;
    const newMod = currentMod ^ mask; // 翻转掩码
    currentConfigs[selectedKeyIdx].mod = newMod;

    // 更新左侧主显与修饰键按钮
    refreshCurrentKeyEditor();

    // 同步更新右侧键帽修饰键微标
    updateKeyCapVisual(selectedKeyIdx);
}

// ── Friendly Key String Formatting ───────────────────────────────────────
function formatComboKeyString(vk, mod) {
    if (vk === 0 && mod === 0) return "--";

    const parts = [];
    const modNames = [
        { mask: 0x01, name: "LCtrl" }, { mask: 0x02, name: "LShift" }, 
        { mask: 0x04, name: "LAlt" }, { mask: 0x08, name: "LWin" },
        { mask: 0x10, name: "RCtrl" }, { mask: 0x20, name: "RShift" }, 
        { mask: 0x40, name: "RAlt" }, { mask: 0x80, name: "RWin" }
    ];

    modNames.forEach(m => {
        if ((mod & m.mask) > 0) {
            parts.push(m.name);
        }
    });

    if (vk !== 0) {
        parts.push(getFriendlyKeyName(vk));
    }

    return parts.length > 0 ? parts.join(" + ") : "--";
}

function formatModifiersShort(mod) {
    if (!mod) return "";
    const badges = [];
    if (mod & 0x01) badges.push("LCtl");
    if (mod & 0x02) badges.push("LShf");
    if (mod & 0x04) badges.push("LAlt");
    if (mod & 0x08) badges.push("LWin");
    if (mod & 0x10) badges.push("RCtl");
    if (mod & 0x20) badges.push("RShf");
    if (mod & 0x40) badges.push("RAlt");
    if (mod & 0x80) badges.push("RWin");
    return badges.join(" ");
}

function getFriendlyKeyName(vk) {
    if (vk === 0) return "--";
    if (VK_TO_NAME[vk]) return VK_TO_NAME[vk];
    if (vk >= 0x30 && vk <= 0x39) return String.fromCharCode(vk);
    if (vk >= 0x41 && vk <= 0x5A) return String.fromCharCode(vk);
    return `0x${vk.toString(16).toUpperCase()}`;
}

function resetConfigUi() {
    selectedKeyIdx = -1;
    capturingIdx = -1;
    document.removeEventListener("keydown", onCapturedKeydown);
    clearPhysicalPressedStates();
    currentSlotPaired = [0, 0];
    currentSlotOs = [0, 0];
    updateHostSlotUi(0);

    for (let i = 0; i < 4; i++) {
        currentConfigs[i] = { vk: 0, mod: 0 };
        const cap = document.getElementById(`key-cap-${i}`);
        if (cap) cap.classList.remove("active");
        const valEl = document.getElementById(`cap-val-${i}`);
        if (valEl) valEl.textContent = "";
        const modsEl = document.getElementById(`cap-mods-${i}`);
        if (modsEl) modsEl.textContent = "";
    }

    // 隐藏左侧与右侧面板（保持占位，位置不移动）
    const settingsPanel = document.getElementById("settings-panel");
    if (settingsPanel) settingsPanel.classList.add("panel-hidden");
    const configPanel = document.getElementById("config-panel");
    if (configPanel) configPanel.classList.add("panel-hidden");
    const btnWrite = document.getElementById("btn-write-config");
    if (btnWrite) {
        btnWrite.classList.add("panel-hidden");
        btnWrite.disabled = true;
    }

    const micToggle = document.getElementById("mic-toggle");
    if (micToggle) micToggle.checked = false;
    onMicToggle();

    const sleepToggle = document.getElementById("sleep-toggle");
    if (sleepToggle) sleepToggle.checked = false;
    setTxPowerUi(4);
}

// ── TX Power UI ──────────────────────────────────────────────────────────
function setTxPowerUi(level) {
    const dbmMap = ["-12 dBm", "-9 dBm", "-6 dBm", "-3 dBm", "Standard", "+3 dBm", "+6 dBm", "+9 dBm"];
    const valEl = document.getElementById("tx-power-val");
    if (valEl) {
        valEl.textContent = dbmMap[level] !== undefined ? dbmMap[level] : "Standard";
    }
    
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

function toggleMicFromDevice() {
    if (!isConnected) {
        alert("请先连接硬件设备后再配置麦克风！");
        return;
    }
    const micToggle = document.getElementById("mic-toggle");
    if (!micToggle) return;
    micToggle.checked = !micToggle.checked;
    onMicToggle();
}

function onMicToggle() {
    // 麦克风硬件指示与开关双向联动（仅切换高亮，文字保持 MIC）
    const slot = document.getElementById("virtualMicSlot");
    const micToggle = document.getElementById("mic-toggle");
    if (slot && micToggle) {
        if (micToggle.checked) {
            slot.classList.add("active");
        } else {
            slot.classList.remove("active");
        }
    }
}

function onSleepToggle() {
    // 本地状态变更，点击 WRITE TO ESP32 批量写入设备
}

function onSleepTimeoutChange() {
    const select = document.getElementById("sleep-timeout-select");
    if (select) {
        currentSleepTimeoutMin = parseInt(select.value);
    }
}

// ── Save All Configs to Device Flash ─────────────────────────────────────
async function saveAllConfigsToDevice() {
    if (!isConnected || !window.pywebview || !window.pywebview.api) return;
    
    const hostName = (currentActiveSlot === 0) ? "Pair 1" : "Pair 2";
    const btnWrite = document.getElementById("btn-write-config");
    const origHtml = btnWrite.innerHTML;
    btnWrite.disabled = true;
    btnWrite.innerHTML = "<span>SAVING & REBOOTING...</span>";
    
    const mic = document.getElementById("mic-toggle").checked ? 1 : 0;
    const sleep = document.getElementById("sleep-toggle").checked ? 1 : 0;
    
    const activeSegments = document.querySelectorAll("#tx-power-bar .power-segment.active");
    const tx = Math.max(0, activeSegments.length - 1);
    
    const sleepSelect = document.getElementById("sleep-timeout-select");
    const sleepTimeout = sleepSelect ? parseInt(sleepSelect.value) : 5;
    
    try {
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
            alert(`Configurations successfully saved to [${hostName}]!\nDevice is rebooting to apply new parameters.`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("Failed to save configuration. Please check serial connection!");
        }
    } catch (e) {
        btnWrite.disabled = false;
        btnWrite.innerHTML = origHtml;
        alert("Exception saving configuration: " + e.message);
    }
}

// ── Key Capture Mechanism ────────────────────────────────────────────────
function triggerCurrentKeyCapture() {
    if (!isConnected) {
        alert("请先连接硬件设备后再捕获按键！");
        return;
    }
    
    if (capturingIdx !== -1) return; // 正在捕获中
    
    capturingIdx = selectedKeyIdx;
    
    const captureView = document.getElementById("captureView");
    const display = document.getElementById("keyDisplay");
    const sub = document.getElementById("captureSub");
    
    if (captureView) captureView.classList.add("listening");
    if (display) display.textContent = "?";
    if (sub) sub.textContent = "PRESS ANY KEY NOW...";
    
    // 监听键盘原生键击
    document.addEventListener("keydown", onCapturedKeydown);
}

function onCapturedKeydown(e) {
    e.preventDefault();
    e.stopPropagation();
    
    const code = e.code;
    const vk = JS_CODE_TO_VK[code];
    
    if (vk !== undefined) {
        // 立即解绑
        document.removeEventListener("keydown", onCapturedKeydown);
        
        const btnIdx = capturingIdx;
        capturingIdx = -1;
        
        // 保持现有修饰键，更新主键码
        currentConfigs[btnIdx].vk = vk;
        
        // 更新界面
        refreshCurrentKeyEditor();
        updateKeyCapVisual(btnIdx);
    } else {
        alert(`未识别或不支持的按键码: ${code}`);
    }
}

// ── Physical Button Event Hook ───────────────────────────────────────────
// 如实响应硬件真实按下的 press (state=1) 与释放的 release (state=0)
function onPhysicalButtonEvent(btnId, state) {
    const cap = document.getElementById(`key-cap-${btnId}`);
    if (!cap) return;
    
    if (state === 1) {
        cap.classList.add("physical-pressed");
    } else if (state === 0) {
        cap.classList.remove("physical-pressed");
    }
}

function clearPhysicalPressedStates() {
    for (let i = 0; i < 4; i++) {
        const cap = document.getElementById(`key-cap-${i}`);
        if (cap) cap.classList.remove("physical-pressed");
    }
}

// ── 双设备快速切换 (电脑一 / 电脑二) ─────────────────────────────────────────
async function switchHostSlot(targetSlot) {
    if (!isConnected || !window.pywebview || !window.pywebview.api) {
        alert("Please connect the device first!");
        return;
    }
    if (targetSlot === currentActiveSlot) {
        return;
    }
    const hostName = (targetSlot === 0) ? "Pair 1" : "Pair 2";
    const confirmed = confirm(
        `Switch active pairing slot to [${hostName}]?\n\n` +
        `• Safe & Reliable: Target slot saved and device auto-reboots to reinitialize Bluetooth stack;\n` +
        `• Auto Connect: Automatically connects to [${hostName}] upon reboot (enters discoverable mode if unpaired);\n` +
        `• Physical Feedback: LED confirms (Pair 1 blinks 3x single, Pair 2 blinks 3x double).\n\n` +
        `Click [OK] to proceed with slot switching.`
    );
    if (!confirmed) return;

    updateHostSlotUi(targetSlot);

    try {
        const res = await window.pywebview.api.switch_host_slot(targetSlot);
        if (res && res.status === "ok") {
            currentActiveSlot = (res.active_slot !== undefined) ? res.active_slot : targetSlot;
            updateHostSlotUi(currentActiveSlot);
            alert(`Switched to [${hostName}] successfully!\nDevice is rebooting to apply Bluetooth parameters.`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("Switch failed: " + (res ? res.message : "Unknown error"));
            updateHostSlotUi(currentActiveSlot);
        }
    } catch (e) {
        alert("Switch exception: " + e.message);
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

    const watermark = document.getElementById("pair-watermark-text");
    if (watermark) {
        watermark.textContent = (slot === 1) ? "PAIR 2" : "PAIR 1";
    }

    updatePairWatermarkIcon(slot);
}

function updatePairWatermarkIcon(slot) {
    const iconEl = document.getElementById("pair-watermark-os-icon");
    if (!iconEl) return;

    const isPaired = (slot === 0) ? currentSlotPaired[0] : currentSlotPaired[1];
    const osType = (slot === 0) ? currentSlotOs[0] : currentSlotOs[1];

    if (!isConnected || !isPaired || !osType) {
        // 未连接、或者该频道未配对、或者未识别系统：透明占位，不显示图标
        iconEl.style.visibility = "hidden";
        iconEl.innerHTML = "";
        return;
    }

    // 已配对且已识别系统：展示浅灰系统Logo
    iconEl.style.visibility = "visible";
    if (osType === 1) { // Apple (macOS)
        iconEl.innerHTML = `<svg viewBox="0 0 170 170" width="18" height="18" fill="currentColor">
            <path d="M150.37 130.25c-2.45 5.66-5.35 10.87-8.71 15.66-4.58 6.53-8.33 11.05-11.22 13.56-4.48 4.12-9.28 6.23-14.42 6.35-3.69 0-8.14-1.05-13.32-3.18-5.19-2.12-9.97-3.17-14.34-3.17-4.58 0-9.49 1.05-14.75 3.17-5.26 2.13-9.5 3.24-12.74 3.35-4.35.13-9.16-1.9-14.42-6.08-3.7-3.04-7.7-7.85-12.01-14.42-5.74-8.81-10.23-18.7-13.48-29.68-3.24-10.97-4.87-21.78-4.87-32.43 0-16.32 4.11-29.74 12.33-40.27 8.22-10.53 18.7-15.86 31.44-15.99 6.29 0 12.82 1.58 19.59 4.75 6.77 3.17 11.22 4.88 13.34 5.12 2.7-.49 7.4-2.31 14.1-5.48 6.7-3.16 12.78-4.66 18.24-4.5 14.15.52 25.13 6.13 32.93 16.83-12.63 7.64-18.76 18.04-18.39 31.22.37 10.37 4.26 19.06 11.66 26.06 7.41 7 16.14 10.88 26.2 11.64-2.22 6.64-4.87 13.06-7.96 19.26zM119.22 33.15c0-7.72 2.76-14.97 8.29-21.75 5.53-6.77 12.28-10.9 20.25-12.4 1.1 6.96-.34 14.11-4.33 21.46-3.99 7.34-9.87 12.08-17.64 14.22-.88-.49-2.07-.86-3.57-1.12-1.5-.27-2.5-.41-3-.41z"/>
        </svg>`;
    } else if (osType === 2) { // Windows PC
        iconEl.innerHTML = `<svg viewBox="0 0 24 24" width="17" height="17" fill="currentColor">
            <path d="M0 3.449L9.75 2.1v9.451H0V3.449zm10.949-1.551L24 0v11.551H10.949V1.898zM0 12.45h9.75V21.9L0 20.551V12.45zm10.949 0H24V24l-13.051-1.898V12.45z"/>
        </svg>`;
    }
}

// ── 取消蓝牙配对重置交互 ─────────────────────────────────────────────────────
async function confirmResetBtPairing() {
    if (!isConnected) {
        alert("Device is not connected. Please connect first.");
        return;
    }
    const hostName = (currentActiveSlot === 0) ? "Pair 1" : "Pair 2";
    const confirmed = confirm(
        `Unpair and clear Bluetooth pairing for [${hostName}]?\n\n` +
        `• Slot Isolation: Only clears pairing for [${hostName}], the other pair remains unaffected!\n` +
        `• Key mappings and sleep settings are fully preserved!\n\n` +
        `Click [OK] to unpair [${hostName}] and restart device.`
    );
    if (!confirmed) return;

    const btn = document.getElementById("btn-reset-bt");
    if (btn) {
        btn.disabled = true;
        btn.textContent = "UNPAIRING...";
    }

    try {
        const res = await window.pywebview.api.reset_bt_pairing(currentActiveSlot);
        if (res && res.status === "ok") {
            alert(`Pairing for [${hostName}] cleared successfully!\nDevice is rebooting in discoverable mode.`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("Unpair failed: " + (res ? res.message : "Unknown error"));
        }
    } catch (e) {
        alert("Unpair exception: " + e.message);
    } finally {
        if (btn) {
            btn.disabled = false;
            btn.innerHTML = `<svg viewBox="0 0 24 24" width="11" height="11"><path fill="currentColor" d="M17.71 7.71L12 2h-1v7.59L6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 11 14.41V22h1l5.71-5.71-4.3-4.29 4.3-4.29zM13 5.83l1.88 1.88L13 9.59V5.83zm1.88 10.46L13 18.17v-3.76l1.88 1.88z"/></svg> UNPAIR`;
        }
    }
}

// ── OTA Firmware Flashing ────────────────────────────────────────────────
async function onLocalBinUpgrade() {
    if (!window.pywebview || !window.pywebview.api || !isConnected) {
        alert("请先连接硬件设备！");
        return;
    }
    
    const filePath = await window.pywebview.api.select_local_bin();
    if (!filePath) return;
    
    selectedOtaPath = filePath;
    const filename = filePath.split("/").pop();
    document.getElementById("selected-file-name").textContent = filename;
    document.getElementById("selected-file-name").style.color = "var(--accent-orange)";
    
    if (confirm(`准备刷入本地固件：\n${filename}\n\n确认立即开始刷写？`)) {
        onFlashFirmware();
    }
}

async function onFlashFirmware() {
    if (!selectedOtaPath || !isConnected) return;
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    const btnLocal = document.querySelector(".btn-ota-local");
    const btnCloud = document.querySelector(".btn-ota-cloud");
    
    if (btnLocal) btnLocal.disabled = true;
    if (btnCloud) btnCloud.disabled = true;

    if (progressFill) progressFill.style.width = "0%";
    if (progressPct) progressPct.textContent = "Flashing: 0.0%";
    
    try {
        const ok = await window.pywebview.api.trigger_ota(selectedOtaPath);
        
        if (ok) {
            if (progressFill) progressFill.style.width = "100%";
            if (progressPct) progressPct.textContent = "100.0% Done";
            // 升级完成ESP32重启，断开连接重置UI状态
            applyDisconnectedUi();
            alert("固件升级成功！开发板正在执行引导重启，请点击 CONNECT 重新连接。");
        } else {
            alert("固件写入失败或被取消，请检查串口与供电后重试。");
            if (progressPct) progressPct.textContent = "Failed";
        }
    } catch (e) {
        alert("固件升级过程异常: " + e.message);
        if (progressPct) progressPct.textContent = "Failed";
    } finally {
        if (btnLocal) btnLocal.disabled = false;
        if (btnCloud) btnCloud.disabled = false;
    }
}

// ── Smart Cloud Check & Auto Update ──────────────────────────────────────
async function postConnectUpdateCheck() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const labelStatus = document.getElementById("firmware-status-label");
    const btnUpdate = document.getElementById("btn-update-trigger");
    
    if (labelStatus) {
        labelStatus.textContent = "(Checking...)";
        labelStatus.style.color = "var(--fg-muted)";
    }
    
    const res = await window.pywebview.api.check_update();
    
    if (res && res.ok) {
        if (res.has_new) {
            if (labelStatus) {
                labelStatus.textContent = `(New: v${res.latest})`;
                labelStatus.style.color = "var(--accent-orange)";
            }
            if (btnUpdate) {
                btnUpdate.style.display = "inline-flex";
                btnUpdate.dataset.latest = res.latest;
                btnUpdate.textContent = "UPDATE";
            }
        } else {
            if (labelStatus) {
                labelStatus.textContent = "(Latest)";
                labelStatus.style.color = "var(--status-green)";
            }
            if (btnUpdate) btnUpdate.style.display = "none";
        }
    } else {
        if (labelStatus) {
            labelStatus.textContent = "";
        }
        if (btnUpdate) btnUpdate.style.display = "none";
    }
}

async function triggerSmartUpdate() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    if (!confirm("Download and flash the latest official firmware from GitHub?\n(Keep device connected and do not power off)")) {
        return;
    }
    
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) {
        btnUpdate.disabled = true;
        btnUpdate.textContent = "UPGRADING...";
    }
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    if (progressFill) progressFill.style.width = "0%";
    if (progressPct) progressPct.textContent = "Downloading: 0%";
    
    const ok = await window.pywebview.api.start_smart_update();
    if (!ok) {
        alert("Failed to start online update. Please check network and serial connection!");
        if (btnUpdate) {
            btnUpdate.disabled = false;
            btnUpdate.textContent = "UPDATE";
        }
    }
}

function onSmartUpdateComplete(success) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) {
        btnUpdate.disabled = false;
        btnUpdate.style.display = "none";
    }
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    
    if (progressFill) progressFill.style.width = "100%";
    if (progressPct) progressPct.textContent = "100.0% Done";
    
    // 升级完成ESP32重启，断开连接重置UI状态
    applyDisconnectedUi();
    alert("Firmware upgraded successfully! The device is rebooting, please click CONNECT to reconnect.\n固件升级成功！开发板正在执行引导重启，请点击 CONNECT 重新连接。");
}

function onSmartUpdateError(reason) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) {
        btnUpdate.disabled = false;
        btnUpdate.textContent = "UPDATE";
    }
    const progressPct = document.getElementById("progress-pct");
    if (progressPct) progressPct.textContent = "Failed";
    alert(`Firmware upgrade failed!\nReason: ${reason}`);
}

function onOtaProgress(written, total, type) {
    if (!total || total <= 0) return;
    const pct = Math.min(100, Math.max(0, (written / total) * 100)).toFixed(1);
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    if (progressFill) progressFill.style.width = `${pct}%`;
    
    let prefix = "Flashing: ";
    if (type === "download") prefix = "Downloading: ";
    if (progressPct) progressPct.textContent = `${prefix}${pct}%`;
}
