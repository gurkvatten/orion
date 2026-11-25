#include "autofill_scripts.h"

namespace orion::gtk {

std::string
get_autofill_script()
{
    return R"(
// autofill-user-script.js
(() => {
  if (window.__autofill_installed) return;
  window.__autofill_installed = true;

  // Utilities
  function isVisible(el) {
    if (!el) return false;
    const style = window.getComputedStyle(el);
    return style && style.display !== 'none' && style.visibility !== 'hidden' && el.offsetParent !== null;
  }

  function triggerEvents(el) {
    try {
      // for native events
      const evInput = new Event('input', { bubbles: true, cancelable: true });
      const evChange = new Event('change', { bubbles: true, cancelable: true });
      el.dispatchEvent(evInput);
      el.dispatchEvent(evChange);

      // React compatibility: set native value then dispatch events
      const setValue = Object.getOwnPropertyDescriptor(el.constructor.prototype, 'value')?.set;
      if (setValue) {
        setValue.call(el, el.value);
        el.dispatchEvent(evInput);
      }
    } catch (e) {
      console.warn('autofill: triggerEvents error', e);
    }
  }

  // Walk shadow roots & nodes to find inputs
  function *deepQuerySelectors(root, selector) {
    if (!root) return;
    const nodes = root.querySelectorAll(selector);
    for (const n of nodes) yield n;

    // traverse shadow roots
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, null, false);
    let node;
    while (node = walker.nextNode()) {
      if (node.shadowRoot) {
        const nested = node.shadowRoot.querySelectorAll(selector);
        for (const n of nested) yield n;
      }
    }
  }

  function findPasswordField() {
    // Most reliable: input[type=password], visible, not disabled
    for (const p of deepQuerySelectors(document, 'input[type="password"]')) {
      if (p && !p.disabled && isVisible(p)) return p;
    }
    // fallback: elements with password-like names
    const candidateNames = ['pass', 'pwd', 'password'];
    for (const name of candidateNames) {
      for (const el of deepQuerySelectors(document, `input[name*="${name}"], input[id*="${name}"]`)) {
        if (!el.disabled && isVisible(el)) return el;
      }
    }
    return null;
  }

  function getLabelsForInput(input) {
    if (!input) return [];
    if (input.labels && input.labels.length) return Array.from(input.labels).map(l => l.textContent.trim());
    // try to find <label for="id">
    const id = input.id;
    if (id) {
      const lab = document.querySelector(`label[for="${id}"]`);
      if (lab) return [lab.textContent.trim()];
    }
    return [];
  }

  function matchUsernameCandidates() {
    const selectors = [
      'input[autocomplete="username"]',
      'input[autocomplete="email"]',
      'input[type="email"]',
      'input[type="text"][autocomplete="username"]'
    ];
    for (const sel of selectors) {
      for (const el of deepQuerySelectors(document, sel)) {
        if (!el.disabled && isVisible(el)) return el;
      }
    }

    // heuristics by name/id/placeholder/labels
    const userHints = ['user', 'login', 'email', 'account', 'uname', 'id'];
    const inputs = [];
    for (const el of deepQuerySelectors(document, 'input[type="text"], input[type="email"], input:not([type])')) {
      if (el.disabled || !isVisible(el)) continue;
      const combined = `${el.name || ''} ${el.id || ''} ${el.placeholder || ''} ${getLabelsForInput(el).join(' ')}`.toLowerCase();
      for (const hint of userHints) {
        if (combined.includes(hint)) {
          inputs.push({el, score: combined.split(hint).length});
          break;
        }
      }
    }

    if (inputs.length) {
      // prefer highest score (more matches)
      inputs.sort((a, b) => b.score - a.score);
      return inputs[0].el;
    }

    // last-resort: first visible input before password in same form
    return null;
  }

  function findBestUsernameForPassword(passEl) {
    if (!passEl) return null;

    // if inside a <form>, search within form (prefer)
    const form = passEl.form;
    if (form) {
      // candidates inside form
      const candidates = [];
      for (const el of form.querySelectorAll('input')) {
        if (el === passEl) continue;
        if (el.type && ['hidden', 'password', 'submit', 'button', 'image', 'file'].includes(el.type)) continue;
        if (el.disabled || !isVisible(el)) continue;
        candidates.push(el);
      }
      // prefer autocomplete attributes
      for (const c of candidates) {
        const ac = c.getAttribute('autocomplete') || '';
        if (/(username|email)/i.test(ac)) return c;
      }
      // heuristics by name/id/placeholder/label
      if (candidates.length) {
        const scored = candidates.map(el => {
          const combined = `${el.name||''} ${el.id||''} ${el.placeholder||''} ${getLabelsForInput(el).join(' ')}`.toLowerCase();
          const score = (combined.match(/user|email|login|account|id|uname/g) || []).length;
          return {el, score};
        }).sort((a,b)=>b.score-a.score);
        if (scored[0].score > 0) return scored[0].el;
        // fallback: return first visible text/email input
        return candidates.find(el => el.type === 'email') || candidates.find(el => el.type === 'text') || candidates[0];
      }
    }

    // else no form: try site-wide heuristics
    const byAutocomplete = matchUsernameCandidates();
    if (byAutocomplete) return byAutocomplete;

    // try nearest input above the password in document order
    // find all inputs and pick the nearest prior element
    const inputs = Array.from(document.querySelectorAll('input')).filter(i => !i.disabled && isVisible(i));
    const passIndex = inputs.indexOf(passEl);
    if (passIndex > 0) return inputs[passIndex - 1];

    return null;
  }

  function setValueAndNotify(el, value) {
    if (!el) return;
    try {
      // set value
      el.focus();
      el.value = value;
      // React: try to set internal value
      const proto = Object.getOwnPropertyDescriptor(el.constructor.prototype, 'value');
      if (proto && proto.set) {
        proto.set.call(el, value);
      }
      triggerEvents(el);
      el.blur();
    } catch (e) {
      try {
        // fallback for contentEditable
        if (el.isContentEditable) {
          el.textContent = value;
          triggerEvents(el);
        }
      } catch (ee) { console.warn('autofill setValue failed', ee); }
    }
  }

  // Primary fill routine
  async function autofillWith(username, password, options = {}) {
    const {autoSubmit = false, maxRetries = 5, skipUsername = false, retryDelay = 300} = options;
    let attempts = 0;

    while (attempts++ < maxRetries) {
      const passEl = findPasswordField();
      if (!passEl) {
        // wait and retry
        await new Promise(r => setTimeout(r, retryDelay));
        continue;
      }

      const userEl = findBestUsernameForPassword(passEl) || matchUsernameCandidates();
      // If still missing username, try searching globally for first text/email input before password
      let finalUser = userEl;
      if (!finalUser) {
        // text or email inputs in document order before password
        const inputs = Array.from(document.querySelectorAll('input')).filter(i => !i.disabled && isVisible(i) && (i.type === 'text' || i.type === 'email' || !i.type));
        const passIndex = inputs.indexOf(passEl);
        if (passIndex > 0) finalUser = inputs[passIndex - 1];
      }

      // apply values
      if (finalUser && !skipUsername) setValueAndNotify(finalUser, username);
      setValueAndNotify(passEl, password);

      // mark fields to indicate autofill (so page scripts can know)
      try {
        if (finalUser) finalUser.dataset.autofilled = '1';
        passEl.dataset.autofilled = '1';
      } catch (e) {}

      // optionally submit
      if (autoSubmit) {
        try {
          const form = passEl.form;
          if (form) {
            form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
            // prefer calling submit() after dispatching
            if (typeof form.submit === 'function') form.submit();
          } else {
            // try clicking a submit button near the password
            const nextSubmit = (el => {
              // search descendants of the same parent
              const parent = el.parentElement;
              if (!parent) return null;
              const btn = parent.querySelector('button[type=submit], input[type=submit]');
              if (btn && isVisible(btn)) return btn;
              // fallback: search whole document
              const anyBtn = document.querySelector('button[type=submit], input[type=submit]');
              return anyBtn;
            })(passEl);
            if (nextSubmit) nextSubmit.click();
          }
        } catch (e) { console.warn('autofill submit failed', e); }
      }

      return {filled: true, usernameField: !!finalUser, passwordField: true};
    }

    return {filled: false};
  }

  // Listen for app-provided credentials via CustomEvent
  function handleAutofillEvent(e) {
    try {
      document.removeEventListener('focusin', autofillFocusHandler); // block focus events during autofill
      const detail = e.detail || {};
      const username = detail.username || '';
      const password = detail.password || '';
      const autoSubmit = !!detail.autoSubmit;
      const maxRetries = detail.maxRetries || 6;
      const skipUsername = !!detail.skipUsername;
      autofillWith(username, password, {autoSubmit, maxRetries, skipUsername})
        .then(result => {
          // Optionally notify the app by dispatching another event with the result
          window.dispatchEvent(new CustomEvent('Autofill:result', {detail: result}));
        });
    } catch (err) {
      console.warn('autofill handler error', err);
    } finally {
      document.addEventListener('focusin', autofillFocusHandler); // unblock focus events
    }
  }

  window.addEventListener('Autofill:fill', handleAutofillEvent, false);

  // Also expose a direct callable function
  window.Autofill = {
    fill: (username, password, opts) => autofillWith(username, password, opts || {})
  };

})();
)";
}

