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
let selectedOtaPath = "";

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
    const btnWrite = document.getElementById("btn-write-config");
    const connPill = document.getElementById("conn-pill");
    const statusDot = document.getElementById("status-dot");
    const statusText = document.getElementById("status-text");
    
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
            if (connPill) connPill.setAttribute("data-state", "connected");
            if (statusDot) statusDot.className = "conn-dot connected";
            if (statusText) statusText.textContent = "Connected";
            
            // 启用保存按钮
            if (btnWrite) btnWrite.disabled = false;
            
            // 读取设备配置
            const config = await window.pywebview.api.fetch_config();
            if (config) {
                renderConfig(config);
            }
            
            // 展示固件版本信息区块
            const fwSection = document.getElementById("firmware-section");
            if (fwSection) fwSection.style.display = "inline-flex";

            // 显示多设备配对切换与重置按钮
            const hostSection = document.getElementById("host-switch-section");
            if (hostSection) hostSection.style.display = "inline-flex";
            const resetBtSection = document.getElementById("reset-bt-section");
            if (resetBtSection) resetBtSection.style.display = "inline-flex";
            
            // 静默检查固件更新
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
        if (connPill) connPill.setAttribute("data-state", "disconnected");
        if (statusDot) statusDot.className = "conn-dot disconnected";
        if (statusText) statusText.textContent = "Disconnected";
        document.getElementById("firmware-ver").textContent = "v--";
        
        // 禁用保存按钮
        if (btnWrite) btnWrite.disabled = true;
        
        // 隐藏升级触发按钮与状态
        const btnUpdate = document.getElementById("btn-update-trigger");
        if (btnUpdate) btnUpdate.style.display = "none";
        const labelStatus = document.getElementById("firmware-status-label");
        if (labelStatus) labelStatus.textContent = "";
        
        // 隐藏设备信息胶囊
        const fwSection = document.getElementById("firmware-section");
        if (fwSection) fwSection.style.display = "none";
        const hostSection = document.getElementById("host-switch-section");
        if (hostSection) hostSection.style.display = "none";
        const resetBtSection = document.getElementById("reset-bt-section");
        if (resetBtSection) resetBtSection.style.display = "none";
        
        // 隐藏操作系统徽章
        const osBadge = document.getElementById("os-badge");
        if (osBadge) osBadge.style.display = "none";
        
        resetConfigUi();
    }
}

