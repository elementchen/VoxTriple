// ==========================================================================
// VoxTriple Webview Frontend App Logic (OpenDesign Modernized)
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
    "ShiftLeft": 0xA0, "ShiftRight": 0xA1,
    "ControlLeft": 0xA2, "ControlRight": 0xA3,
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
    0xA4: "LOption", 0xA5: "ROption",
    0x5B: "LCmd", 0x5C: "RCmd"
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

// ── DOM Initialization ───────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", () => {
    // 串口连接按钮
    const btnConnect = document.getElementById("btn-connect");
    if (btnConnect) {
        btnConnect.addEventListener("click", onConnectClick);
    }
    
    // 修饰键 Pills 点击委托
    const modPills = document.querySelectorAll(".mod-pill");
    modPills.forEach(pill => {
        pill.addEventListener("click", () => {
            const mask = parseInt(pill.getAttribute("data-mask"));
            toggleModForCurrent(mask);
        });
    });

    // 拖拽升级 bin 文件处理
    const dropzone = document.getElementById("dropzone");
    if (dropzone) {
        dropzone.addEventListener("dragover", (e) => {
            e.preventDefault();
            dropzone.classList.add("dragover");
        });
        dropzone.addEventListener("dragleave", () => {
            dropzone.classList.remove("dragover");
        });
        dropzone.addEventListener("drop", async (e) => {
            e.preventDefault();
            dropzone.classList.remove("dragover");
            if (e.dataTransfer.files.length > 0) {
                const file = e.dataTransfer.files[0];
                if (file.name.endsWith(".bin")) {
                    const filePath = file.path || file.name;
                    triggerLocalBinDrop(filePath, file.name);
                } else {
                    alert("请拖入有效的 .bin 固件文件！");
                }
            }
        });
    }

    // 初始化按键 0 选中态
    selectKey(0);

    // 轮询串口设备 (每 3 秒)
    setInterval(pollPorts, 3000);
    pollPorts();
});

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
    const connBadge = document.getElementById("connBadge");
    const connText = document.getElementById("connText");
    
    if (!isConnected) {
        const portSelect = document.getElementById("port-select");
        const selectedPort = portSelect ? portSelect.value : "";
        if (!selectedPort) {
            alert("请选择有效的设备串口！");
            return;
        }
        
        btn.textContent = "CONNECTING...";
        const ok = await window.pywebview.api.connect_device(selectedPort);
        if (ok) {
            isConnected = true;
            btn.textContent = "DISCONNECT";
            btn.classList.add("connected-state");
            if (connBadge) {
                connBadge.className = "status-badge connected";
            }
            if (connText) {
                connText.textContent = "Connected";
            }
            
            // 启用保存按钮
            if (btnWrite) btnWrite.disabled = false;
            
            // 读取设备配置
            const config = await window.pywebview.api.fetch_config();
            if (config) {
                renderConfig(config);
            }
            
            // 检查固件更新
            postConnectUpdateCheck();
        } else {
            btn.textContent = "CONNECT";
            alert("连接失败，请检查串口是否已被占用或设备未插好！");
        }
    } else {
        await window.pywebview.api.disconnect_device();
        isConnected = false;
        btn.textContent = "CONNECT";
        btn.classList.remove("connected-state");
        if (connBadge) {
            connBadge.className = "status-badge disconnected";
        }
        if (connText) {
            connText.textContent = "Disconnected";
        }
        document.getElementById("firmware-ver").textContent = "v--";
        
        // 禁用保存按钮
        if (btnWrite) btnWrite.disabled = true;
        
        // 隐藏升级触发按钮与提示
        const btnUpdate = document.getElementById("btn-update-trigger");
        if (btnUpdate) btnUpdate.style.display = "none";
        const labelStatus = document.getElementById("firmware-status-label");
        if (labelStatus) labelStatus.textContent = "";
        
        // 折叠 OTA 面板
        toggleOtaPanel(false);
        
        resetConfigUi();
    }
}

