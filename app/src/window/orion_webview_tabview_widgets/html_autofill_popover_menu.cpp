#include <string>

namespace orion::gtk {

std::string
get_html_autofill_popover_menu_script()
{
    return R"(

// ──────────────────────────────────────────────
//   Data
// ──────────────────────────────────────────────
const suggestions = [
  "Apple", "Apricot", "Avocado", "Banana"
];

// ──────────────────────────────────────────────
//   Inject CSS once
// ──────────────────────────────────────────────
function injectStyles() {
  // Prevent injecting multiple times
  if (document.getElementById('dynamic-popover-styles')) return;

  const style = document.createElement('style');
  style.id = 'dynamic-popover-styles';
  style.textContent = `
    .popover-menu {
      position: absolute;
      z-index: 10000;
      background: white;
      border: 1px solid #d0d0d0;
      border-radius: 10px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.18);
      max-height: 280px;
      overflow-y: auto;
      list-style: none;
      padding: 8px 0;
      margin: 0;
      display: none;
      min-width: 200px;
    }

    .popover-menu.visible {
      display: block;
    }

    .popover-menu li {
      padding: 10px 18px;
      cursor: pointer;
      font-size: 15px;
      user-select: none;
    }

    .popover-menu li:hover,
    .popover-menu li.selected {
      background: #e6f2ff;
    }

    .popover-menu li.empty {
      color: #777;
      cursor: default;
      font-style: italic;
    }

    .popover-menu::-webkit-scrollbar {
      width: 6px;
    }
    .popover-menu::-webkit-scrollbar-thumb {
      background: #bbb;
      border-radius: 3px;
    }

    /* Focus style for input */
    input:focus {
      border-color: #006edc;
      box-shadow: 0 0 0 3px rgba(0, 110, 220, 0.2);
    }
  `;
  document.head.appendChild(style);
}

// ──────────────────────────────────────────────
//   Create popover element dynamically
// ──────────────────────────────────────────────
function createPopoverMenu() {
  const menu = document.createElement('ul');
  menu.className = 'popover-menu';
  menu.setAttribute('role', 'listbox');
  menu.setAttribute('aria-label', 'Suggestions');
  menu.setAttribute('tabindex', '-1'); // not focusable itself
  return menu;
}

// ──────────────────────────────────────────────
//   State & helpers
// ──────────────────────────────────────────────
let popover = null;
let activeIndex = -1;
let filtered = [];

// Get or create popover
function getPopover() {
  if (!popover) {
    popover = createPopoverMenu();
  }
  return popover;
}

function positionAndAttach(input) {
  const rect = input.getBoundingClientRect();
  const menu = getPopover();

  // Attach to the nearest positioned ancestor (the wrapper)
  const wrapper = input.parentElement;
  if (!wrapper.contains(menu)) {
    wrapper.appendChild(menu);
  }

  menu.style.left = '0px';
  menu.style.width = `${rect.width}px`;

  // Default: below the input
  let top = rect.height + 6;

  // If not enough space below → open above
  const spaceBelow = window.innerHeight - rect.bottom;
  if (spaceBelow < 280 && rect.top > 280) {
    top = - (menu.offsetHeight + 6 || 280 + 6); // approximate height if not yet rendered
  }

  menu.style.top = `${top}px`;
}

// ──────────────────────────────────────────────
//   Render & update
// ──────────────────────────────────────────────
function renderItems(items) {
  const menu = getPopover();
  menu.innerHTML = '';

  if (items.length === 0) {
    const li = document.createElement('li');
    li.textContent = 'No matches found';
    li.className = 'empty';
    menu.appendChild(li);
    return;
  }

  items.forEach((text, i) => {
    const li = document.createElement('li');
    li.textContent = text;
    li.setAttribute('role', 'option');
    li.dataset.value = text;

    li.addEventListener('click', () => {
      input.value = text;
      hidePopover();
      input.focus();
    });

    menu.appendChild(li);
  });
}

function highlightItem(index) {
  const items = getPopover().querySelectorAll('li:not(.empty)');
  items.forEach((li, i) => {
    li.classList.toggle('selected', i === index);
    if (i === index) {
      li.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
  });
  activeIndex = index;
}

function showPopover() {
  getPopover().classList.add('visible');
}

function hidePopover() {
  if (popover) {
    popover.classList.remove('visible');
    activeIndex = -1;
    popover.querySelectorAll('li').forEach(li => li.classList.remove('selected'));
  }
}

function setEventListeners(input) {
    input.addEventListener('input', (e) => {
    // const value = e.target.value.trim().toLowerCase();

    // if (!value) {
    //     hidePopover();
    //     return;
    // }

    filtered = suggestions//.filter(item => item.toLowerCase().includes(value));

    renderItems(filtered);
    activeIndex = -1;

    if (filtered.length > 0) {
        positionAndAttach(input);
        showPopover();
    } else {
        hidePopover();
    }
    });
}

/*
    // TEST html popover:
    // Inject styles on first run
    injectStyles();
    const passEl = findPasswordField();
    const userEl = findBestUsernameForPassword(passEl) || matchUsernameCandidates();
    setEventListeners(userEl);
    return;
*/

)";
}

} // namespace orion::gtk