// ── Render Config Cache ──────────────────────────────────────────────────
function renderConfig(config) {
    if (!config) return;

    // 固件版本
    const fwEl = document.getElementById("firmware-ver");
    if (fwEl) fwEl.textContent = "v" + (config.version || "1.0.18-beta1");

    // 麦克风与休眠模式开关
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

    // 双设备配对状态 (电脑一 / 电脑二)
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
            pill0.title = `电脑一 (已配对${os0Name})`;
        } else {
            dot0.className = "host-status-dot empty";
            pill0.title = "电脑一 (未配对/空槽)";
        }
    }

    const dot1 = document.getElementById("host-dot-1");
    const pill1 = document.getElementById("host-pill-1");
    const os1Name = (config.slot1_os === 1) ? " · macOS" : (config.slot1_os === 2) ? " · Windows" : "";
    if (dot1 && pill1) {
        if (config.slot1_paired === 1) {
            dot1.className = "host-status-dot paired";
            pill1.title = `电脑二 (已配对${os1Name})`;
        } else {
            dot1.className = "host-status-dot empty";
            pill1.title = "电脑二 (未配对/空槽)";
        }
    }

    // 渲染操作系统识别徽章（左侧栏 KEY MAPPING 标题旁）
    const curOs = (config.query_os !== undefined) ? config.query_os : ((currentActiveSlot === 0) ? config.slot0_os : config.slot1_os);
    updateOsBadge(curOs);

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
    if (idx < 0 || idx > 3) return;
    selectedKeyIdx = idx;

    // 更新右侧键帽选中样式
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

    // 更新左侧面板按键序号
    const keyIndexEl = document.getElementById("currentKeyIndex");
    if (keyIndexEl) {
        keyIndexEl.textContent = `KEY ${idx + 1}`;
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
        subEl.textContent = "Press any physical key on keyboard";
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
    // 麦克风硬件指示联动
    const slot = document.getElementById("virtualMicSlot");
    const micToggle = document.getElementById("mic-toggle");
    if (slot && micToggle) {
        if (micToggle.checked) {
            slot.style.color = "var(--accent-orange)";
        } else {
            slot.style.color = "var(--fg-muted)";
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
    
    const hostName = (currentActiveSlot === 0) ? "电脑一" : "电脑二";
    const btnWrite = document.getElementById("btn-write-config");
    const origHtml = btnWrite.innerHTML;
    btnWrite.disabled = true;
    btnWrite.innerHTML = "<span>WRITING & REBOOTING...</span>";
    
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
            alert(`所有按键与设置已成功写入【${hostName}】！\n设备正在自动重启以彻底应用新配置与蓝牙参数。`);
            if (isConnected) {
                onConnectClick();
            }
        } else {
            alert("配置写入失败，请检查串口连接状态！");
        }
    } catch (e) {
        btnWrite.disabled = false;
        btnWrite.innerHTML = origHtml;
        alert("写入配置发生异常: " + e.message);
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
        `• 自动连接：重启后自动连接【${hostName}】（若未配对则进入广播等待配对）；\n` +
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
        `• 安全保护：按键键位映射、休眠时间等均完整保留！\n\n` +
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
            btn.innerHTML = `<svg viewBox="0 0 24 24" width="11" height="11"><path fill="currentColor" d="M17.71 7.71L12 2h-1v7.59L6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 11 14.41V22h1l5.71-5.71-4.3-4.29 4.3-4.29zM13 5.83l1.88 1.88L13 9.59V5.83zm1.88 10.46L13 18.17v-3.76l1.88 1.88z"/></svg> FORGET BT`;
        }
    }
}

