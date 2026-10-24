/**
 * VoxTriple Official Website Interactive Architecture
 * Apple-grade White High-Tech Portal & Flash-Talk (闪电说) Ecosystem
 */

document.addEventListener('DOMContentLoaded', () => {
  // 1. 初始化 Lucide 图标
  if (window.lucide) {
    window.lucide.createIcons();
  }

  // 2. 模拟按键与上位机控制台联动逻辑
  const simKeys = document.querySelectorAll('.sim-key');
  const ledIndicator = document.getElementById('board-led');
  const logContainer = document.getElementById('serial-log-stream');
  const appKeyDisplay = document.getElementById('app-key-active');
  const appStatusDisplay = document.getElementById('app-status-badge');

  const keyConfig = {
    'btn-1': {
      gpio: 4,
      name: 'Key 1 (PTT 数字硅麦收音)',
      hidKey: 'F13 (0x68) / PTT Stream',
      action: '按住即说 · 数字硅麦收音中',
      isPTT: true
    },
    'btn-2': {
      gpio: 18,
      name: 'Key 2 (Enter / 直发 AI)',
      hidKey: 'Enter (0x28)',
      action: '松手顺按发送 · Prompt 直达模型',
      isPTT: false
    },
    'btn-3': {
      gpio: 19,
      name: 'Key 3 (Esc / 一键取消)',
      hidKey: 'Escape (0x29)',
      action: '发送 Esc 取消录音与指令',
      isPTT: false
    },
    'btn-4': {
      gpio: 23,
      name: 'Key 4 (双设备蓝牙切换)',
      hidKey: 'Custom HID (0x7F)',
      action: '蓝牙双通道秒切 (Host A <-> Host B)',
      isPTT: false
    }
  };

  function triggerKeyAction(btnId, isPress) {
    const config = keyConfig[btnId];
    if (!config) return;

    const timeStr = new Date().toTimeString().split(' ')[0] + '.' + Math.floor(Math.random() * 900 + 100);

    if (isPress) {
      if (ledIndicator) {
        ledIndicator.classList.remove('bg-emerald-400');
        ledIndicator.classList.add('bg-blue-500', 'ring-4', 'ring-blue-400/40');
      }
      if (appKeyDisplay) {
        appKeyDisplay.textContent = config.name;
        appKeyDisplay.className = 'text-xs font-mono font-bold text-blue-600 bg-blue-50 px-2.5 py-1 rounded-md border border-blue-200';
      }
      if (appStatusDisplay) {
        appStatusDisplay.textContent = config.isPTT ? '🎙️ 数字硅麦收音中 (mSBC 16kHz)' : '⚡ HID 信号触发中';
        appStatusDisplay.className = 'text-[11px] px-2 py-0.5 rounded-full font-medium bg-emerald-50 text-emerald-700 border border-emerald-200';
      }

      appendLog(`[${timeStr}] [INTR] GPIO ${config.gpio} FALLING -> ${config.name} PRESSED`);
      appendLog(`[${timeStr}] [HID-TX] HID Report Dispatched: ${config.hidKey}`);
      if (config.isPTT) {
        appendLog(`[${timeStr}] [I2S-AUDIO] Digital MEMS Mic DMA Stream Active (Zero Latency)`);
      }
    } else {
      if (ledIndicator) {
        ledIndicator.classList.remove('bg-blue-500', 'ring-4', 'ring-blue-400/40');
        ledIndicator.classList.add('bg-emerald-400');
      }
      if (appKeyDisplay) {
        appKeyDisplay.textContent = 'None (IDLE / 等待大拇指触发)';
        appKeyDisplay.className = 'text-xs font-mono font-bold text-gray-500 bg-gray-100 px-2.5 py-1 rounded-md';
      }
      if (appStatusDisplay) {
        appStatusDisplay.textContent = '● 经典蓝牙直连已就绪 (免驱)';
        appStatusDisplay.className = 'text-[11px] px-2 py-0.5 rounded-full font-medium bg-blue-50 text-blue-600 border border-blue-100';
      }
      appendLog(`[${timeStr}] [INTR] GPIO ${config.gpio} RISING -> Released (Standby)`);
    }
  }

  function appendLog(text) {
    if (!logContainer) return;
    const line = document.createElement('div');
    line.className = 'leading-relaxed hover:bg-slate-800/80 px-2 py-0.5 rounded transition-colors text-[11px] font-mono';
    
    if (text.includes('PRESSED')) {
      line.innerHTML = `<span class="text-sky-400 font-semibold">${text}</span>`;
    } else if (text.includes('Active')) {
      line.innerHTML = `<span class="text-emerald-400 font-medium">${text}</span>`;
    } else {
      line.innerHTML = `<span class="text-slate-400">${text}</span>`;
    }

    logContainer.appendChild(line);
    logContainer.scrollTop = logContainer.scrollHeight;

    if (logContainer.children.length > 35) {
      logContainer.removeChild(logContainer.children[0]);
    }
  }

  // 绑定实体模拟按键点击
  simKeys.forEach(keyEl => {
    const btnId = keyEl.id;

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

  // 键盘物理快捷键同步 (数字键 1, 2, 3, 4)
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

  // 3. 20 秒无线 OTA 模拟演示
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
      otaStatus.textContent = '正在获取官方最新固件 voxtriple_fw_v1.0.19.bin...';
      otaBar.style.width = '0%';
      otaPercent.textContent = '0%';

      const interval = setInterval(() => {
        progress += Math.floor(Math.random() * 6) + 4;
        if (progress > 100) progress = 100;

        otaBar.style.width = `${progress}%`;
        otaPercent.textContent = `${progress}%`;

        if (progress < 30) {
          otaStatus.textContent = '建立高速蓝牙分包通道 (Block Size: 512B)...';
        } else if (progress < 85) {
          otaStatus.textContent = `写入 ESP32 OTA 分区中... [${Math.floor(progress * 7.8)} KB / 780 KB]`;
        } else if (progress < 100) {
          otaStatus.textContent = '校验 SHA-256 完整性并刷新 Bootloader 分区表...';
        } else {
          clearInterval(interval);
          otaStatus.textContent = '✅ OTA 写入完成！芯片已完成平滑软重启 (耗时: 18.2s)';
          otaStatus.className = 'text-xs text-emerald-600 font-semibold mt-1';
          otaBtn.disabled = false;
          otaBtn.classList.remove('opacity-50', 'cursor-not-allowed');
          otaRunning = false;
        }
      }, 350);
    });
  }

  // 4. 音频频谱动态波形模拟
  const waveBars = document.querySelectorAll('.dynamic-wave-bar');
  if (waveBars.length > 0) {
    setInterval(() => {
      waveBars.forEach(bar => {
        const scale = (Math.random() * 0.75 + 0.25).toFixed(2);
        bar.style.transform = `scaleY(${scale})`;
      });
    }, 180);
  }

  // 5. 闪电技能库：一键复制 Prompt 脚本
  const copyBtns = document.querySelectorAll('.copy-prompt-btn');
  copyBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const promptText = btn.getAttribute('data-prompt') || '';
      if (promptText) {
        navigator.clipboard.writeText(promptText).then(() => {
          const originalText = btn.innerHTML;
          btn.innerHTML = `<i data-lucide="check" class="w-3.5 h-3.5 text-emerald-600"></i><span class="text-emerald-600">已复制配置</span>`;
          if (window.lucide) window.lucide.createIcons();
          setTimeout(() => {
            btn.innerHTML = originalText;
            if (window.lucide) window.lucide.createIcons();
          }, 2000);
        });
      }
    });
  });

  // 6. 技能分类 Filter 切换
  const filterTabs = document.querySelectorAll('.skill-filter-tab');
  const skillCards = document.querySelectorAll('.skill-card-item');

  filterTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      filterTabs.forEach(t => {
        t.classList.remove('bg-blue-600', 'text-white');
        t.classList.add('bg-gray-100', 'text-gray-700');
      });
      tab.classList.remove('bg-gray-100', 'text-gray-700');
      tab.classList.add('bg-blue-600', 'text-white');

      const filterCategory = tab.getAttribute('data-category');
      skillCards.forEach(card => {
        if (filterCategory === 'all' || card.getAttribute('data-category') === filterCategory) {
          card.style.display = 'flex';
        } else {
          card.style.display = 'none';
        }
      });
    });
  });
});
