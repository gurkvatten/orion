if (typeof AppleAnimator !== "function") {

	var smoothScrollingAnimator,
		smoothScrollingAnimation,
		scrollEventIsSmoothScroll = !1;

	var AnimationTerminationCondition = {
		Interrupted: 0,
		CompletedSuccessfully: 1
	};

	var ContentAwareNavigationMarker = "reader-content-aware-navigation-marker",
		ContentAwareNavigationAnimationDuration = 200,
		ContentAwareNavigationElementOffset = 8,
		ContentAwareNavigationDirection = {
			Up: 0,
			Down: 1
		};

	function articleHeight() {
		return document.getElementById("article").offsetHeight + 2 * parseFloat(getComputedStyle(document.getElementById("article")).marginTop)
	}
	function smoothScroll(e, t, n, i) {
		function o(t, n) {
			scrollEventIsSmoothScroll = !0,
			e.scrollTop = n,
			setTimeout(function() {
				scrollEventIsSmoothScroll = !1
			}, 0)
		}
		const a = 1e3 / 60;
		let r = e.scrollTop,
			l = r + t,
			s = 0,
			d = articleHeight() - window.innerHeight;
		if (l < s && (l = s), l > d && (l = d), r != l) {
			let e = Math.abs(l - r);
			if (e < Math.abs(t) && (n = n * e / Math.abs(t)), smoothScrollingAnimator) {
				let e = smoothScrollingAnimator.animations[0],
					t = e.progress,
					s = t > .5 ? 1 - t : t,
					d = n / (1 - s),
					c = -s * d,
					m = Math.sin(Math.PI / 2 * s),
					u = m * m,
					g = (r - l * u) / (1 - u);
				return abortSmoothScroll(), smoothScrollingAnimator = new AppleAnimator(d, a, i), smoothScrollingAnimation = new AppleAnimation(g, l, o), smoothScrollingAnimator.addAnimation(smoothScrollingAnimation), void smoothScrollingAnimator.start(c)
			}
			smoothScrollingAnimator = new AppleAnimator(n, a, i),
			smoothScrollingAnimation = new AppleAnimation(r, l, o),
			smoothScrollingAnimator.addAnimation(smoothScrollingAnimation),
			smoothScrollingAnimator.start()
		}
	}
	function abortSmoothScroll() {
		smoothScrollingAnimator.stop(AnimationTerminationCondition.Interrupted),
		smoothScrollingAnimator = null,
		smoothScrollingAnimation = null
	}
	function articleScrolled() {
		if ((!scrollEventIsSmoothScroll) && (smoothScrollingAnimator)) {
			abortSmoothScroll();
		}
	}
	function traverseReaderContent(e, t) {
		if (e) {
			let n = e.offsetTop,
				i = document.createTreeWalker(document.getElementById("article"), NodeFilter.SHOW_ELEMENT, {
					acceptNode: function(e) {
						let t = e.classList;
						return t.contains("page-number") || t.contains("float") || t.contains("page") || t.contains("scrollable") || "HR" === e.tagName || 0 === e.offsetHeight || "inline" === getComputedStyle(e).display || n === e.offsetTop ? NodeFilter.FILTER_SKIP : NodeFilter.FILTER_ACCEPT
					}
				});
			return i.currentNode = e, i[t]()
		}
	}
	function nextReaderContentElement(e) {
		return traverseReaderContent(e, "nextNode")
	}
	function previousReaderContentElement(e) {
		return traverseReaderContent(e, "previousNode")
	}
	function articleTitleElement() {
		return document.querySelector("#article .page .title")
	}

	function specialKeyDown(e) {
		switch (e.keyCode) {
		case 74: // j
			ContentAwareScrollerJS.scroll(ContentAwareNavigationDirection.Down);
			e.preventDefault();
			break;
				
		case 75: // k
			ContentAwareScrollerJS.scroll(ContentAwareNavigationDirection.Up);
			e.preventDefault();
			break;
				
		case 27: // esc
			%READER-BODY-CLICK-EVENT%
			e.preventDefault();
			break;
		}
	}

	AppleAnimator = function(e, t, n) {
		this.startTime = 0,
		this.duration = e,
		this.interval = t,
		this.animations = [],
		this.animationFinishedCallback = n,
		this.currentFrameRequestID = null,
		this._firstTime = !0;
		let i = this;
		this.animate = function() {
			function e(e, t, n) {
				return e < t ? t : e > n ? n : e
			}
			let t,
				n,
				o,
				a = (new Date).getTime(),
				r = i.duration;
			t = e(a - i.startTime, 0, r),
			a = t / r,
			n = .5 - .5 * Math.cos(Math.PI * a),
			o = t >= r;
			let l = i.animations,
				s = l.length,
				d = i._firstTime;
			for (let e = 0; e < s; ++e)
				l[e].doFrame(i, n, d, o, a);
			if (o)
				return void i.stop(AnimationTerminationCondition.CompletedSuccessfully);
			i._firstTime = !1,
			this.currentFrameRequestID = requestAnimationFrame(i.animate)
		}
	},
	AppleAnimator.prototype = {
		start: function(e) {
			let t = (new Date).getTime(),
				n = this.interval;
			this.startTime = t - n,
			e && (this.startTime += e),
			this.currentFrameRequestID = requestAnimationFrame(this.animate)
		},
		stop: function(e) {
			this.animationFinishedCallback && this.animationFinishedCallback(e),
			this.currentFrameRequestID && cancelAnimationFrame(this.currentFrameRequestID)
		},
		addAnimation: function(e) {
			this.animations[this.animations.length] = e
		}
	},
	AppleAnimation = function(e, t, n) {
		this.from = e,
		this.to = t,
		this.callback = n,
		this.now = e,
		this.ease = 0,
		this.progress = 0
	},
	AppleAnimation.prototype = {
		doFrame: function(e, t, n, i, o) {
			let a;
			a = i ? this.to : this.from + (this.to - this.from) * t,
			this.now = a,
			this.ease = t,
			this.progress = o,
			this.callback(e, a, n, i)
		}
	};

	ContentAwareScroller = function() {
		this._numberOfContentAwareScrollAnimationsInProgress = 0
	},
	ContentAwareScroller.prototype = {
		_contentElementAtTopOfViewport: function() {
			let e = articleTitleElement();
			do {
				if (!(e.getBoundingClientRect().top < ContentAwareNavigationElementOffset))
					return e
			} while (e = nextReaderContentElement(e));
			return null
		},
		_clearTargetOfContentAwareScrolling: function() {
			let e = document.getElementById(ContentAwareNavigationMarker);
			e && e.removeAttribute("id")
		},
		_contentAwareScrollFinished: function(e) {
			e === AnimationTerminationCondition.CompletedSuccessfully && (--this._numberOfContentAwareScrollAnimationsInProgress || (smoothScrollingAnimator = null, smoothScrollingAnimation = null, this._clearTargetOfContentAwareScrolling()))
		},
		scroll: function(e) {
			let t,
				n,
				i = document.getElementById(ContentAwareNavigationMarker),
				o = i || this._contentElementAtTopOfViewport();
			if (e === ContentAwareNavigationDirection.Down) {
				let e = Math.abs(o.getBoundingClientRect().top - ContentAwareNavigationElementOffset) < 1;
				t = i || e ? nextReaderContentElement(o) : o
			} else if (e === ContentAwareNavigationDirection.Up)
				if (o === articleTitleElement()) {
					if (0 === document.scrollingElement.scrollTop)
						return;
					n = -1 * document.scrollingElement.scrollTop
				} else
					t = previousReaderContentElement(o);
			t && (n = t.getBoundingClientRect().top - ContentAwareNavigationElementOffset),
			++this._numberOfContentAwareScrollAnimationsInProgress,
			smoothScroll(document.scrollingElement, n, ContentAwareNavigationAnimationDuration, this._contentAwareScrollFinished.bind(this)),
			this._clearTargetOfContentAwareScrolling(),
			t && (t.id = ContentAwareNavigationMarker)
		}
	}

	var ContentAwareScrollerJS = new ContentAwareScroller;
}

window.addEventListener('keydown', specialKeyDown, true);
window.addEventListener('scroll', articleScrolled, {
	capture: !1,
	passive: !0
});
