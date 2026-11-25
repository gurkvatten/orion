function runReadabilityScript() {

    window.webkit.messageHandlers.readerData.postMessage({
        epoch: window.__epoch,
        readerData: checkReadability()
    });
}

(function() {
    if("%READER-EPOCH%" != window.__epoch)
    {
        return;
    }

    function isDomReady() {
        return document.body && document.body.innerText.length > 800;
    }
    
    let debounceTimer;

    function resetDebounceTimer(timeout) {
        clearTimeout(debounceTimer);

        debounceTimer = setTimeout(() => {
            observer.disconnect();
            clearTimeout(debounceTimer);
            runReadabilityScript();
        }, timeout);
    }

    const observerCallback = function(observer) {
        resetDebounceTimer(300);
    };
    const observer = new MutationObserver(observerCallback);
    const config = { childList: true, subtree: true };

    if (isDomReady()) {
        runReadabilityScript();
    } else {
        resetDebounceTimer(800);
        
        if (document.body) {
            observer.observe(document.body, config);
        } else {
            // Fallback if body doesn't exist yet
            document.addEventListener('DOMContentLoaded', () => {
                resetDebounceTimer(800);
                observer.observe(document.body, config);
            });
        }
    }
})();