(function() {
    if(window.__orion_element_picker)
    {
        return;
    }

    class OrionPicker extends HTMLElement {
        static blockingEvents = [
            'mousedown', 'mouseup', 
            'pointerdown', 'pointerup', 
            'auxclick', 'dblclick'
        ];
        constructor() {
            super();

            const shadowRoot = this.attachShadow({ mode: 'closed' });
            shadowRoot.innerHTML = `
                <style>
                    #highlight-box {
                        position: fixed;
                        pointer-events: none; /* Let clicks pass through to the page */
                        outline: 3px solid AccentColor;
                        outline-offset: -3px;
                        background-color: color(from AccentColor srgb r g b / 0.2);
                        display: none; /* Hidden until we hover something */
                        z-index: 2147483647;
                        transition: none;
                    }
                    #erased-highlights-box {
                        position: fixed;
                        width: 100%;
                        height: 100%;
                        pointer-events: none;
                        z-index: 2147483647;
                    }
                    .erased-element-highlight {
                        position: fixed;
                        outline: 3px dashed AccentColor;
                        outline-offset: -3px;
                        background-color: color(from AccentColor srgb r g b / 0.1);
                    }
                </style>
                <div id="highlight-box"></div>
                <div id="erased-highlights-box"></div>
            `;

            this.highlightBox = shadowRoot.getElementById('highlight-box');
            this.currentTarget = null;

            this.erasedSelectorsContainer = shadowRoot.getElementById('erased-highlights-box');
            this.persistentBoxes = [];
            this.elementToErasedSelector = new WeakMap();

            this.onMouseMove = this.onMouseMove.bind(this);
            this.onMouseOut = this.onMouseOut.bind(this);
            this.onClick = this.onClick.bind(this);
            this.onKeyDown = this.onKeyDown.bind(this);
            this.blockEvent = this.blockEvent.bind(this);
            this.fail = this.fail.bind(this);

            this.onScroll = this.onScroll.bind(this);
            this.onResize = this.onResize.bind(this);
            this.onMutation = this.onMutation.bind(this);

            this.updatePersistentPositions = this.updatePersistentPositions.bind(this);
        }

        connectedCallback() {
            Object.assign(this.style, {
                position: 'fixed',
                top: '0',
                left: '0',
                width: '100%',
                height: '100%',
                pointerEvents: 'auto',
                zIndex: '2147483647'
            });


            window.addEventListener('mousemove', this.onMouseMove, true);
            window.addEventListener('mouseout', this.onMouseOut, true);
            window.addEventListener('click', this.onClick, true);
            window.addEventListener('keydown', this.onKeyDown, true);
            document.addEventListener('visibilitychange', this.fail, true);
            window.addEventListener('pagehide', this.fail, true);
            window.addEventListener('popstate', this.fail, true);
            window.addEventListener('hashchange', this.fail, true);

            window.addEventListener('scroll', this.onScroll, { capture: true, passive: true });
            window.addEventListener('resize', this.onResize, { capture: true, passive: true });
            
            OrionPicker.blockingEvents.forEach(ev => {
                window.addEventListener(ev, this.blockEvent, true);
            });
        }

        disconnectedCallback() {
            window.removeEventListener('mousemove', this.onMouseMove, true);
            window.removeEventListener('mouseout', this.onMouseOut, true);
            window.removeEventListener('click', this.onClick, true);
            window.removeEventListener('keydown', this.onKeyDown, true);
            document.removeEventListener('visibilitychange', this.fail, true);
            window.removeEventListener('pagehide', this.fail, true);
            window.removeEventListener('visibilitychange', this.fail, true);
            window.removeEventListener('popstate', this.fail, true);
            window.removeEventListener('hashchange', this.fail, true);


            window.removeEventListener('scroll', this.onScroll, true);
            window.removeEventListener('resize', this.onResize, true);

            
            OrionPicker.blockingEvents.forEach(ev => {
                window.removeEventListener(ev, this.blockEvent, true);
            });

            if (document.body.style.cursor === 'crosshair') {
                document.body.style.cursor = '';
            }

            delete window.__orion_element_picker;
            window.webkit.messageHandlers.pickerComplete.postMessage(null);

            if(this.mutationObserver) {
                this.mutationObserver.disconnect();
            }

            if(this.mutationTimeout) {
                clearTimeout(this.mutationTimeout);
            }
        }

        blockEvent(e) {
            e.preventDefault();
            e.stopPropagation();
            e.stopImmediatePropagation();
        }

        onMouseMove(e) {
            this.style.pointerEvents = 'none';

            this.currentTarget = document.elementFromPoint(e.clientX, e.clientY);

            this.style.pointerEvents = 'auto';

            this.blockEvent(e);

            //safety precaution for if our shadow DOM holder is the hovered object
            if (this.currentTarget === this) return;

            if (this.currentTarget) {
                const rect = this.currentTarget.getBoundingClientRect();

                //shapes the highlightBox according to the element position
                this.highlightBox.style.display = 'block';
                this.highlightBox.style.top = `${rect.top}px`;
                this.highlightBox.style.left = `${rect.left}px`;
                this.highlightBox.style.width = `${rect.width}px`;
                this.highlightBox.style.height = `${rect.height}px`;
            }
            else
            {
                this.highlightBox.style.display = 'none';
            }
        }

        onMouseOut(e) {
            this.blockEvent(e);
            
            //entirely drops highlight upon losing mouse cursor
            if (!e.relatedTarget || e.relatedTarget.nodeName === 'HTML') {
                this.highlightBox.style.display = 'none';
                this.currentTarget = null;
            }
        }

        onClick(e) {
            this.blockEvent(e);

            if (!this.currentTarget)
            {
                //this.fail();
                return;
            }
            
            function createSelectorHeuristic(el) {
                if (!el || el.nodeType !== Node.ELEMENT_NODE) return '';

                const path = [];
                let current = el;



                while (current && current.nodeType === Node.ELEMENT_NODE && current.nodeName.toLowerCase() !== 'html') {
                    const tag = current.nodeName.toLowerCase();
                    let selector = tag;

                    //checks for a stable, globally unique id
                    if (isStable(current.id)) {
                        const escapedId = '#' + CSS.escape(current.id);
                        try {
                            if (document.querySelectorAll(escapedId).length === 1) {
                                path.unshift(escapedId);
                                break; // ID is globally unique, we are done
                            }
                        } catch (e) { /* Ignore invalid selectors */ }
                    }

                    //gathers all seemingly stable css classes
                    const validClasses = Array.from(current.classList).filter(isStable);
                    if (validClasses.length > 0) {
                        selector += '.' + validClasses.map(c => CSS.escape(c)).join('.');
                    }

                    //attempts to solve conflicts with siblings that have the same tag+class signature by appending :nth-child
                    if (tag !== 'body' && tag !== 'head') {
                        let requiresNthChild = false;
                        let sibling = current.parentNode ? current.parentNode.firstElementChild : null;
                        let index = 1;
                        let myIndex = 1;

                        while (sibling) {
                            if (sibling === current) {
                                myIndex = index;
                            } else if (sibling.nodeName.toLowerCase() === tag) {
                                const siblingHasSameClasses = validClasses.every(c => sibling.classList.contains(c));
                                if (siblingHasSameClasses) {
                                    requiresNthChild = true;
                                }
                            }
                            index++;
                            sibling = sibling.nextElementSibling;
                        }

                        if (requiresNthChild) {
                            selector += `:nth-child(${myIndex})`;
                        }
                    }

                    path.unshift(selector);

                    //early exit if globally unique
                    //TODO: probably force this to go a bit further
                    // try {
                    //     const currentPathStr = path.join(' > ');
                    //     if (document.querySelectorAll(currentPathStr).length === 1) {
                    //         break;
                    //     }
                    // } catch (e) {}

                    current = current.parentNode;
                }

                return path.join(' > ');

                //heuristic: checks for signs that an id/css classname is not stable between page instances
                function isStable(name) {
                    
                    if (!name || typeof name !== 'string') return false;
                    
                    //strips away whitespace, leading '.', '#'
                    const clean = name.trim().replace(/^[.#]/, '');
                    if (!clean) return false;
                    
                    //CSS-in-JS / bundler-generated prefixes (styled-components, emotion, JSS, generic "css-" hashes, chakra)
                    if (/^(css|sc|jss\d*|emotion|styled|chakra)-[a-z0-9]+$/i.test(clean)) return false;
                    
                    //angular view-encapsulation artifacts
                    if (/^_ngcontent-|^_nghost-|^ng-tns-/i.test(clean)) return false;

                    //vue SFC CSS (scoped style);
                    if (/^data-v-[a-f0-9]+$/i.test(clean)) return false;
                    
                    //uuid
                    if (/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(clean)) return false;
                    
                    //react CSS modules style trailing hash: Component_class__1a2b3c
                    //(only flagged if the suffix itself looks random, so plain words like "card__title" aren't caught)
                    const modulesMatch = clean.match(/(?:__|--)([a-z0-9]{5,8})$/i);
                    if(modulesMatch && looksRandom(modulesMatch[1])) return false;

                    //trailing numeric sequence that looks auto-generated, likely instance specific then
                    if (/\d{4,}$/.test(clean)) return false; //4+ numeric chars at end
                    if (/^\d+$/.test(clean)) return false; //wholly numeric
                    
                    //split by dashes and underscores, filter out any "falsy" values from resulting array
                    const parts = clean.split(/[-_]/).filter(Boolean);

                    //last part seems to be a hash
                    if (parts.length >= 1 && looksRandom(parts[parts.length - 1])) return false;
                    
                    
                    //transient UI-state class names
                    //const statePrefixPattern = /^(is|has|js)[-_](active|open|closed|expanded|collapsed|visible|hidden|selected|checked|disabled|loading|error|dragging|focus(ed)?|hover(ed)?|show(n)?|hide)$/i;
                    const statePrefixPattern = /^(is|has|js|with|no|active|open|show|hide|selected|disabled|expanded|collapsed)/
                    const bareStateWords = new Set([
                        'hover', 'active', 'focus', 'focused', 'visited', 'disabled',
                        'selected', 'checked', 'open', 'closed', 'expanded', 'collapsed',
                        'show', 'shown', 'hide', 'hidden', 'loading', 'error', 'success',
                        'dragging', 'current', 'pressed',
                    ]);
                    if (statePrefixPattern.test(clean) || bareStateWords.has(clean.toLowerCase())) return false;
                    
                    return true;

                    /**
                     * Rough "does this look like random noise rather than a word" check.
                     * Flags pure hex strings (typical hash output) and mixed alnum strings
                     * with an unusually low vowel ratio.
                     */
                    function looksRandom(str) {
                        if (!str || str.length < 5) return false; //short segments are usually real words (btn, nav, sm)
                        
                        const hasDigit = /\d/.test(str);
                        const hasLetter = /[a-z]/i.test(str);
                        const hexOnly = /^[0-9a-f]+$/i.test(str);
                        
                        if (hexOnly && hasDigit && str.length >= 6) return true;
                        
                        if (hasDigit && hasLetter) {
                            const vowels = (str.match(/[aeiou]/gi) || []).length;
                            if (vowels / str.length < 0.15) return true;
                        
                        }
                    
                        return false;
                    }
                }
            }

            const existingSelector = this.elementToErasedSelector.get(this.currentTarget);
    
            const elementData = existingSelector
                ? {
                    erase: false,
                    selector: existingSelector,
                    host: window.location.hostname,
                }
                : {
                    erase: true,
                    selector: createSelectorHeuristic(this.currentTarget),
                    host: window.location.hostname,
                };

            window.webkit.messageHandlers.pickerComplete.postMessage(elementData);
            //this.remove();
        }

        onKeyDown(e) {
            this.blockEvent(e);
            if (e.key === 'Escape') {
                this.fail();
            }
        }

        onScroll() {
            this.queueUpdatePersistentPositions();
        }
    
        onResize() {
            this.queueUpdatePersistentPositions();
        }

        onMutation(mutations) {
            //check to see whether this callback is actually from this OrionPicker widget
            let isExternalMutation = false;
            
            for (let i = 0; i < mutations.length; i++) {
                if (mutations[i].target !== this) {
                    isExternalMutation = true;
                    break;
                }
            }

            if(isExternalMutation) {
                if (this.mutationTimeout) {
                    clearTimeout(this.mutationTimeout);
                }

                //100 ms debounce to wait for settling
                this.mutationTimeout = setTimeout(() => {
                    requestAnimationFrame(() => {
                        if (this.savedSelectors.length > 0) {
                            this.refreshErasedElementHighlights();
                        }
                    })
                }, 100);
            }
        }


        fail() {
            this.remove();
        }

        updatePersistentPositions() {
            const updates = [];

            for (let i = 0; i < this.persistentBoxes.length; i++) {
                const { box, element } = this.persistentBoxes[i];
                if (!element.isConnected) {
                    updates.push({ box, display: 'none' });
                    continue;
                }
    
                const rect = element.getBoundingClientRect();
                if (rect.width === 0 && rect.height === 0) {
                    updates.push({ box, display: 'none' });
                    continue;
                }
                updates.push({
                    box,
                    display: 'block',
                    width: rect.width,
                    height: rect.height,
                    x: rect.left,
                    y: rect.top
                });
            }
            
            for (let i = 0; i < updates.length; i++) {
                const update = updates[i];
                const style = update.box.style;

                style.display = update.display;
                
                if (update.display === 'block') {
                    style.width = `${update.width}px`;
                    style.height = `${update.height}px`;
                    //transform should ideally make for better hardware acceleration
                    style.transform = `translate(${update.x}px, ${update.y}px)`;
                }
            }

            this.updateQueued = false;
        }

        queueUpdatePersistentPositions() {
            if(!this.updateQueued && this.persistentBoxes.length)
            {
                requestAnimationFrame(this.updatePersistentPositions);
                this.updateQueued = true;
            }
        }

        refreshErasedElementHighlights() {
            this.erasedSelectorsContainer.innerHTML = '';
            this.elementToErasedSelector = new WeakMap();
            this.persistentBoxes = [];


            if(this.savedSelectors)
            {
                this.savedSelectors.forEach(selector => {
                    const elements = document.querySelectorAll(selector);

                    elements.forEach(el => {
                        const box = document.createElement('div');
                        box.className = 'erased-element-highlight';

                        this.erasedSelectorsContainer.appendChild(box);

                        this.elementToErasedSelector.set(el, selector);
                        this.persistentBoxes.push({box, element: el});
                    });
                })
                this.updatePersistentPositions();
            }
        }


        setErasedSelectors(selectorString) {
            
            function splitSelectors(selector) {
                const result = [];
                if(!selector) return result;
                let current = '';
                
                let parens = 0;
                let brackets = 0;
                let singleQuote = false;
                let doubleQuote = false;
                let isEscaped = false;

                for (let i = 0; i < selector.length; i++) {
                    const char = selector[i];

                    // Handle escaped characters (e.g., \, or \")
                    if (isEscaped) {
                        current += char;
                        isEscaped = false;
                        continue;
                    }

                    switch (char) {
                        case '\\':
                            isEscaped = true;
                            break;
                        case "'":
                            if (!doubleQuote) singleQuote = !singleQuote;
                            break;
                        case '"':
                            if (!singleQuote) doubleQuote = !doubleQuote;
                            break;
                        case '(':
                            if (!singleQuote && !doubleQuote) parens++;
                            break;
                        case ')':
                            if (!singleQuote && !doubleQuote) parens--;
                            break;
                        case '[':
                            if (!singleQuote && !doubleQuote) brackets++;
                            break;
                        case ']':
                            if (!singleQuote && !doubleQuote) brackets--;
                            break;
                        case ',':
                            // Only split if we are strictly at the top level
                            if (!singleQuote && !doubleQuote && parens === 0 && brackets === 0) {
                            if (current.trim()) result.push(current.trim());
                            current = '';
                            continue; // Skip adding the comma to the current string
                            }
                            break;
                    }
                    
                    current += char;
                }

                // Push the final segment
                if (current.trim()) {
                    result.push(current.trim());
                }

                return result;
            }

            this.savedSelectors = splitSelectors(selectorString);
            if(this.savedSelectors)
            {
                if(this.mutationObserver) {
                    this.mutationObserver.disconnect();
                }
                this.mutationObserver = new MutationObserver(this.onMutation);
                this.mutationObserver.observe(document.body, {
                    childList: true,
                    subtree: true,
                    attributes: true,
                    attributeFilter: ['class', 'style', 'hidden']
                });
                this.refreshErasedElementHighlights();
            }

        }
    }

    if(!customElements.get('orion-element-picker')) {
        customElements.define('orion-element-picker', OrionPicker);
    }

    const picker = document.createElement('orion-element-picker');
    document.body.appendChild(picker);

    window.__orion_element_picker = picker;
})();