// ── Render Config Cache ──────────────────────────────────────────────────
function renderConfig(config) {
    if (!config) return;

    // 固件版本
    const fwEl = document.getElementById("firmware-ver");
    if (fwEl) fwEl.textContent = "v" + (config.version || "1.0.17");

    // 麦克风与深度休眠开关
    const micToggle = document.getElementById("mic-toggle");
    if (micToggle) micToggle.checked = (config.mic_enabled === 1);
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

    // 发射功率
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
    if (idx < 0 || idx > 3) return;
    selectedKeyIdx = idx;

    // 更新右侧键帽选中样式
    for (let i = 0; i < 4; i++) {
        const cap = document.getElementById(`key-cap-${i}`);
        if (cap) {
            if (i === idx) {
                cap.classList.add("selected");
            } else {
                cap.classList.remove("selected");
            }
        }
    }

    // 更新左侧面板徽章
    const badge = document.getElementById("selectedKeyBadge");
    if (badge) {
        badge.textContent = `KEY 0${idx + 1}`;
    }

    // 刷新左侧编辑内容
    refreshCurrentKeyEditor();
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
        subEl.textContent = "CLICK TO CAPTURE";
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
        { mask: 0x04, name: "LOption" }, { mask: 0x08, name: "LCmd" },
        { mask: 0x10, name: "RCtrl" }, { mask: 0x20, name: "RShift" }, 
        { mask: 0x40, name: "ROption" }, { mask: 0x80, name: "RCmd" }
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
    if (mod & 0x04) badges.push("LOpt");
    if (mod & 0x08) badges.push("LCmd");
    if (mod & 0x10) badges.push("RCtl");
    if (mod & 0x20) badges.push("RShf");
    if (mod & 0x40) badges.push("ROpt");
    if (mod & 0x80) badges.push("RCmd");
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
    for (let i = 0; i < 4; i++) {
        currentConfigs[i] = { vk: 0, mod: 0 };
        updateKeyCapVisual(i);
    }
    refreshCurrentKeyEditor();
    const micToggle = document.getElementById("mic-toggle");
    if (micToggle) micToggle.checked = false;
    const sleepToggle = document.getElementById("sleep-toggle");
    if (sleepToggle) sleepToggle.checked = false;
    setTxPowerUi(0);
}

