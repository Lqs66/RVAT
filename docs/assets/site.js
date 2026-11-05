(() => {
  'use strict';
  document.documentElement.classList.add('js');

  const toggle = document.querySelector('.nav-toggle');
  const navigation = document.getElementById('site-nav');
  if (toggle && navigation) {
    toggle.addEventListener('click', () => {
      const open = toggle.getAttribute('aria-expanded') !== 'true';
      toggle.setAttribute('aria-expanded', String(open));
      navigation.classList.toggle('is-open', open);
    });
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && toggle.getAttribute('aria-expanded') === 'true') {
        toggle.setAttribute('aria-expanded', 'false');
        navigation.classList.remove('is-open');
        toggle.focus();
      }
    });
  }

  document.querySelectorAll('[data-math]').forEach(element => {
    if (typeof window.katex !== 'undefined') {
      window.katex.render(element.dataset.math, element, {
        displayMode: element.dataset.display === 'true',
        throwOnError: false,
        strict: 'ignore'
      });
    }
  });

  document.querySelectorAll('.document-content pre').forEach(block => {
    const code = block.querySelector('code');
    if (!code || !navigator.clipboard) return;
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'copy-button';
    button.textContent = 'Copy';
    button.setAttribute('aria-label', 'Copy code to clipboard');
    button.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(code.textContent);
        button.textContent = 'Copied';
      } catch {
        button.textContent = 'Copy unavailable';
      }
      window.setTimeout(() => { button.textContent = 'Copy'; }, 2000);
    });
    block.prepend(button);
  });

  document.querySelectorAll('.document-content table').forEach(table => {
    if (table.parentElement.classList.contains('table-scroll')) return;
    const wrapper = document.createElement('div');
    wrapper.className = 'table-scroll';
    wrapper.tabIndex = 0;
    wrapper.setAttribute('role', 'region');
    wrapper.setAttribute('aria-label', 'Scrollable table');
    table.before(wrapper);
    wrapper.append(table);
  });

  const results = document.getElementById('testing-results');
  if (results && results.tHead) {
    const updateHeader = () => {
      results.style.setProperty('--group-header-height', `${results.tHead.rows[0].getBoundingClientRect().height}px`);
    };
    updateHeader();
    if ('ResizeObserver' in window) new ResizeObserver(updateHeader).observe(results.tHead.rows[0]);
    else window.addEventListener('resize', updateHeader);
  }

  const images = document.querySelectorAll('.document-content img');
  if (images.length && typeof HTMLDialogElement !== 'undefined') {
    const dialog = document.createElement('dialog');
    dialog.className = 'image-dialog';
    dialog.setAttribute('aria-label', 'Expanded figure');
    const header = document.createElement('div');
    header.className = 'image-dialog-header';
    const source = document.createElement('a');
    source.textContent = 'Open original figure ↗';
    source.target = '_blank';
    source.rel = 'noopener';
    const close = document.createElement('button');
    close.type = 'button';
    close.className = 'image-dialog-close';
    close.textContent = 'Close';
    header.append(source, close);
    const content = document.createElement('div');
    content.className = 'image-dialog-content';
    const expanded = document.createElement('img');
    content.append(expanded);
    dialog.append(header, content);
    document.body.append(dialog);
    close.addEventListener('click', () => dialog.close());
    dialog.addEventListener('click', event => {
      if (event.target !== dialog) return;
      const bounds = dialog.getBoundingClientRect();
      if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) dialog.close();
    });
    images.forEach(image => {
      if (image.closest('a, button')) return;
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'image-expand';
      button.setAttribute('aria-label', `Enlarge figure: ${image.alt || 'diagram'}`);
      image.before(button);
      button.append(image);
      button.addEventListener('click', () => {
        expanded.src = image.currentSrc || image.src;
        expanded.alt = image.alt;
        source.href = expanded.src;
        dialog.showModal();
        content.scrollTo(0, 0);
      });
    });
  }
})();
