/**
 * VoxTriple Official Website Interactive Architecture
 * Live Simulator, Key Monitor, Audio Spectrum & Firmware Flash Visualizer
 */

document.addEventListener('DOMContentLoaded', () => {
  // 1. 模拟按键与上位机联动逻辑
  const simKeys = document.querySelectorAll('.sim-key');
  const ledIndicator = document.getElementById('board-led');
  const logContainer = document.getElementById('serial-log-stream');
  const appKeyDisplay = document.getElementById('app-key-active');
  const appStatusDisplay = document.getElementById('app-status-badge');

  const keyConfig = {
    'btn-1': {
      gpio: 4,
      name: 'Button 1 (PTT / 唤醒)',
      hidKey: 'Enter (0x28)',
      action: 'PTT 录音启动 & 发送回车',
      isPTT: true
    },
    'btn-2': {
      gpio: 18,
      name: 'Button 2 (Key 2)',
      hidKey: 'Escape (0x29)',
      action: '发送 Esc 取消指令',
      isPTT: false
    },
    'btn-3': {
      gpio: 19,
      name: 'Button 3 (Key 3)',
      hidKey: 'Space (0x2C)',
      action: '发送空格指令 (播放/暂停)',
      isPTT: false
    },
    'btn-4': {
      gpio: 23,
      name: 'Button 4 (Key 4 / 配对)',
      hidKey: 'Tab (0x2B)',
      action: '发送 Tab (长按3秒重置蓝牙)',
      isPTT: false
    }
  };

  function triggerKeyAction(btnId, isPress) {
    const config = keyConfig[btnId];
    if (!config) return;

    const timeStr = new Date().toTimeString().split(' ')[0] + '.' + Math.floor(Math.random() * 900 + 100);

    if (isPress) {
      if (ledIndicator) ledIndicator.classList.add('active');
      if (appKeyDisplay) {
        appKeyDisplay.textContent = config.name;
        appKeyDisplay.className = 'text-sm font-mono font-bold text-blue-400 bg-blue-500/10 px-2.5 py-1 rounded-md border border-blue-500/20';
      }
      if (appStatusDisplay) {
        appStatusDisplay.textContent = config.isPTT ? '🎙️ 麦克风音频传输中 (mSBC)' : '⚡ HID 信号触发中';
        appStatusDisplay.className = 'text-xs px-2 py-0.5 rounded-full font-medium bg-emerald-500/10 text-emerald-400 border border-emerald-500/20';
      }

      appendLog(`[${timeStr}] [INTR] GPIO ${config.gpio} FALLING -> Key: ${config.name} PRESSED`);
      appendLog(`[${timeStr}] [HID-TX] HID Report Sent: ${config.hidKey}`);
      if (config.isPTT) {
        appendLog(`[${timeStr}] [HFP-DSP] INMP441 DMA buffer full -> mSBC 16kHz Stream ON`);
      }
    } else {
      if (ledIndicator) ledIndicator.classList.remove('active');
      if (appKeyDisplay) {
        appKeyDisplay.textContent = 'None (IDLE / 等待输入)';
        appKeyDisplay.className = 'text-sm font-mono font-bold text-neutral-500 bg-neutral-800/40 px-2.5 py-1 rounded-md';
      }
      if (appStatusDisplay) {
        appStatusDisplay.textContent = '● 串口就绪 (COM5 @ 115200)';
        appStatusDisplay.className = 'text-xs px-2 py-0.5 rounded-full font-medium bg-blue-500/10 text-blue-400 border border-blue-500/20';
      }
      appendLog(`[${timeStr}] [INTR] GPIO ${config.gpio} RISING -> Released (Standby)`);
    }
  }

  function appendLog(text) {
    if (!logContainer) return;
    const line = document.createElement('div');
    line.className = 'leading-relaxed hover:bg-white/5 px-2 py-0.5 rounded transition-colors text-[11px]';
    
    if (text.includes('PRESSED')) {
      line.innerHTML = `<span class="text-blue-400 font-semibold">${text}</span>`;
    } else if (text.includes('Stream ON')) {
      line.innerHTML = `<span class="text-emerald-400 font-medium">${text}</span>`;
    } else {
      line.innerHTML = `<span class="text-neutral-400">${text}</span>`;
    }

    logContainer.appendChild(line);
    logContainer.scrollTop = logContainer.scrollHeight;

    // 保留最近 40 行，防止 DOM 过多
    if (logContainer.children.length > 40) {
      logContainer.removeChild(logContainer.children[0]);
    }
  }

  // 绑定实体模拟按键点击与长按
  simKeys.forEach(keyEl => {
    const btnId = keyEl.id;

    // 鼠标点击事件
    keyEl.addEventListener('mousedown', () => {
      keyEl.classList.add('pressed');
      triggerKeyAction(btnId, true);
    });

    keyEl.addEventListener('mouseup', () => {
      keyEl.classList.remove('pressed');
      triggerKeyAction(btnId, false);
    });

    keyEl.addEventListener('mouseleave', () => {
      if (keyEl.classList.contains('pressed')) {
        keyEl.classList.remove('pressed');
        triggerKeyAction(btnId, false);
      }
    });

    // 移动端触控
    keyEl.addEventListener('touchstart', (e) => {
      e.preventDefault();
      keyEl.classList.add('pressed');
      triggerKeyAction(btnId, true);
    });

    keyEl.addEventListener('touchend', (e) => {
      e.preventDefault();
      keyEl.classList.remove('pressed');
      triggerKeyAction(btnId, false);
    });
  });

  // 2. 键盘物理快捷键同步 (数字键 1, 2, 3, 4)
  window.addEventListener('keydown', (e) => {
    if (['1', '2', '3', '4'].includes(e.key)) {
      const target = document.getElementById(`btn-${e.key}`);
      if (target && !target.classList.contains('pressed')) {
        target.classList.add('pressed');
        triggerKeyAction(`btn-${e.key}`, true);
      }
    }
  });

  window.addEventListener('keyup', (e) => {
    if (['1', '2', '3', '4'].includes(e.key)) {
      const target = document.getElementById(`btn-${e.key}`);
      if (target && target.classList.contains('pressed')) {
        target.classList.remove('pressed');
        triggerKeyAction(`btn-${e.key}`, false);
      }
    }
  });

  // 3. 20秒 OTA 模拟演示动画
  const otaBtn = document.getElementById('demo-ota-btn');
  const otaBar = document.getElementById('demo-ota-progress');
  const otaPercent = document.getElementById('demo-ota-percent');
  const otaStatus = document.getElementById('demo-ota-status');

  if (otaBtn && otaBar && otaPercent && otaStatus) {
    let otaRunning = false;
    otaBtn.addEventListener('click', () => {
      if (otaRunning) return;
      otaRunning = true;
      otaBtn.disabled = true;
      otaBtn.classList.add('opacity-50', 'cursor-not-allowed');

      let progress = 0;
      otaStatus.textContent = '正在从 GitHub 获取 esp32_bt_mic_v1.0.19.bin...';
      otaBar.style.width = '0%';
      otaPercent.textContent = '0%';

      const interval = setInterval(() => {
        progress += Math.floor(Math.random() * 6) + 3;
        if (progress > 100) progress = 100;

        otaBar.style.width = `${progress}%`;
        otaPercent.textContent = `${progress}%`;

        if (progress < 30) {
          otaStatus.textContent = '正在通过物理串口建立直接高速分包传输通道...';
        } else if (progress < 85) {
          otaStatus.textContent = `写入 Flash (0x10000) 中... [${progress * 8} KB / 780 KB]`;
        } else if (progress < 100) {
          otaStatus.textContent = '校验 SHA256 完整性并刷新 Bootloader 分区表...';
        } else {
          clearInterval(interval);
          otaStatus.textContent = '✅ OTA 写入完成！芯片已自动执行软重启 (Total: 18.4s)';
          otaStatus.className = 'text-xs text-emerald-400 font-semibold mt-1';
          otaBtn.disabled = false;
          otaBtn.classList.remove('opacity-50', 'cursor-not-allowed');
          otaRunning = false;
        }
      }, 350);
    });
  }

  // 4. 音频频谱动态随机抖动
  const waveBars = document.querySelectorAll('.dynamic-wave-bar');
  setInterval(() => {
    waveBars.forEach(bar => {
      const scale = (Math.random() * 0.7 + 0.3).toFixed(2);
      bar.style.transform = `scaleY(${scale})`;
    });
  }, 180);
});
