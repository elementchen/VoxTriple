/**
 * VoxTriple White E-Commerce Long-Scroll Interaction
 * Pure White Flagship Tmall Store Experience
 */

document.addEventListener('DOMContentLoaded', () => {
  // 产品配置数据
  const productData = {
    editions: {
      flagship: {
        id: 'flagship',
        title: '官方旗舰成品版 (开箱即用)',
        price: 299,
        originalPrice: 399,
        desc: '全套高精度出厂调校装配，客制化机械轴体，已内置聚合物锂电池并刷入最新固件。赠 1.5m 编织线与收纳袋。'
      },
      maker: {
        id: 'maker',
        title: '极客 DIY 探险套装 (自备烙铁)',
        price: 199,
        originalPrice: 269,
        desc: '包含 3D 打印外壳、ESP32 沉金主板、INMP441 模块、机械轴体与排线螺丝，体验亲手组装的创客乐趣。'
      }
    }
  };

  let state = {
    selectedEdition: 'flagship',
    quantity: 1
  };

  // DOM 元素
  const currentPriceEl = document.getElementById('current-price');
  const originalPriceEl = document.getElementById('original-price');
  const stickyPriceEl = document.getElementById('sticky-price');
  const editionBoxes = document.querySelectorAll('.edition-box');
  const qtyMinus = document.getElementById('qty-minus');
  const qtyPlus = document.getElementById('qty-plus');
  const qtyInput = document.getElementById('qty-input');
  const stickyBuyBar = document.getElementById('sticky-buy-bar');
  const heroSection = document.getElementById('hero-section');

  // 画廊切换
  const mainImage = document.getElementById('main-preview-img');
  const mainVideo = document.getElementById('main-preview-video');
  const thumbs = document.querySelectorAll('.thumb-card');

  thumbs.forEach(thumb => {
    thumb.addEventListener('click', () => {
      thumbs.forEach(t => t.classList.remove('active'));
      thumb.classList.add('active');

      const mediaType = thumb.getAttribute('data-type');
      const mediaSrc = thumb.getAttribute('data-src');

      if (mediaType === 'video') {
        if (mainImage) mainImage.classList.add('hidden');
        if (mainVideo) {
          mainVideo.classList.remove('hidden');
          mainVideo.src = mediaSrc;
          mainVideo.play().catch(e => console.log('Video autoplay prevented:', e));
        }
      } else {
        if (mainVideo) {
          mainVideo.pause();
          mainVideo.classList.add('hidden');
        }
        if (mainImage) {
          mainImage.classList.remove('hidden');
          mainImage.style.opacity = '0.3';
          mainImage.src = mediaSrc;
          setTimeout(() => {
            mainImage.style.opacity = '1';
          }, 150);
        }
      }
    });
  });

  // 价格更新
  function updatePrice() {
    const edition = productData.editions[state.selectedEdition];
    const totalCurrent = edition.price * state.quantity;
    const totalOriginal = edition.originalPrice * state.quantity;

    if (currentPriceEl) currentPriceEl.textContent = `¥${totalCurrent}`;
    if (originalPriceEl) originalPriceEl.textContent = `¥${totalOriginal}`;
    if (stickyPriceEl) stickyPriceEl.textContent = `¥${totalCurrent}`;
  }

  editionBoxes.forEach(box => {
    box.addEventListener('click', () => {
      const editionKey = box.getAttribute('data-edition');
      if (!productData.editions[editionKey]) return;

      editionBoxes.forEach(b => b.classList.remove('selected'));
      box.classList.add('selected');
      state.selectedEdition = editionKey;

      updatePrice();
    });
  });

  // 数量加减
  if (qtyMinus && qtyPlus && qtyInput) {
    qtyMinus.addEventListener('click', () => {
      if (state.quantity > 1) {
        state.quantity--;
        qtyInput.value = state.quantity;
        updatePrice();
      }
    });

    qtyPlus.addEventListener('click', () => {
      if (state.quantity < 99) {
        state.quantity++;
        qtyInput.value = state.quantity;
        updatePrice();
      }
    });
  }

  // 吸顶悬浮购买栏监听
  if (heroSection && stickyBuyBar) {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (!entry.isIntersecting) {
          stickyBuyBar.classList.add('visible');
        } else {
          stickyBuyBar.classList.remove('visible');
        }
      },
      { threshold: 0.1 }
    );
    observer.observe(heroSection);
  }

  // 购买弹窗
  const buyModal = document.getElementById('buy-modal');
  const btnBuyNow = document.getElementById('btn-buy-now');
  const stickyBuyBtn = document.getElementById('sticky-buy-btn');
  const closeModalBtn = document.getElementById('close-modal-btn');
  const modalConfirmBtn = document.getElementById('modal-confirm-btn');
  const modalSummary = document.getElementById('modal-summary');
  const modalPrice = document.getElementById('modal-price');

  function openModal() {
    const edition = productData.editions[state.selectedEdition];
    const totalPrice = edition.price * state.quantity;

    if (modalSummary) {
      modalSummary.innerHTML = `
        <div class="flex items-center gap-4 p-4 rounded-xl bg-gray-50 border border-gray-200 mb-4">
          <img src="../Medias/Final03.png" class="w-16 h-16 object-contain rounded-lg border border-gray-200 bg-white" alt="VoxTriple" />
          <div>
            <div class="font-bold text-gray-900 text-sm">VoxTriple 掌上语音输入机械终端</div>
            <div class="text-xs text-blue-600 font-medium mt-1">${edition.title} × ${state.quantity}</div>
            <div class="text-xs text-gray-500 mt-0.5">客制化机械轴体 · 聚合物锂电长续航</div>
          </div>
        </div>
        <div class="text-xs text-gray-600 space-y-1.5 leading-relaxed bg-blue-50/50 p-3 rounded-lg border border-blue-100">
          <p>🚚 <strong>顺丰极速航空包邮</strong>：现货订单 24 小时内发出</p>
          <p>🛡️ <strong>品质承诺</strong>：支持 7 天无理由退换，整机 1 年官方换新质保</p>
          <p>📦 <strong>开箱即连</strong>：无需安装驱动，蓝牙直连即享每分钟 300 字语音口述体验</p>
        </div>
      `;
    }

    if (modalPrice) {
      modalPrice.textContent = `¥${totalPrice}`;
    }

    if (buyModal) {
      buyModal.classList.remove('hidden');
      buyModal.classList.add('flex');
    }
  }

  function closeModal() {
    if (buyModal) {
      buyModal.classList.add('hidden');
      buyModal.classList.remove('flex');
    }
  }

  if (btnBuyNow) btnBuyNow.addEventListener('click', openModal);
  if (stickyBuyBtn) stickyBuyBtn.addEventListener('click', openModal);
  if (closeModalBtn) closeModalBtn.addEventListener('click', closeModal);
  if (buyModal) {
    buyModal.addEventListener('click', (e) => {
      if (e.target === buyModal) closeModal();
    });
  }

  if (modalConfirmBtn) {
    modalConfirmBtn.addEventListener('click', () => {
      showToast('🎉 预定意向已记录！感谢您对 VoxTriple 极客产品的支持。');
      closeModal();
    });
  }

  // FAQ 手风琴
  const faqRows = document.querySelectorAll('.faq-row');
  faqRows.forEach(row => {
    const trigger = row.querySelector('.faq-trigger');
    if (trigger) {
      trigger.addEventListener('click', () => {
        const isOpen = row.classList.contains('open');
        faqRows.forEach(r => r.classList.remove('open'));
        if (!isOpen) {
          row.classList.add('open');
        }
      });
    }
  });

  // 提示 Toast
  function showToast(msg) {
    const toast = document.createElement('div');
    toast.className = 'fixed top-6 left-1/2 -translate-x-1/2 z-50 px-5 py-3 rounded-full bg-gray-900 text-white text-sm font-medium shadow-2xl transition-all duration-300 pointer-events-none flex items-center gap-2 border border-gray-700';
    toast.innerHTML = `<span>${msg}</span>`;
    document.body.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translate(-50%, -20px)';
      setTimeout(() => toast.remove(), 300);
    }, 2800);
  }

  updatePrice();
});