// ── TX Power UI ──────────────────────────────────────────────────────────
function setTxPowerUi(level) {
    const dbmMap = ["-12", "-9", "-6", "-3", "0", "3", "6", "9"];
    const valEl = document.getElementById("tx-power-val");
    if (valEl) {
        valEl.textContent = `${dbmMap[level] || "0"} dBm`;
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

// ── Mic & Sleep Toggles ──────────────────────────────────────────────────
function onMicToggle() {
    // 实时更新视觉联动
    const slot = document.getElementById("virtualMicSlot");
    const micToggle = document.getElementById("mic-toggle");
    if (slot && micToggle) {
        if (micToggle.checked) {
            slot.style.borderColor = "rgba(255, 122, 0, 0.4)";
        } else {
            slot.style.borderColor = "var(--border-subtle)";
        }
    }
}

function onSleepToggle() {
    // 本地开关状态，点击 SAVE CONFIG 写入设备
}

// ── Save All Configs to Device Flash ─────────────────────────────────────
async function saveAllConfigsToDevice() {
    if (!isConnected || !window.pywebview || !window.pywebview.api) return;
    
    const btnWrite = document.getElementById("btn-write-config");
    const origText = btnWrite.textContent;
    btnWrite.disabled = true;
    btnWrite.textContent = "WRITING...";
    
    const mic = document.getElementById("mic-toggle").checked ? 1 : 0;
    const sleep = document.getElementById("sleep-toggle").checked ? 1 : 0;
    
    const activeSegments = document.querySelectorAll("#tx-power-bar .power-segment.active");
    const tx = Math.max(0, activeSegments.length - 1);
    
    try {
        const ok = await window.pywebview.api.write_config(
            currentConfigs, 
            tx, 
            sleep, 
            mic
        );
        
        btnWrite.disabled = false;
        btnWrite.textContent = origText;
        
        if (ok) {
            alert("配置已成功写入设备 Flash 闪存！\nConfiguration saved successfully.");
        } else {
            alert("配置写入失败，请检查串口连接状态！");
        }
    } catch (e) {
        btnWrite.disabled = false;
        btnWrite.textContent = origText;
        alert("写入配置异常: " + e.message);
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
        
        // 保持现有修饰键，更新按键码
        currentConfigs[btnIdx].vk = vk;
        
        // 更新界面
        refreshCurrentKeyEditor();
        updateKeyCapVisual(btnIdx);
    } else {
        alert(`未识别或不支持的按键码: ${code}`);
    }
}

// ── Physical Button Event Hook ───────────────────────────────────────────
function onPhysicalButtonEvent(btnId, state) {
    const cap = document.getElementById(`key-cap-${btnId}`);
    if (!cap) return;
    
    if (state === 1) {
        cap.classList.add("physical-pressed");
        setTimeout(() => {
            cap.classList.remove("physical-pressed");
        }, 220);
    }
}


// ── Sleep Timeout Setting ────────────────────────────────────────────────
async function onSleepTimeoutChange() {
    if (!isConnected || !window.pywebview || !window.pywebview.api) return;
    
    const select = document.getElementById("sleep-timeout-select");
    if (!select) return;
    const newMin = parseInt(select.value);
    
    const confirmed = confirm(`确认将深度休眠等待时间设置为 [${newMin} 分钟]？\n\n设备将写入配置并自动重启，生效后请重新点击 CONNECT 连接。`);
    if (!confirmed) {
        select.value = String(currentSleepTimeoutMin);
        return;
    }
    
    try {
        const res = await window.pywebview.api.set_sleep_timeout_min(newMin);
        if (res && res.success) {
            currentSleepTimeoutMin = newMin;
            alert(`休眠等待时间已成功更新为 [${newMin} 分钟]！\n设备正在重启，请等待几秒后重新连接。`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("设置休眠时间失败: " + (res ? res.message : "未知错误"));
            select.value = String(currentSleepTimeoutMin);
        }
    } catch (e) {
        alert("执行设置休眠时间发生异常: " + e.message);
        select.value = String(currentSleepTimeoutMin);
    }
}

// ── Reset BT Pairing ─────────────────────────────────────────────────────
async function confirmResetBtPairing() {
    if (!isConnected) {
        alert("设备未连接，请先连接设备后再执行重置。");
        return;
    }
    const confirmed = confirm(
        "确定要遗忘当前设备的蓝牙配对吗？\n\n" +
        "• 操作效果：设备将清空所有蓝牙配对记录和历史连接缓存，并自动重启。\n" +
        "• 适用场景：需要更换新电脑配对，或遇到蓝牙重连异常。\n" +
        "• 安全保护：按键键位映射、开发板型号、休眠时间等均会完整保留！\n\n" +
        "点击【确定】立即执行并重启设备。"
    );
    if (!confirmed) return;

    const btn = document.getElementById("btn-forget-pairings");
    if (btn) {
        btn.disabled = true;
        btn.textContent = "RESETTING...";
    }

    try {
        const res = await window.pywebview.api.reset_bt_pairing();
        if (res && res.status === "ok") {
            alert("蓝牙配对记录已成功清空！\n设备正在重启进入可配对状态，现在你可以在电脑的蓝牙设置中重新搜索配对。");
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
            btn.textContent = "FORGET BT";
        }
    }
}

// ── OTA Upgrade Panel & Operations ───────────────────────────────────────
function toggleOtaPanel(forceState) {
    const otaPanel = document.getElementById("ota-panel");
    if (!otaPanel) return;
    
    if (forceState !== undefined) {
        if (forceState) {
            otaPanel.classList.remove("hidden");
        } else {
            otaPanel.classList.add("hidden");
        }
    } else {
        otaPanel.classList.toggle("hidden");
    }
}

async function onLocalBinUpgrade() {
    if (!window.pywebview || !window.pywebview.api || !isConnected) {
        alert("请先连接硬件设备！");
        return;
    }
    
    const filePath = await window.pywebview.api.select_local_bin();
    if (!filePath) return;
    
    executeBinFlash(filePath);
}

async function triggerLocalBinDrop(filePath, fileName) {
    if (!isConnected) {
        alert("请先连接硬件设备！");
        return;
    }
    if (confirm(`确认立即刷入固件 [${fileName}]？`)) {
        executeBinFlash(filePath);
    }
}

async function executeBinFlash(filePath) {
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    if (progressFill) progressFill.style.width = "0%";
    if (progressPct) progressPct.textContent = "Connecting to flash local BIN...";
    
    const ok = await window.pywebview.api.trigger_ota(filePath);
    if (ok) {
        alert("固件本地升级成功！开发板正在重启生效。");
        if (progressFill) progressFill.style.width = "100%";
        if (progressPct) progressPct.textContent = "100% Completed";
    } else {
        alert("OTA 写入完成，设备正在执行校验与重启。若重启后正常加载，即代表升级成功！");
    }
}

async function onCloudBinUpgrade() {
    if (!window.pywebview || !window.pywebview.api || !isConnected) {
        alert("请先连接硬件设备！");
        return;
    }
    
    if (!confirm("确定要立即从 GitHub 下载该固件并直接刷写吗？\n(升级期间请保持有线连接且不要断电)")) {
        return;
    }
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    if (progressFill) progressFill.style.width = "0%";
    if (progressPct) progressPct.textContent = "Downloading cloud firmware...";
    
    const ok = await window.pywebview.api.start_smart_update();
    if (!ok) {
        alert("启动在线云端升级失败，请检查网络或串口连接！");
    }
}

// ── Smart Cloud Check ────────────────────────────────────────────────────
async function postConnectUpdateCheck() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const labelStatus = document.getElementById("firmware-status-label");
    const btnUpdate = document.getElementById("btn-update-trigger");
    
    if (labelStatus) {
        labelStatus.textContent = "(检测中...)";
        labelStatus.style.color = "var(--text-muted)";
    }
    
    try {
        const res = await window.pywebview.api.check_update();
        if (res && res.ok) {
            if (res.has_new) {
                if (labelStatus) {
                    labelStatus.textContent = `(发现新版 v${res.latest})`;
                    labelStatus.style.color = "var(--accent-orange)";
                }
                if (btnUpdate) {
                    btnUpdate.style.display = "inline-flex";
                    btnUpdate.onclick = () => toggleOtaPanel(true);
                }
            } else {
                if (labelStatus) {
                    labelStatus.textContent = "(已是最新)";
                    labelStatus.style.color = "#22c55e";
                }
                if (btnUpdate) btnUpdate.style.display = "none";
            }
        } else {
            if (labelStatus) {
                labelStatus.textContent = "(检测失败)";
                labelStatus.style.color = "var(--text-muted)";
            }
            if (btnUpdate) btnUpdate.style.display = "none";
        }
    } catch (e) {
        if (labelStatus) labelStatus.textContent = "";
        if (btnUpdate) btnUpdate.style.display = "none";
    }
}

function onSmartUpdateComplete(success) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) btnUpdate.disabled = false;
    
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    const labelStatus = document.getElementById("firmware-status-label");
    
    if (success) {
        if (progressFill) progressFill.style.width = "100%";
        if (progressPct) progressPct.textContent = "100% Completed";
        alert("固件在线升级成功！开发板正在重启生效。");
    } else {
        alert("在线升级完成！开发板正在重启引导，请等待片刻后重新连接。");
    }
    if (btnUpdate) btnUpdate.style.display = "none";
    if (labelStatus) {
        labelStatus.textContent = "(已是最新)";
        labelStatus.style.color = "#22c55e";
    }
}

function onSmartUpdateError(reason) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) {
        btnUpdate.disabled = false;
        btnUpdate.textContent = "UPDATE";
    }
    alert(`在线固件升级失败！\n原因: ${reason}`);
}

// ── Python Bridge Progress Callback ──────────────────────────────────────
function onOtaProgress(written, total, type) {
    const pct = ((written / total) * 100).toFixed(1);
    const fill = document.getElementById("progress-fill");
    const text = document.getElementById("progress-pct");
    if (fill) fill.style.width = `${pct}%`;
    
    let prefix = "Flashing: ";
    if (type === "download") {
        prefix = "Downloading: ";
    }
    if (text) text.textContent = `${prefix}${pct}%`;
}

// ── Close Native Window ──────────────────────────────────────────────────
function closeAppWindow() {
    if (window.pywebview && window.pywebview.api) {
        window.pywebview.api.close_window();
    }
}