std::string
get_input_focus_script()
{
    return R"(
function isUsernameField(el) {
    if (el.tagName !== 'INPUT') return false;

    const type = (el.type || '').toLowerCase();
    if (type === 'password') return false; // ignore password
    if (type !== 'text' && type !== 'email') return false;

    // 1. Official HTML standard: autocomplete="username"
    if ((el.autocomplete || '').toLowerCase() === 'username') {
        return true;
    }

    // 2. Common name/id patterns
    const name = (el.name || '').toLowerCase();
    const id = (el.id || '').toLowerCase();

    const usernameKeywords = [
        'user', 'username', 'login', 'userid', 'user_id', 'account', 'email'
    ];

    if (usernameKeywords.some(k => name.includes(k) || id.includes(k))) {
        return true;
    }

    // 3. Placeholder/aria-label text
    const label = (el.placeholder || el.getAttribute('aria-label') || '').toLowerCase();
    if (usernameKeywords.some(k => label.includes(k))) {
        return true;
    }

    return false;
}

function autofillFocusHandler(e) {
    if (!e.target) return;
    const el = e.target;
    if (!el || el.tagName !== 'INPUT') return;

    const htmlType = (el.type || '').toLowerCase();
    if (htmlType !== 'text' && htmlType !== 'email' && htmlType !== 'password') return;

    let type = 'other';
    if (isUsernameField(el)) {
        type = 'username';
    } else if (htmlType === 'password') {
        type = 'password';
    }

    const rect = el.getBoundingClientRect();

    window.webkit.messageHandlers.inputFocus.postMessage({
        type: type,
        tag: el.tagName,
        htmlType: htmlType,
        x: rect.x + window.scrollX,
        y: rect.y + window.scrollY,
        width: rect.width,
        height: rect.height,
        scrollX: window.scrollX,
        scrollY: window.scrollY
    });
}

document.addEventListener('focusin', autofillFocusHandler);
)";
}