// ── 操作系统类型徽章渲染 (1: Apple, 2: Windows) ─────────────────────────────
function updateOsBadge(osType) {
    const badge = document.getElementById("os-badge");
    const icon = document.getElementById("os-badge-icon");
    const text = document.getElementById("os-badge-text");
    if (!badge || !icon || !text) return;

    if (osType === 1) { // Apple
        badge.style.display = "inline-flex";
        badge.className = "os-badge os-apple";
        badge.title = "已识别配对系统：Apple (macOS)";
        icon.innerHTML = `<svg viewBox="0 0 170 170" width="11" height="11" fill="currentColor">
            <path d="M150.37 130.25c-2.45 5.66-5.35 10.87-8.71 15.66-4.58 6.53-8.33 11.05-11.22 13.56-4.48 4.12-9.28 6.23-14.42 6.35-3.69 0-8.14-1.05-13.32-3.18-5.19-2.12-9.97-3.17-14.34-3.17-4.58 0-9.49 1.05-14.75 3.17-5.26 2.13-9.5 3.24-12.74 3.35-4.35.13-9.16-1.9-14.42-6.08-3.7-3.04-7.7-7.85-12.01-14.42-5.74-8.81-10.23-18.7-13.48-29.68-3.24-10.97-4.87-21.78-4.87-32.43 0-16.32 4.11-29.74 12.33-40.27 8.22-10.53 18.7-15.86 31.44-15.99 6.29 0 12.82 1.58 19.59 4.75 6.77 3.17 11.22 4.88 13.34 5.12 2.7-.49 7.4-2.31 14.1-5.48 6.7-3.16 12.78-4.66 18.24-4.5 14.15.52 25.13 6.13 32.93 16.83-12.63 7.64-18.76 18.04-18.39 31.22.37 10.37 4.26 19.06 11.66 26.06 7.41 7 16.14 10.88 26.2 11.64-2.22 6.64-4.87 13.06-7.96 19.26zM119.22 33.15c0-7.72 2.76-14.97 8.29-21.75 5.53-6.77 12.28-10.9 20.25-12.4 1.1 6.96-.34 14.11-4.33 21.46-3.99 7.34-9.87 12.08-17.64 14.22-.88-.49-2.07-.86-3.57-1.12-1.5-.27-2.5-.41-3-.41z"/>
        </svg>`;
        text.textContent = "macOS";
    } else if (osType === 2) { // Windows
        badge.style.display = "inline-flex";
        badge.className = "os-badge os-windows";
        badge.title = "已识别配对系统：Windows PC";
        icon.innerHTML = `<svg viewBox="0 0 24 24" width="11" height="11" fill="currentColor">
            <path d="M0 3.449L9.75 2.1v9.451H0V3.449zm10.949-1.551L24 0v11.551H10.949V1.898zM0 12.45h9.75V21.9L0 20.551V12.45zm10.949 0H24V24l-13.051-1.898V12.45z"/>
        </svg>`;
        text.textContent = "Windows";
    } else {
        badge.style.display = "none";
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
    
    progressFill.style.width = "0%";
    progressPct.textContent = "Flashing: 0.0%";
    
    const ok = await window.pywebview.api.trigger_ota(selectedOtaPath);
    
    if (ok) {
        alert("固件升级成功！开发板正在重启，请稍候重新连接。");
        progressFill.style.width = "100%";
        progressPct.textContent = "100% Completed";
        if (isConnected) onConnectClick();
    } else {
        alert("固件写入完成，设备正在执行引导校验与重启。");
    }
}

// ── Smart Cloud Check & Auto Update ──────────────────────────────────────
async function postConnectUpdateCheck() {
    if (!window.pywebview || !window.pywebview.api) return;
    
    const labelStatus = document.getElementById("firmware-status-label");
    const btnUpdate = document.getElementById("btn-update-trigger");
    
    if (labelStatus) {
        labelStatus.textContent = "(检测中...)";
        labelStatus.style.color = "var(--fg-muted)";
    }
    
    const res = await window.pywebview.api.check_update();
    
    if (res && res.ok) {
        if (res.has_new) {
            if (labelStatus) {
                labelStatus.textContent = `(有新版 v${res.latest})`;
                labelStatus.style.color = "var(--accent-orange)";
            }
            if (btnUpdate) {
                btnUpdate.style.display = "inline-flex";
                btnUpdate.dataset.latest = res.latest;
                btnUpdate.textContent = "UPDATE";
            }
        } else {
            if (labelStatus) {
                labelStatus.textContent = "(最新)";
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
    
    if (!confirm("确定要立即从 GitHub 自动下载并刷写最新固件吗？\n(升级期间请保持有线连接且不要断电)")) {
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
        alert("启动在线升级失败，请检查网络或串口连接！");
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
    const labelStatus = document.getElementById("firmware-status-label");
    
    if (progressFill) progressFill.style.width = "100%";
    if (progressPct) progressPct.textContent = "100% Completed";
    if (labelStatus) {
        labelStatus.textContent = "(最新)";
        labelStatus.style.color = "var(--status-green)";
    }
    
    alert("固件在线升级成功！开发板正在重启生效，请等待片刻后重新连接。");
    if (isConnected) onConnectClick();
}

function onSmartUpdateError(reason) {
    const btnUpdate = document.getElementById("btn-update-trigger");
    if (btnUpdate) {
        btnUpdate.disabled = false;
        btnUpdate.textContent = "UPDATE";
    }
    alert(`在线固件升级失败！\n原因: ${reason}`);
}

function onOtaProgress(written, total, type) {
    const pct = ((written / total) * 100).toFixed(1);
    const progressFill = document.getElementById("progress-fill");
    const progressPct = document.getElementById("progress-pct");
    if (progressFill) progressFill.style.width = `${pct}%`;
    
    let prefix = "Flashing: ";
    if (type === "download") prefix = "Downloading: ";
    if (progressPct) progressPct.textContent = `${prefix}${pct}%`;
}
