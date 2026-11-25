//NOTE: all linux specific edits are marked with EDIT, ENDEDIT

if (typeof OrionReader !== "function") {
    class OrionReader {
        
        constructor() {
            this.MaximumWidthOfImageOrVideoExtendingBeyondTextContainer = 1300;
            this._textSizeIndex = %READER-FONTSIZE-INDEX%;
            this._fontFamilyName = "%READER-FONTNAME%";
            this.setCurrentTextSizeIndex(this._textSizeIndex);
        }
        
        parse() {
            this.images = document.getElementsByTagName("img");
            
            %READER-SCRIPT-1%
            
            %READER-SCRIPT-2%
            
            %READER-SCRIPT-3%
            
            // replacing <header> with <h2 class="subhead">
            var header = article.getElementsByClassName("page")[0].getElementsByTagName("div")[1].getElementsByTagName("header")[0]
            if (typeof header !== "undefined") {
                // removing svg header from techcrunch articles
                if (%READER-TECHCRUNCH-CONDITION%) {
                    if (header.innerHTML.includes("<svg")) {
                        header.parentElement.removeChild(header);
                    }
                    
                } else {
                    var h2 = document.createElement("h2");
                    h2.innerHTML = header.innerHTML;
                    h2.className = "subhead";
                    header.parentNode.replaceChild(h2, header);
                }
            }
            
            if (%READER-TECHCRUNCH-CONDITION%) {
                const svgs = article.getElementsByTagName("svg");
                for (var i = 0; i < svgs.length; i++) {
                    svgs[i].parentElement.removeChild(svgs[i]);
                }
            }
            
            // used to appear as a raw text on bbc.com pages
            const spans = document.getElementsByTagName("span");
            for (var i = 0; i < spans.length; i++) {
                if ((spans[i].innerText == "image copyright") || (spans[i].innerText == "image caption")) {
                    spans[i].parentNode.removeChild(spans[i]);
                    i -= 1;
                }
            }
            
            const pres = document.getElementsByTagName("pre");
            for (var i = 0; i < pres.length; i++) {
                var p = document.createElement("p");
                p.innerHTML = pres[i].innerHTML;
                pres[i].parentNode.replaceChild(p, pres[i]);
            }
        }
        
        fixImages() {
            this.originalArticleWidth = '800px';
            
            %READER-SCRIPT-4%
            
            for (var i = 0; i < this.images.length; i++) {
                if ((typeof this.images[i].attributes["src"] !== "undefined") && (this.images[i].attributes.src.value.length > 0)) {
                    this.images[i].removeAttribute("srcset");
                    this.images[i].removeAttribute("sizes");
                }
                
                if ((this.images[i].width < 32) && (this.images[i].height < 32)) {
                    this.images[i].setAttribute("class", "reader-image-tiny");
                }
                this.images[i].removeAttribute("border");
                this.images[i].removeAttribute("hspace");
                this.images[i].removeAttribute("vspace");
                
                var Fe = this.images[i].getAttribute("align");
                if (this.images[i].removeAttribute("align"), "left" !== Fe && "right" !== Fe || (this.images[i].classList.add("float"))) {};
                
                if ((typeof this.images[i].attributes["src"].length === "undefined") && (typeof this.images[i].attributes["data-lazy-src"] !== "undefined")) {
                    this.images[i].setAttribute("src", this.images[i].attributes["data-lazy-src"].textContent);
                    
                    var attributeNames = [];
                    for (var j = 0; j < this.images[i].attributes.length; j++) {
                        let name = this.images[i].attributes[j].name;
                        if (name.startsWith("data-")) {
                            attributeNames.push(name);
                        }
                    }
                    for (let t of attributeNames)
                        this.images[i].removeAttribute(t);
                }
            }
        }
        
        bodyFontColor() {
            return window.getComputedStyle(document.body).getPropertyValue('--body-font-color');
        }
        
        documentElementWidth() {
            return document.documentElement.clientWidth;
        }
        
        articleWidth() {
            return document.getElementById("article").getBoundingClientRect().width;
        }
        
        _shouldUsePaperAppearance() {
            return this.articleWidth() + 140 < this.documentElementWidth();
        }
        
        makeWideElementsScrollable() {
            let e = document.querySelectorAll("table, pre");
            for (let t of e)
                if (!t.classList.contains("float") && !t.parentElement.classList.contains("scrollable")) {
                    let e = document.createElement("div");
                    t.parentElement.insertBefore(e, t),
                    t.remove(),
                    e.appendChild(t),
                    e.classList.add("scrollable")
                }
        }
        
        canLayOutContentMaintainingAspectRatioBeyondTextColumn() {
            return window.innerHeight >= 695 || window.innerWidth / window.innerHeight <= 1.25
        }
        
        _layOutImagesAndVideoElementsBeyondTextColumn() {
            let e = this.canLayOutContentMaintainingAspectRatioBeyondTextColumn(),
                t = document.getElementById("article");
            if (typeof t !== "undefined") {
                let r = t.querySelectorAll("img, video");
                for (let n of r)
                    this.setImageOrVideoShouldLayOutBeyondTextColumnIfAppropriate(n, e)
            }
        }
        
        leadingMarginAndPaddingAppliedToElementFromAncestors(e) {
            let t = 0,
                n = e.parentElement;
            for (; n && !n.classList.contains("page");) {
                let e = getComputedStyle(n);
                t += parseFloat(e["-webkit-padding-start"]) + parseFloat(e["-webkit-margin-start"]),
                n = n.parentElement
            }
            return t
        }
        
        stopExtendingElementBeyondTextColumn(e) {
            e.classList.remove("extendsBeyondTextColumn"),
            e.style && (e.style.removeProperty("width"), e.style.removeProperty("-webkit-margin-start"))
        }
        
        extendElementBeyondTextColumn(e, t, n) {
            e.classList.add("extendsBeyondTextColumn"),
            e.style && (e.style.setProperty("width", t + "px"), e.style.setProperty("-webkit-margin-start", (n - t) / 2 - this.leadingMarginAndPaddingAppliedToElementFromAncestors(e) + "px"))
        }
        
        setImageOrVideoShouldLayOutBeyondTextColumnIfAppropriate(e, t) {
            if (t && !e.closest("blockquote, table, .float")) {
                let t,
                    n = this._textColumnWidthInPoints(),
                    i = parseFloat(e.getAttribute("width"));
                t = isNaN(i) ? e.naturalWidth : i, this.articleWidth();
                let o = Math.min(t, Math.min(this.MaximumWidthOfImageOrVideoExtendingBeyondTextContainer, this._widthAvailableForLayout()));
                if (o > n) {
                    return void this.extendElementBeyondTextColumn(e, o, n)
                } else if (o > 0) {
                    this.stopExtendingElementBeyondTextColumn(e);
                }
            } else {
                this.stopExtendingElementBeyondTextColumn(e);
            }
        }
        
        usesPaperAppearance() {
            return document.documentElement.classList.contains("paper");
        }
        
        _widthAvailableForLayout() {
            return this.usesPaperAppearance() ? this.articleWidth() : this.documentElementWidth();
        }
        
        _textColumnWidthInPoints() {
            return parseFloat(getComputedStyle(document.querySelector("#article .page")).width);
        }
        
        _layOutElementsContainingTextBeyondTextColumn() {
            const e = {
                    PRE: !0,
                    TABLE: !1
                },
                t = 22;
            let n = document.querySelectorAll(".scrollable pre, .scrollable table");
            for (let i of n) {
                let n = i.parentElement;
                for (let e = n; e; e = e.parentElement)
                    "BLOCKQUOTE" === e.tagName && e.classList.add("simple");
                this.stopExtendingElementBeyondTextColumn(n);
                let o = i.scrollWidth,
                    a = this._textColumnWidthInPoints();
                if (!(o <= a)) {
                    let r = getComputedStyle(document.querySelector(".page")),
                        l = 0;
                    if (e[i.tagName]) {
                        let e = parseFloat(r["-webkit-padding-start"]) + parseFloat(r["-webkit-margin-start"]);
                        l = Math.min(e, t)
                    }
                    let s = Math.min(o, this._widthAvailableForLayout() - 2 * l);
                    this.extendElementBeyondTextColumn(n, s, a);
                }
            }
        }
        
        makeWideElementsScrollable() {
            let e = document.querySelectorAll("table, pre");
            for (let t of e)
                if (!t.classList.contains("float") && !t.parentElement.classList.contains("scrollable")) {
                    let e = document.createElement("div");
                    t.parentElement.insertBefore(e, t),
                    t.remove(),
                    e.appendChild(t),
                    e.classList.add("scrollable")
                }
        }
        
        layOutContent() {
            if ((document.querySelector("#article .page")) && (this._shouldUsePaperAppearance())) {
                document.documentElement.classList.add("paper");
            } else {
                document.documentElement.classList.remove("paper");
                this.makeWideElementsScrollable();
            }
            this._layOutImagesAndVideoElementsBeyondTextColumn();
            this._layOutElementsContainingTextBeyondTextColumn();
            this.makeWideElementsScrollable();
        }
        
        scrollToFirstInstanceOfText(query) {
            if (query.trim() == '') {
                return;
            }
            var paras = document.body.querySelectorAll('p');
            for (let i=0; i<paras.length; i++) {
                if (paras[i].innerText.includes(query) == true) {
                    var rect = paras[i].getBoundingClientRect();
                    var scrollToOptions = {left: 0, top: rect.y, behavior: 'auto'};
                    window.scrollBy(scrollToOptions);
                    break;
                }
            }
        }
        
        setCurrentTextSizeIndex(e) {
            this._textSizeIndex = e;
            this._rebuildDynamicStyleSheet();
        }
        
        fontSettings(e) {
            function t(e) {
                return e.replace(/\W/g, "").toLowerCase()
            }
            const n = [15, 16, 17, 18, 19, 20, 21, 23, 26, 28, 37, 46],
                i = ["25px", "26px", "27px", "28px", "29px", "30px", "31px", "33px", "37px", "39px", "51px", "62px"],
                o = {
                    System: {
                        lineHeights: ["25px", "26px", "27px", "29px", "30px", "31px", "32px", "33px", "38px", "39px", "51px", "62px"],
                        usesSystemFont: !0
                    },
                    Charter: {
                        lineHeights: ["25px", "26px", "27px", "28px", "29px", "30px", "32px", "34px", "38px", "39px", "51px", "62px"]
                    },
                    Georgia: {
                        lineHeights: ["25px", "26px", "27px", "28px", "29px", "30px", "32px", "34px", "38px", "41px", "51px", "62px"]
                    },
                    "Iowan Old Style": {
                        lineHeights: ["25px", "26px", "27px", "28px", "29px", "30px", "32px", "34px", "38px", "39px", "51px", "62px"]
                    },
                    Palatino: {
                        lineHeights: ["25px", "26px", "27px", "28px", "29px", "30px", "31px", "34px", "37px", "40px", "51px", "62px"]
                    },
                    Seravek: {
                        lineHeights: ["25px", "26px", "27px", "28px", "28px", "30px", "31px", "34px", "37px", "39px", "51px", "62px"]
                    },
                    "Hiragino Sans W3": {
                        lineHeights: ["1.85em", "1.78em", "1.74em", "1.71em", "1.72em", "1.73em", "1.75em", "1.76em", "1.78em", "1.9em", "1.92em", "2em"]
                    },
                    "Hiragino Kaku Gothic ProN": {
                        lineHeights: ["1.85em", "1.78em", "1.74em", "1.7em", "1.69em", "1.68em", "1.69em", "1.7em", "1.74em", "1.85em", "1.9em", "2em"]
                    },
                    "Hiragino Mincho ProN": {
                        lineHeights: ["1.75em", "1.72em", "1.69em", "1.66em", "1.64em", "1.56em", "1.53em", "1.56em", "1.6em", "1.65em", "1.69em", "1.72em"]
                    },
                    "Hiragino Maru Gothic ProN": {
                        lineHeights: ["1.85em", "1.78em", "1.74em", "1.7em", "1.69em", "1.68em", "1.69em", "1.7em", "1.74em", "1.85em", "1.9em", "2em"]
                    },
                    "PingFang SC": {
                        lineHeights: ["1.85em", "1.78em", "1.74em", "1.7em", "1.69em", "1.68em", "1.69em", "1.7em", "1.74em", "1.85em", "1.9em", "2em"],
                        usesSystemFont: !0
                    },
                    "Heiti SC": {
                        lineHeights: ["1.85em", "1.78em", "1.74em", "1.7em", "1.7em", "1.71em", "1.72em", "1.75em", "1.8em", "1.9em", "1.95em", "2em"]
                    },
                    "Songti SC": {
                        lineHeights: ["1.8em", "1.78em", "1.74em", "1.72em", "1.71em", "1.72em", "1.73em", "1.75em", "1.8em", "1.9em", "1.95em", "1.96em"]
                    },
                    "Kaiti SC": {
                        lineHeights: ["1.75em", "1.72em", "1.69em", "1.66em", "1.64em", "1.56em", "1.53em", "1.56em", "1.6em", "1.65em", "1.69em", "1.72em"]
                    },
                    "Yuanti SC": {
                        lineHeights: ["1.95em", "1.93em", "1.9em", "1.87em", "1.85em", "1.8em", "1.83em", "1.85em", "1.88em", "1.9em", "1.91em", "1.92em"]
                    },
                    "Libian SC": {
                        fontSizes: [27, 28, 29, 30, 31, 32, 33, 35, 37, 40, 42, 46],
                        lineHeights: ["1.65em", "1.63em", "1.62em", "1.61em", "1.6em", "1.6em", "1.61em", "1.62em", "1.63em", "1.64em", "1.64em", "1.65em"]
                    },
                    "Weibei SC": {
                        fontSizes: [21, 22, 23, 24, 25, 26, 27, 29, 32, 34, 39, 43],
                        lineHeights: ["1.65em", "1.63em", "1.62em", "1.61em", "1.6em", "1.6em", "1.61em", "1.62em", "1.63em", "1.64em", "1.64em", "1.65em"]
                    },
                    "Yuppy SC": {
                        lineHeights: ["1.75em", "1.73em", "1.7em", "1.67em", "1.65em", "1.6em", "1.63em", "1.65em", "1.68em", "1.7em", "1.71em", "1.72em"]
                    },
                    "PingFang TC": {
                        lineHeights: ["1.85em", "1.78em", "1.75em", "1.72em", "1.7em", "1.7em", "1.7em", "1.72em", "1.75em", "1.82em", "1.85em", "1.9em"],
                        usesSystemFont: !0
                    },
                    "Heiti TC": {
                        lineHeights: ["1.85em", "1.78em", "1.75em", "1.72em", "1.71em", "1.71em", "1.72em", "1.75em", "1.78em", "1.82em", "1.86em", "1.9em"]
                    },
                    "Songti TC": {
                        lineHeights: ["1.8em", "1.78em", "1.74em", "1.73em", "1.72em", "1.72em", "1.73em", "1.75em", "1.8em", "1.9em", "1.95em", "1.96em"]
                    },
                    "Kaiti TC": {
                        fontSizes: [20, 21, 22, 23, 24, 25, 26, 28, 31, 33, 38, 43],
                        lineHeights: ["1.63em", "1.62em", "1.62em", "1.6em", "1.56em", "1.53em", "1.5em", "1.53em", "1.56em", "1.6em", "1.62em", "1.63em"]
                    },
                    "Yuanti TC": {
                        lineHeights: ["1.95em", "1.93em", "1.9em", "1.87em", "1.85em", "1.8em", "1.83em", "1.85em", "1.88em", "1.9em", "1.91em", "1.92em"]
                    },
                    "Libian TC": {
                        fontSizes: [27, 28, 29, 30, 31, 32, 33, 35, 37, 40, 42, 46],
                        lineHeights: ["1.65em", "1.63em", "1.62em", "1.61em", "1.6em", "1.6em", "1.61em", "1.62em", "1.63em", "1.64em", "1.64em", "1.65em"]
                    },
                    "Weibei TC": {
                        fontSizes: [21, 22, 23, 24, 25, 26, 27, 29, 32, 34, 39, 43],
                        lineHeights: ["1.65em", "1.63em", "1.62em", "1.61em", "1.6em", "1.6em", "1.61em", "1.62em", "1.63em", "1.64em", "1.64em", "1.65em"]
                    },
                    "Yuppy TC": {
                        lineHeights: ["1.75em", "1.73em", "1.7em", "1.67em", "1.65em", "1.6em", "1.63em", "1.65em", "1.68em", "1.7em", "1.71em", "1.72em"]
                    },
                    "Noto Nastaliq Urdu": {
                        lineHeights: ["2.6em", "2.7em", "2.8em", "2.8em", "2.8em", "2.8em", "2.8em", "2.6em", "2.5em", "2.5em", "2.5em", "2.6em"]
                    }
                };
            let a = o[e];
            return a || (a = {}), a.cssClassName = t(e), a.usesSystemFont || (a.fontFamilyName = e), a.fontSizes || (a.fontSizes = n), a.lineHeights || (a.lineHeights = i), a;
        }
        
        _currentFontSettings() {
            return this.fontSettings(this._fontFamilyName)
        }
        
        _rebuildDynamicStyleSheet() {
            let e = document.getElementById("dynamic-article-content").sheet;
            for (; e.cssRules.length;)
                e.removeRule(0);
            let t = this._currentFontSettings().fontSizes[this._textSizeIndex] + "px",
                n = this._currentFontSettings().lineHeights[this._textSizeIndex];
            e.insertRule("#article { font-size: " + t + "; line-height: " + n + "; }")
        }
        
        preparePrintingMailingFrame() {
            let e = this.printingMailingFrameElementId(),
                t = document.getElementById(e);
            t && document.body.removeChild(t),
            t = this.sanitizedFullArticleFrame(),
            t.id = e;
        }
        
        sanitizedFullArticleFrame() {
            let e = document.createElement("iframe");
            e.style.display = "none",
            e.style.position = "absolute",
            document.body.appendChild(e);
            let t = e.contentDocument,
                n = document.createElement("base");
            n.href = this.originalURL,
            t.head.appendChild(n);
            let i = document.createElement("div");
            i.className = "original-url";
            let o = document.createElement("a");
            o.href = this.originalURL,
            o.textContent = this.originalURL,
            i.appendChild(document.createElement("br")),
            i.appendChild(o),
            i.appendChild(document.createElement("br")),
            i.appendChild(document.createElement("br")),
            t.body.appendChild(i),
            t.body.appendChild(this.sanitizedFullArticle()),
            t.head.appendChild(document.getElementById("print").cloneNode(!0));
            let a = t.createElement("title");
            return a.innerText = document.title, t.head.appendChild(a), e
        }
        
        sanitizedFullArticle() {
            let e = document.getElementById("article").cloneNode(!0);
            e.removeAttribute("tabindex");
            e.style.removeProperty("max-width");
            const t = e.querySelectorAll(".title");
            for (let e = 1, n = t.length; e < n; ++e)
                t[e].remove();
            for (let t of e.querySelectorAll(".page-number, #incoming-page-placeholder"))
                t.remove();
            let j = e.querySelectorAll("img, video, audio, source");
            for (let d of j) {
                d.setAttribute("src", d.src)
                d.removeAttribute("loading")
            }
            for (let t of e.querySelectorAll(".extendsBeyondTextColumn"))
                this.stopExtendingElementBeyondTextColumn(t);
            for (let t of e.querySelectorAll(".delimeter"))
                t.innerText = "\u2022";
            for (let t of e.getElementsByTagName("*"))
                t.style.removeProperty("color");
            e.classList.add(this._currentFontSettings().cssClassName);
            let n = this._currentFontSettings().fontFamilyName;
            n && (e.style.fontFamily = n),
            this._currentFontSettings().usesSystemFont && e.classList.add("system"),
            e.classList.add("exported");
            let a = document.getElementById("article-content").sheet.cssRules,
                r = a.length;
            for (let t = 0; t < r; ++t) {
                let n = a[t].selectorText,
                    i = a[t].style;
                if (i) {
                    let t = i.cssText;
                    e.matches(n) && e.style && (e.style.cssText += t);
                    for (let i of e.querySelectorAll(n))
                        i.style && (i.style.cssText += t)
                }
            }
            return e
        }
        
        printingMailingFrameElementId() {
            return "printing-mailing-frame"
        }
};

    var reader = new OrionReader();
};

%DESKTOP-SCROLLER-SCRIPT%

reader.parse();

//EDIT: since we inject this using WebKitUserContentManager at document end, DOMContentLoaded has likely already fired, check against that
function init() {
    
    // <This fixes overscroll background color on initial state>
    const bodyClass = document.body.className;
    document.body.className = "";
    
    setTimeout(function() {
        document.body.className = bodyClass;
    }, 50);
    // </This fixes overscroll background color on initial state>
    
    setTimeout(function() {
        reader.fixImages();
        reader.layOutContent();
        reader.scrollToFirstInstanceOfText("%READER-TEXT-INITIAL-OFFSET%");
    }, 200);
}
if(document.readyState) {
    init();
}
else {
    window.addEventListener('DOMContentLoaded', init);
}
//ENDEDIT
                        
window.addEventListener('resize', function(event) {
    
    reader.layOutContent();
    
}, true);

document.documentElement.addEventListener('click', function(e) {
    if (document.getElementById("article").contains(e.target) == false) {
        %READER-BODY-CLICK-EVENT%
    }
});

//EDIT: this is added to notify us of a successful pageload in the reader webview
window.webkit.messageHandlers.readerInitialised.postMessage(window.__epoch);
//ENDEDIT