std::string
get_form_submit_script()
{
    return R"(
(function() {

    function extractCredentials(form) {
        let username = "";
        let password = "";

        // All inputs inside the form
        const inputs = form.querySelectorAll("input");

        for (const input of inputs) {
            const type = (input.type || "").toLowerCase();

            if (!username && (type === "text" || type === "email" || input.autocomplete === "username")) {
                username = input.value || "";
            }

            if (!password && type === "password") {
                password = input.value || "";
            }
        }

        return { username, password };
    }

    // Hook all forms
    for (const form of document.forms) {
        form.addEventListener("submit", function(e) {
            const creds = extractCredentials(form);

            window.webkit.messageHandlers.formSubmit.postMessage({
                username: creds.username,
                password: creds.password
            });

            // NOTE:
            // If you want to ALLOW the form submission, do nothing.
            // If you want to prevent (for testing), uncomment:
            // e.preventDefault();
        }, true);
    }

    document.addEventListener("submit", submitHandler, true);

    document.addEventListener("click", function(e) {
        if (e.target.closest("button[type=submit]")) {
            submitHandler(e);
        }
    }, true);

    function submitHandler(e) {
        const username = document.querySelector("input[type=text],input[type=email]")?.value || "";
        const password = document.querySelector("input[type=password]")?.value || "";

        window.webkit.messageHandlers.formSubmit.postMessage({ username, password });
    }
})();
)";
}

std::string
get_scrape_username_password_script()
{
    return R"(
(() => {
  const result = {
    username: null,
    password: null
  };

  const inputs = Array.from(document.querySelectorAll("input"));

  for (const el of inputs) {
    const type = (el.type || "").toLowerCase();
    const name = (el.name || "").toLowerCase();
    const id   = (el.id || "").toLowerCase();
    const ac   = (el.autocomplete || "").toLowerCase();

    if (!result.password && type === "password") {
      if (el.value)
        result.password = el.value;
    }

    if (!result.username && type !== "password") {
      if (
        ac === "username" ||
        name.includes("user") ||
        name.includes("email") ||
        id.includes("user") ||
        id.includes("email")
      ) {
        if (el.value)
          result.username = el.value;
      }
    }
  }

  return result;
})();
)";
}

} // namespace orion::gtk