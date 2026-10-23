/**
 * VoxTriple E-Commerce Landing Page Script
 * Smooth Apple-like micro-interactions, variant selector, and gallery handling
 */

document.addEventListener('DOMContentLoaded', () => {
  // 1. 数据配置
  const productData = {
    colors: {
      black: {
        name: '深空曜黑 (Stealth Black)',
        image: '../Medias/Final03.png',
        desc: '沉稳哑光质感，微珠光涂层，极客与办公桌面的经典之选'
      },
      gray: {
        name: '工业透灰 (Cyber Gray)',
        image: '../Medias/Final02.png',
        desc: '半透磨砂外壳，微透内部沉金 PCB 与蓝色指示灯光芒'
      },
      white: {
        name: '极简冷白 (Frost White)',
        image: '../Medias/Final1.png',
        desc: '纯净温润，如瓷石般素雅，与浅色键盘和原木桌面浑然天成'
      }
    },
    editions: {
      maker: {
        id: 'maker',
        title: '创客 DIY 探险套装',
        price: 199,
        originalPrice: 269,
        desc: '包含 3D 打印精磨外壳、沉金主板、INMP441 模块、轻触微动与螺丝排线。享受亲手焊接与组装的纯粹极客乐趣。'
      },
      flagship: {
        id: 'flagship',
        title: '官方旗舰成品版 (开箱即用)',
        price: 299,
        originalPrice: 399,
        desc: '出厂由声学工程师精密校准装配，通过多轮信号与声学阻抗测试。附赠 1.5m 编织 Type-C 弯头数据线与便携收纳绒布袋。'
      }
    }
  };

  let state = {
    selectedColor: 'black',
    selectedEdition: 'flagship',
    quantity: 1
  };

  // DOM 元素引用
  const mainMediaContainer = document.getElementById('main-media-container');
  const mainImage = document.getElementById('main-preview-img');
  const mainVideo = document.getElementById('main-preview-video');
  const thumbs = document.querySelectorAll('.thumb-item');
  const colorDots = document.querySelectorAll('.color-dot');
  const currentColorLabel = document.getElementById('current-color-label');
  const editionCards = document.querySelectorAll('.option-card');
  const currentPriceEl = document.getElementById('current-price');
  const originalPriceEl = document.getElementById('original-price');
  const stickyPriceEl = document.getElementById('sticky-price');
  const qtyMinus = document.getElementById('qty-minus');
  const qtyPlus = document.getElementById('qty-plus');
  const qtyInput = document.getElementById('qty-input');
  const stickyBuyBar = document.getElementById('sticky-buy-bar');
  const heroSection = document.getElementById('hero-section');
  
  // 模态弹窗与 Toast
  const buyModal = document.getElementById('buy-modal');
  const closeModalBtn = document.getElementById('close-modal-btn');
  const btnBuyNow = document.getElementById('btn-buy-now');
  const stickyBuyBtn = document.getElementById('sticky-buy-btn');
  const modalConfirmBtn = document.getElementById('modal-confirm-btn');
  const modalSummary = document.getElementById('modal-summary');
  const modalPrice = document.getElementById('modal-price');

  // 2. 媒体画廊切换
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
          mainVideo.play().catch(e => console.log('Autoplay prevented:', e));
        }
      } else {
        if (mainVideo) {
          mainVideo.pause();
          mainVideo.classList.add('hidden');
        }
        if (mainImage) {
          mainImage.classList.remove('hidden');
          mainImage.style.opacity = '0.4';
          mainImage.src = mediaSrc;
          setTimeout(() => {
            mainImage.style.opacity = '1';
          }, 150);
        }
      }
    });
  });

  // 3. 颜色选择切换
  colorDots.forEach(dot => {
    dot.addEventListener('click', () => {
      const colorKey = dot.getAttribute('data-color');
      if (!productData.colors[colorKey]) return;

      colorDots.forEach(d => d.classList.remove('active'));
      dot.classList.add('active');
      state.selectedColor = colorKey;

      const colorInfo = productData.colors[colorKey];
      if (currentColorLabel) {
        currentColorLabel.textContent = colorInfo.name;
      }

      // 切换主图为该颜色效果
      if (mainImage) {
        if (mainVideo) {
          mainVideo.pause();
          mainVideo.classList.add('hidden');
        }
        mainImage.classList.remove('hidden');
        mainImage.style.opacity = '0.3';
        mainImage.src = colorInfo.image;
        setTimeout(() => {
          mainImage.style.opacity = '1';
        }, 150);
      }
    });
  });

  // 4. 版本选配与价格联动
  function updatePriceDisplay() {
    const edition = productData.editions[state.selectedEdition];
    const totalCurrent = edition.price * state.quantity;
    const totalOriginal = edition.originalPrice * state.quantity;

    if (currentPriceEl) currentPriceEl.textContent = `¥${totalCurrent}`;
    if (originalPriceEl) originalPriceEl.textContent = `¥${totalOriginal}`;
    if (stickyPriceEl) stickyPriceEl.textContent = `¥${totalCurrent}`;
  }

  editionCards.forEach(card => {
    card.addEventListener('click', () => {
      const editionKey = card.getAttribute('data-edition');
      if (!productData.editions[editionKey]) return;

      editionCards.forEach(c => c.classList.remove('selected'));
      card.classList.add('selected');
      state.selectedEdition = editionKey;

      updatePriceDisplay();
    });
  });

  // 5. 数量加减
  if (qtyMinus && qtyPlus && qtyInput) {
    qtyMinus.addEventListener('click', () => {
      if (state.quantity > 1) {
        state.quantity--;
        qtyInput.value = state.quantity;
        updatePriceDisplay();
      }
    });

    qtyPlus.addEventListener('click', () => {
      if (state.quantity < 99) {
        state.quantity++;
        qtyInput.value = state.quantity;
        updatePriceDisplay();
      }
    });
  }

  // 6. 吸顶购买栏滚动监听 (Intersection Observer)
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

  // 7. 购买弹窗交互
  function openBuyModal() {
    const colorInfo = productData.colors[state.selectedColor];
    const editionInfo = productData.editions[state.selectedEdition];
    const totalPrice = editionInfo.price * state.quantity;

    if (modalSummary) {
      modalSummary.innerHTML = `
        <div class="flex items-center gap-4 p-3 rounded-xl bg-white/5 border border-white/10 mb-4">
          <img src="${colorInfo.image}" class="w-16 h-16 object-cover rounded-lg border border-white/10" alt="Thumb" />
          <div>
            <div class="font-semibold text-white">VoxTriple 智键麦克风</div>
            <div class="text-xs text-neutral-400 mt-1">${editionInfo.title}</div>
            <div class="text-xs text-blue-400 mt-0.5">${colorInfo.name} × ${state.quantity}</div>
          </div>
        </div>
        <div class="text-xs text-neutral-400 space-y-1.5 leading-relaxed">
          <p>• 顺丰极速航空包邮，预计付款后 24 小时内发出</p>
          <p>• 支持 7 天无理由退换货与 1 年官方免费换新保障</p>
          <p>• 随包裹赠送专属开发指南与免驱即用快速入门手册</p>
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

  function closeBuyModal() {
    if (buyModal) {
      buyModal.classList.add('hidden');
      buyModal.classList.remove('flex');
    }
  }

  if (btnBuyNow) btnBuyNow.addEventListener('click', openBuyModal);
  if (stickyBuyBtn) stickyBuyBtn.addEventListener('click', openBuyModal);
  if (closeModalBtn) closeModalBtn.addEventListener('click', closeBuyModal);
  if (buyModal) {
    buyModal.addEventListener('click', (e) => {
      if (e.target === buyModal) closeBuyModal();
    });
  }

  if (modalConfirmBtn) {
    modalConfirmBtn.addEventListener('click', () => {
      showToast('🎉 预定意向已记录！感谢您对 VoxTriple 极客产品的支持。');
      closeBuyModal();
    });
  }

  // 8. FAQ 手风琴展开折叠
  const faqItems = document.querySelectorAll('.faq-item');
  faqItems.forEach(item => {
    const trigger = item.querySelector('.faq-trigger');
    if (trigger) {
      trigger.addEventListener('click', () => {
        const isOpen = item.classList.contains('open');
        // 关闭同级其他
        faqItems.forEach(i => i.classList.remove('open'));
        if (!isOpen) {
          item.classList.add('open');
        }
      });
    }
  });

  // 9. 轻量 Toast 提示
  function showToast(msg) {
    const toast = document.createElement('div');
    toast.className = 'fixed top-6 left-1/2 -translate-x-1/2 z-50 px-5 py-3 rounded-full bg-blue-600/90 text-white text-sm font-medium backdrop-blur-md shadow-2xl transition-all duration-300 pointer-events-none flex items-center gap-2 border border-blue-400/30';
    toast.innerHTML = `<span>${msg}</span>`;
    document.body.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translate(-50%, -20px)';
      setTimeout(() => toast.remove(), 300);
    }, 2800);
  }

  // 初始化价格
  updatePriceDisplay();
});
