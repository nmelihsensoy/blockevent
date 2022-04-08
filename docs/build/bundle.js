
(function(l, r) { if (!l || l.getElementById('livereloadscript')) return; r = l.createElement('script'); r.async = 1; r.src = '//' + (self.location.host || 'localhost').split(':')[0] + ':35729/livereload.js?snipver=1'; r.id = 'livereloadscript'; l.getElementsByTagName('head')[0].appendChild(r) })(self.document);
var app = (function () {
    'use strict';

    function noop() { }
    function add_location(element, file, line, column, char) {
        element.__svelte_meta = {
            loc: { file, line, column, char }
        };
    }
    function run(fn) {
        return fn();
    }
    function blank_object() {
        return Object.create(null);
    }
    function run_all(fns) {
        fns.forEach(run);
    }
    function is_function(thing) {
        return typeof thing === 'function';
    }
    function safe_not_equal(a, b) {
        return a != a ? b == b : a !== b || ((a && typeof a === 'object') || typeof a === 'function');
    }
    function is_empty(obj) {
        return Object.keys(obj).length === 0;
    }
    function append(target, node) {
        target.appendChild(node);
    }
    function insert(target, node, anchor) {
        target.insertBefore(node, anchor || null);
    }
    function detach(node) {
        node.parentNode.removeChild(node);
    }
    function element(name) {
        return document.createElement(name);
    }
    function text(data) {
        return document.createTextNode(data);
    }
    function space() {
        return text(' ');
    }
    function listen(node, event, handler, options) {
        node.addEventListener(event, handler, options);
        return () => node.removeEventListener(event, handler, options);
    }
    function attr(node, attribute, value) {
        if (value == null)
            node.removeAttribute(attribute);
        else if (node.getAttribute(attribute) !== value)
            node.setAttribute(attribute, value);
    }
    function children(element) {
        return Array.from(element.childNodes);
    }
    function set_style(node, key, value, important) {
        if (value === null) {
            node.style.removeProperty(key);
        }
        else {
            node.style.setProperty(key, value, important ? 'important' : '');
        }
    }
    function custom_event(type, detail, bubbles = false) {
        const e = document.createEvent('CustomEvent');
        e.initCustomEvent(type, bubbles, false, detail);
        return e;
    }

    let current_component;
    function set_current_component(component) {
        current_component = component;
    }
    function get_current_component() {
        if (!current_component)
            throw new Error('Function called outside component initialization');
        return current_component;
    }
    function onMount(fn) {
        get_current_component().$$.on_mount.push(fn);
    }

    const dirty_components = [];
    const binding_callbacks = [];
    const render_callbacks = [];
    const flush_callbacks = [];
    const resolved_promise = Promise.resolve();
    let update_scheduled = false;
    function schedule_update() {
        if (!update_scheduled) {
            update_scheduled = true;
            resolved_promise.then(flush);
        }
    }
    function add_render_callback(fn) {
        render_callbacks.push(fn);
    }
    function add_flush_callback(fn) {
        flush_callbacks.push(fn);
    }
    // flush() calls callbacks in this order:
    // 1. All beforeUpdate callbacks, in order: parents before children
    // 2. All bind:this callbacks, in reverse order: children before parents.
    // 3. All afterUpdate callbacks, in order: parents before children. EXCEPT
    //    for afterUpdates called during the initial onMount, which are called in
    //    reverse order: children before parents.
    // Since callbacks might update component values, which could trigger another
    // call to flush(), the following steps guard against this:
    // 1. During beforeUpdate, any updated components will be added to the
    //    dirty_components array and will cause a reentrant call to flush(). Because
    //    the flush index is kept outside the function, the reentrant call will pick
    //    up where the earlier call left off and go through all dirty components. The
    //    current_component value is saved and restored so that the reentrant call will
    //    not interfere with the "parent" flush() call.
    // 2. bind:this callbacks cannot trigger new flush() calls.
    // 3. During afterUpdate, any updated components will NOT have their afterUpdate
    //    callback called a second time; the seen_callbacks set, outside the flush()
    //    function, guarantees this behavior.
    const seen_callbacks = new Set();
    let flushidx = 0; // Do *not* move this inside the flush() function
    function flush() {
        const saved_component = current_component;
        do {
            // first, call beforeUpdate functions
            // and update components
            while (flushidx < dirty_components.length) {
                const component = dirty_components[flushidx];
                flushidx++;
                set_current_component(component);
                update(component.$$);
            }
            set_current_component(null);
            dirty_components.length = 0;
            flushidx = 0;
            while (binding_callbacks.length)
                binding_callbacks.pop()();
            // then, once components are updated, call
            // afterUpdate functions. This may cause
            // subsequent updates...
            for (let i = 0; i < render_callbacks.length; i += 1) {
                const callback = render_callbacks[i];
                if (!seen_callbacks.has(callback)) {
                    // ...so guard against infinite loops
                    seen_callbacks.add(callback);
                    callback();
                }
            }
            render_callbacks.length = 0;
        } while (dirty_components.length);
        while (flush_callbacks.length) {
            flush_callbacks.pop()();
        }
        update_scheduled = false;
        seen_callbacks.clear();
        set_current_component(saved_component);
    }
    function update($$) {
        if ($$.fragment !== null) {
            $$.update();
            run_all($$.before_update);
            const dirty = $$.dirty;
            $$.dirty = [-1];
            $$.fragment && $$.fragment.p($$.ctx, dirty);
            $$.after_update.forEach(add_render_callback);
        }
    }
    const outroing = new Set();
    let outros;
    function transition_in(block, local) {
        if (block && block.i) {
            outroing.delete(block);
            block.i(local);
        }
    }
    function transition_out(block, local, detach, callback) {
        if (block && block.o) {
            if (outroing.has(block))
                return;
            outroing.add(block);
            outros.c.push(() => {
                outroing.delete(block);
                if (callback) {
                    if (detach)
                        block.d(1);
                    callback();
                }
            });
            block.o(local);
        }
    }

    const globals = (typeof window !== 'undefined'
        ? window
        : typeof globalThis !== 'undefined'
            ? globalThis
            : global);

    function bind(component, name, callback) {
        const index = component.$$.props[name];
        if (index !== undefined) {
            component.$$.bound[index] = callback;
            callback(component.$$.ctx[index]);
        }
    }
    function create_component(block) {
        block && block.c();
    }
    function mount_component(component, target, anchor, customElement) {
        const { fragment, on_mount, on_destroy, after_update } = component.$$;
        fragment && fragment.m(target, anchor);
        if (!customElement) {
            // onMount happens before the initial afterUpdate
            add_render_callback(() => {
                const new_on_destroy = on_mount.map(run).filter(is_function);
                if (on_destroy) {
                    on_destroy.push(...new_on_destroy);
                }
                else {
                    // Edge case - component was destroyed immediately,
                    // most likely as a result of a binding initialising
                    run_all(new_on_destroy);
                }
                component.$$.on_mount = [];
            });
        }
        after_update.forEach(add_render_callback);
    }
    function destroy_component(component, detaching) {
        const $$ = component.$$;
        if ($$.fragment !== null) {
            run_all($$.on_destroy);
            $$.fragment && $$.fragment.d(detaching);
            // TODO null out other refs, including component.$$ (but need to
            // preserve final state?)
            $$.on_destroy = $$.fragment = null;
            $$.ctx = [];
        }
    }
    function make_dirty(component, i) {
        if (component.$$.dirty[0] === -1) {
            dirty_components.push(component);
            schedule_update();
            component.$$.dirty.fill(0);
        }
        component.$$.dirty[(i / 31) | 0] |= (1 << (i % 31));
    }
    function init(component, options, instance, create_fragment, not_equal, props, append_styles, dirty = [-1]) {
        const parent_component = current_component;
        set_current_component(component);
        const $$ = component.$$ = {
            fragment: null,
            ctx: null,
            // state
            props,
            update: noop,
            not_equal,
            bound: blank_object(),
            // lifecycle
            on_mount: [],
            on_destroy: [],
            on_disconnect: [],
            before_update: [],
            after_update: [],
            context: new Map(options.context || (parent_component ? parent_component.$$.context : [])),
            // everything else
            callbacks: blank_object(),
            dirty,
            skip_bound: false,
            root: options.target || parent_component.$$.root
        };
        append_styles && append_styles($$.root);
        let ready = false;
        $$.ctx = instance
            ? instance(component, options.props || {}, (i, ret, ...rest) => {
                const value = rest.length ? rest[0] : ret;
                if ($$.ctx && not_equal($$.ctx[i], $$.ctx[i] = value)) {
                    if (!$$.skip_bound && $$.bound[i])
                        $$.bound[i](value);
                    if (ready)
                        make_dirty(component, i);
                }
                return ret;
            })
            : [];
        $$.update();
        ready = true;
        run_all($$.before_update);
        // `false` as a special case of no DOM component
        $$.fragment = create_fragment ? create_fragment($$.ctx) : false;
        if (options.target) {
            if (options.hydrate) {
                const nodes = children(options.target);
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                $$.fragment && $$.fragment.l(nodes);
                nodes.forEach(detach);
            }
            else {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                $$.fragment && $$.fragment.c();
            }
            if (options.intro)
                transition_in(component.$$.fragment);
            mount_component(component, options.target, options.anchor, options.customElement);
            flush();
        }
        set_current_component(parent_component);
    }
    /**
     * Base class for Svelte components. Used when dev=false.
     */
    class SvelteComponent {
        $destroy() {
            destroy_component(this, 1);
            this.$destroy = noop;
        }
        $on(type, callback) {
            const callbacks = (this.$$.callbacks[type] || (this.$$.callbacks[type] = []));
            callbacks.push(callback);
            return () => {
                const index = callbacks.indexOf(callback);
                if (index !== -1)
                    callbacks.splice(index, 1);
            };
        }
        $set($$props) {
            if (this.$$set && !is_empty($$props)) {
                this.$$.skip_bound = true;
                this.$$set($$props);
                this.$$.skip_bound = false;
            }
        }
    }

    function dispatch_dev(type, detail) {
        document.dispatchEvent(custom_event(type, Object.assign({ version: '3.46.4' }, detail), true));
    }
    function append_dev(target, node) {
        dispatch_dev('SvelteDOMInsert', { target, node });
        append(target, node);
    }
    function insert_dev(target, node, anchor) {
        dispatch_dev('SvelteDOMInsert', { target, node, anchor });
        insert(target, node, anchor);
    }
    function detach_dev(node) {
        dispatch_dev('SvelteDOMRemove', { node });
        detach(node);
    }
    function listen_dev(node, event, handler, options, has_prevent_default, has_stop_propagation) {
        const modifiers = options === true ? ['capture'] : options ? Array.from(Object.keys(options)) : [];
        if (has_prevent_default)
            modifiers.push('preventDefault');
        if (has_stop_propagation)
            modifiers.push('stopPropagation');
        dispatch_dev('SvelteDOMAddEventListener', { node, event, handler, modifiers });
        const dispose = listen(node, event, handler, options);
        return () => {
            dispatch_dev('SvelteDOMRemoveEventListener', { node, event, handler, modifiers });
            dispose();
        };
    }
    function attr_dev(node, attribute, value) {
        attr(node, attribute, value);
        if (value == null)
            dispatch_dev('SvelteDOMRemoveAttribute', { node, attribute });
        else
            dispatch_dev('SvelteDOMSetAttribute', { node, attribute, value });
    }
    function set_data_dev(text, data) {
        data = '' + data;
        if (text.wholeText === data)
            return;
        dispatch_dev('SvelteDOMSetData', { node: text, data });
        text.data = data;
    }
    function validate_slots(name, slot, keys) {
        for (const slot_key of Object.keys(slot)) {
            if (!~keys.indexOf(slot_key)) {
                console.warn(`<${name}> received an unexpected slot "${slot_key}".`);
            }
        }
    }
    /**
     * Base class for Svelte components with some minor dev-enhancements. Used when dev=true.
     */
    class SvelteComponentDev extends SvelteComponent {
        constructor(options) {
            if (!options || (!options.target && !options.$$inline)) {
                throw new Error("'target' is a required option");
            }
            super();
        }
        $destroy() {
            super.$destroy();
            this.$destroy = () => {
                console.warn('Component was already destroyed'); // eslint-disable-line no-console
            };
        }
        $capture_state() { }
        $inject_state() { }
    }

    /* src/Screen.svelte generated by Svelte v3.46.4 */
    const file$1 = "src/Screen.svelte";

    function create_fragment$1(ctx) {
    	let div;
    	let canvas_1;
    	let mounted;
    	let dispose;
    	add_render_callback(/*onwindowresize*/ ctx[16]);

    	const block = {
    		c: function create() {
    			div = element("div");
    			canvas_1 = element("canvas");
    			set_style(canvas_1, "width", /*scale*/ ctx[1] + "%");
    			set_style(canvas_1, "height", /*scale*/ ctx[1]);
    			attr_dev(canvas_1, "width", 32);
    			attr_dev(canvas_1, "height", 32);
    			attr_dev(canvas_1, "class", "svelte-1g5oq65");
    			add_location(canvas_1, file$1, 121, 4, 3086);
    			attr_dev(div, "class", "screen svelte-1g5oq65");
    			add_location(div, file$1, 119, 0, 3000);
    		},
    		l: function claim(nodes) {
    			throw new Error("options.hydrate only works if the component was compiled with the `hydratable: true` option");
    		},
    		m: function mount(target, anchor) {
    			insert_dev(target, div, anchor);
    			append_dev(div, canvas_1);
    			/*canvas_1_binding*/ ctx[17](canvas_1);

    			if (!mounted) {
    				dispose = [
    					listen_dev(window, "DOMMouseScroll", /*updateScale*/ ctx[3], false, false, false),
    					listen_dev(window, "mousewheel", /*updateScale*/ ctx[3], false, false, false),
    					listen_dev(window, "keydown", /*handleKeydown*/ ctx[4], false, false, false),
    					listen_dev(window, "keyup", /*handleKeyup*/ ctx[5], false, false, false),
    					listen_dev(window, "resize", /*onwindowresize*/ ctx[16]),
    					listen_dev(canvas_1, "mousedown", /*drawDown*/ ctx[6], false, false, false),
    					listen_dev(canvas_1, "mousemove", /*drawMove*/ ctx[7], false, false, false),
    					listen_dev(canvas_1, "mouseup", /*drawUp*/ ctx[8], false, false, false),
    					listen_dev(canvas_1, "mouseout", drawOut, false, false, false)
    				];

    				mounted = true;
    			}
    		},
    		p: function update(ctx, [dirty]) {
    			if (dirty & /*scale*/ 2) {
    				set_style(canvas_1, "width", /*scale*/ ctx[1] + "%");
    			}

    			if (dirty & /*scale*/ 2) {
    				set_style(canvas_1, "height", /*scale*/ ctx[1]);
    			}
    		},
    		i: noop,
    		o: noop,
    		d: function destroy(detaching) {
    			if (detaching) detach_dev(div);
    			/*canvas_1_binding*/ ctx[17](null);
    			mounted = false;
    			run_all(dispose);
    		}
    	};

    	dispatch_dev("SvelteRegisterBlock", {
    		block,
    		id: create_fragment$1.name,
    		type: "component",
    		source: "",
    		ctx
    	});

    	return block;
    }

    function drawOut(e) {
    	
    }

    function instance$1($$self, $$props, $$invalidate) {
    	let innerHeight;
    	let { $$slots: slots = {}, $$scope } = $$props;
    	validate_slots('Screen', slots, []);
    	let canvas;
    	let ctx;
    	let scale = 100;
    	let isControlPressed = false;
    	let drag = false;
    	let imageObj = null;
    	let { srcProp = "./screen1.png" } = $$props;
    	let { rectW } = $$props;
    	let { rectH } = $$props;
    	let { rectStartX } = $$props;
    	let { rectStartY } = $$props;
    	let { screenW } = $$props;
    	let { screenH } = $$props;

    	onMount(() => {
    		ctx = canvas.getContext('2d');
    		ctx.imageSmoothingEnabled = false;
    		loadImg(srcProp);
    	});

    	function loadImg(imgSrc) {
    		var image = new Image();
    		image.src = imgSrc;

    		image.onload = function () {
    			$$invalidate(0, canvas.width = image.width, canvas);
    			$$invalidate(13, screenW = image.width);
    			$$invalidate(0, canvas.height = image.height, canvas);
    			$$invalidate(14, screenH = image.height);
    			ctx.drawImage(image, 0, 0);
    			imageObj = image;

    			if (innerHeight - 150 < image.height) {
    				$$invalidate(1, scale = (innerHeight - 150) * 100 / image.height);
    			} else {
    				$$invalidate(1, scale = 100);
    			}
    		};

    		$$invalidate(9, rectW = 0);
    		$$invalidate(10, rectH = 0);
    		$$invalidate(11, rectStartX = 0);
    		$$invalidate(12, rectStartY = 0);
    	}

    	function updateScale(e) {
    		if (!isControlPressed) {
    			return;
    		}

    		var delta = e.wheelDelta ? e.wheelDelta : -e.detail;

    		if (delta < 0) {
    			if (scale >= 10) {
    				$$invalidate(1, scale -= 5);
    			}
    		} else {
    			if (scale < 100) {
    				$$invalidate(1, scale += 5);
    			}
    		}
    	}

    	function handleKeydown(e) {
    		if (e.keyCode == 16) {
    			e.preventDefault();
    			isControlPressed = true;
    		}
    	}

    	function handleKeyup(e) {
    		if (e.keyCode == 16) {
    			isControlPressed = false;
    		}
    	}

    	function drawDown(e) {
    		if (e.button == 0) {
    			$$invalidate(11, rectStartX = canvas.width * e.offsetX / canvas.getBoundingClientRect().width);
    			$$invalidate(12, rectStartY = canvas.height * e.offsetY / canvas.getBoundingClientRect().height);
    			drag = true;
    		}
    	}

    	function drawMove(e) {
    		if (drag) {
    			ctx.clearRect(0, 0, canvas.width, canvas.height);
    			ctx.drawImage(imageObj, 0, 0);
    			$$invalidate(9, rectW = canvas.width * e.offsetX / canvas.getBoundingClientRect().width - rectStartX);
    			$$invalidate(10, rectH = canvas.height * e.offsetY / canvas.getBoundingClientRect().height - rectStartY);
    			ctx.shadowColor = '#d53';
    			ctx.shadowBlur = 20;
    			ctx.lineJoin = 'bevel';
    			ctx.lineWidth = 15;
    			ctx.strokeStyle = '#38f';
    			ctx.fillStyle = 'rgba(225,225,225,0.5)';
    			ctx.fillRect(rectStartX, rectStartY, rectW, rectH);
    		}
    	}

    	function drawUp(e) {
    		drag = false;
    	}

    	const writable_props = ['srcProp', 'rectW', 'rectH', 'rectStartX', 'rectStartY', 'screenW', 'screenH'];

    	Object.keys($$props).forEach(key => {
    		if (!~writable_props.indexOf(key) && key.slice(0, 2) !== '$$' && key !== 'slot') console.warn(`<Screen> was created with unknown prop '${key}'`);
    	});

    	function onwindowresize() {
    		$$invalidate(2, innerHeight = window.innerHeight);
    	}

    	function canvas_1_binding($$value) {
    		binding_callbacks[$$value ? 'unshift' : 'push'](() => {
    			canvas = $$value;
    			$$invalidate(0, canvas);
    		});
    	}

    	$$self.$$set = $$props => {
    		if ('srcProp' in $$props) $$invalidate(15, srcProp = $$props.srcProp);
    		if ('rectW' in $$props) $$invalidate(9, rectW = $$props.rectW);
    		if ('rectH' in $$props) $$invalidate(10, rectH = $$props.rectH);
    		if ('rectStartX' in $$props) $$invalidate(11, rectStartX = $$props.rectStartX);
    		if ('rectStartY' in $$props) $$invalidate(12, rectStartY = $$props.rectStartY);
    		if ('screenW' in $$props) $$invalidate(13, screenW = $$props.screenW);
    		if ('screenH' in $$props) $$invalidate(14, screenH = $$props.screenH);
    	};

    	$$self.$capture_state = () => ({
    		onMount,
    		canvas,
    		ctx,
    		scale,
    		isControlPressed,
    		drag,
    		imageObj,
    		srcProp,
    		rectW,
    		rectH,
    		rectStartX,
    		rectStartY,
    		screenW,
    		screenH,
    		loadImg,
    		updateScale,
    		handleKeydown,
    		handleKeyup,
    		drawDown,
    		drawMove,
    		drawUp,
    		drawOut,
    		innerHeight
    	});

    	$$self.$inject_state = $$props => {
    		if ('canvas' in $$props) $$invalidate(0, canvas = $$props.canvas);
    		if ('ctx' in $$props) ctx = $$props.ctx;
    		if ('scale' in $$props) $$invalidate(1, scale = $$props.scale);
    		if ('isControlPressed' in $$props) isControlPressed = $$props.isControlPressed;
    		if ('drag' in $$props) drag = $$props.drag;
    		if ('imageObj' in $$props) imageObj = $$props.imageObj;
    		if ('srcProp' in $$props) $$invalidate(15, srcProp = $$props.srcProp);
    		if ('rectW' in $$props) $$invalidate(9, rectW = $$props.rectW);
    		if ('rectH' in $$props) $$invalidate(10, rectH = $$props.rectH);
    		if ('rectStartX' in $$props) $$invalidate(11, rectStartX = $$props.rectStartX);
    		if ('rectStartY' in $$props) $$invalidate(12, rectStartY = $$props.rectStartY);
    		if ('screenW' in $$props) $$invalidate(13, screenW = $$props.screenW);
    		if ('screenH' in $$props) $$invalidate(14, screenH = $$props.screenH);
    		if ('innerHeight' in $$props) $$invalidate(2, innerHeight = $$props.innerHeight);
    	};

    	if ($$props && "$$inject" in $$props) {
    		$$self.$inject_state($$props.$$inject);
    	}

    	$$self.$$.update = () => {
    		if ($$self.$$.dirty & /*srcProp*/ 32768) {
    			loadImg(srcProp);
    		}
    	};

    	$$invalidate(2, innerHeight = 0);

    	return [
    		canvas,
    		scale,
    		innerHeight,
    		updateScale,
    		handleKeydown,
    		handleKeyup,
    		drawDown,
    		drawMove,
    		drawUp,
    		rectW,
    		rectH,
    		rectStartX,
    		rectStartY,
    		screenW,
    		screenH,
    		srcProp,
    		onwindowresize,
    		canvas_1_binding
    	];
    }

    class Screen extends SvelteComponentDev {
    	constructor(options) {
    		super(options);

    		init(this, options, instance$1, create_fragment$1, safe_not_equal, {
    			srcProp: 15,
    			rectW: 9,
    			rectH: 10,
    			rectStartX: 11,
    			rectStartY: 12,
    			screenW: 13,
    			screenH: 14
    		});

    		dispatch_dev("SvelteRegisterComponent", {
    			component: this,
    			tagName: "Screen",
    			options,
    			id: create_fragment$1.name
    		});

    		const { ctx } = this.$$;
    		const props = options.props || {};

    		if (/*rectW*/ ctx[9] === undefined && !('rectW' in props)) {
    			console.warn("<Screen> was created without expected prop 'rectW'");
    		}

    		if (/*rectH*/ ctx[10] === undefined && !('rectH' in props)) {
    			console.warn("<Screen> was created without expected prop 'rectH'");
    		}

    		if (/*rectStartX*/ ctx[11] === undefined && !('rectStartX' in props)) {
    			console.warn("<Screen> was created without expected prop 'rectStartX'");
    		}

    		if (/*rectStartY*/ ctx[12] === undefined && !('rectStartY' in props)) {
    			console.warn("<Screen> was created without expected prop 'rectStartY'");
    		}

    		if (/*screenW*/ ctx[13] === undefined && !('screenW' in props)) {
    			console.warn("<Screen> was created without expected prop 'screenW'");
    		}

    		if (/*screenH*/ ctx[14] === undefined && !('screenH' in props)) {
    			console.warn("<Screen> was created without expected prop 'screenH'");
    		}
    	}

    	get srcProp() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set srcProp(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get rectW() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set rectW(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get rectH() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set rectH(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get rectStartX() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set rectStartX(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get rectStartY() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set rectStartY(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get screenW() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set screenW(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	get screenH() {
    		throw new Error("<Screen>: Props cannot be read directly from the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}

    	set screenH(value) {
    		throw new Error("<Screen>: Props cannot be set directly on the component instance unless compiling with 'accessors: true' or '<svelte:options accessors/>'");
    	}
    }

    /* src/App.svelte generated by Svelte v3.46.4 */

    const { console: console_1, window: window_1 } = globals;
    const file = "src/App.svelte";

    function create_fragment(ctx) {
    	let div0;
    	let div0_class_value;
    	let t0;
    	let main;
    	let div3;
    	let div2;
    	let div1;
    	let span0;
    	let h3;
    	let t2;
    	let span1;
    	let i0;
    	let t3;
    	let span2;
    	let i1;
    	let t4;
    	let br;
    	let t5;
    	let span5;
    	let i2;
    	let t6;
    	let span3;
    	let t8;
    	let span4;
    	let t10;
    	let t11;
    	let span7;
    	let i3;
    	let t12;
    	let span6;
    	let t14;
    	let screen;
    	let updating_screenW;
    	let updating_screenH;
    	let updating_rectStartX;
    	let updating_rectStartY;
    	let updating_rectW;
    	let updating_rectH;
    	let updating_srcProp;
    	let t15;
    	let footer;
    	let div4;
    	let p0;
    	let t17;
    	let p1;
    	let span8;
    	let t19;
    	let t20;
    	let p2;
    	let i4;
    	let current;
    	let mounted;
    	let dispose;

    	function screen_screenW_binding(value) {
    		/*screen_screenW_binding*/ ctx[17](value);
    	}

    	function screen_screenH_binding(value) {
    		/*screen_screenH_binding*/ ctx[18](value);
    	}

    	function screen_rectStartX_binding(value) {
    		/*screen_rectStartX_binding*/ ctx[19](value);
    	}

    	function screen_rectStartY_binding(value) {
    		/*screen_rectStartY_binding*/ ctx[20](value);
    	}

    	function screen_rectW_binding(value) {
    		/*screen_rectW_binding*/ ctx[21](value);
    	}

    	function screen_rectH_binding(value) {
    		/*screen_rectH_binding*/ ctx[22](value);
    	}

    	function screen_srcProp_binding(value) {
    		/*screen_srcProp_binding*/ ctx[23](value);
    	}

    	let screen_props = {};

    	if (/*screenW*/ ctx[4] !== void 0) {
    		screen_props.screenW = /*screenW*/ ctx[4];
    	}

    	if (/*screenH*/ ctx[5] !== void 0) {
    		screen_props.screenH = /*screenH*/ ctx[5];
    	}

    	if (/*x1*/ ctx[0] !== void 0) {
    		screen_props.rectStartX = /*x1*/ ctx[0];
    	}

    	if (/*y1*/ ctx[1] !== void 0) {
    		screen_props.rectStartY = /*y1*/ ctx[1];
    	}

    	if (/*x2*/ ctx[2] !== void 0) {
    		screen_props.rectW = /*x2*/ ctx[2];
    	}

    	if (/*y2*/ ctx[3] !== void 0) {
    		screen_props.rectH = /*y2*/ ctx[3];
    	}

    	if (/*screenComponent*/ ctx[6] !== void 0) {
    		screen_props.srcProp = /*screenComponent*/ ctx[6];
    	}

    	screen = new Screen({ props: screen_props, $$inline: true });
    	binding_callbacks.push(() => bind(screen, 'screenW', screen_screenW_binding));
    	binding_callbacks.push(() => bind(screen, 'screenH', screen_screenH_binding));
    	binding_callbacks.push(() => bind(screen, 'rectStartX', screen_rectStartX_binding));
    	binding_callbacks.push(() => bind(screen, 'rectStartY', screen_rectStartY_binding));
    	binding_callbacks.push(() => bind(screen, 'rectW', screen_rectW_binding));
    	binding_callbacks.push(() => bind(screen, 'rectH', screen_rectH_binding));
    	binding_callbacks.push(() => bind(screen, 'srcProp', screen_srcProp_binding));

    	const block = {
    		c: function create() {
    			div0 = element("div");
    			t0 = space();
    			main = element("main");
    			div3 = element("div");
    			div2 = element("div");
    			div1 = element("div");
    			span0 = element("span");
    			h3 = element("h3");
    			h3.textContent = "blockevent";
    			t2 = space();
    			span1 = element("span");
    			i0 = element("i");
    			t3 = space();
    			span2 = element("span");
    			i1 = element("i");
    			t4 = text("drop anywhere to upload ");
    			br = element("br");
    			t5 = space();
    			span5 = element("span");
    			i2 = element("i");
    			t6 = text("use ");
    			span3 = element("span");
    			span3.textContent = "SHIFT";
    			t8 = text("+");
    			span4 = element("span");
    			span4.textContent = "SCROLL";
    			t10 = text(" to zoom");
    			t11 = space();
    			span7 = element("span");
    			i3 = element("i");
    			t12 = text("draw with ");
    			span6 = element("span");
    			span6.textContent = "LEFT CLICK";
    			t14 = space();
    			create_component(screen.$$.fragment);
    			t15 = space();
    			footer = element("footer");
    			div4 = element("div");
    			p0 = element("p");
    			p0.textContent = ">_";
    			t17 = space();
    			p1 = element("p");
    			span8 = element("span");
    			span8.textContent = "./blockevent ";
    			t19 = text(/*outputCmd*/ ctx[8]);
    			t20 = space();
    			p2 = element("p");
    			i4 = element("i");
    			attr_dev(div0, "id", "dropzone");

    			attr_dev(div0, "class", div0_class_value = "overlay dropzone " + (/*fileDropping*/ ctx[7] === true
    			? 'dropzone_show'
    			: 'dropzone_hide') + " svelte-1qtrke0");

    			add_location(div0, file, 90, 0, 1527);
    			attr_dev(h3, "class", "svelte-1qtrke0");
    			add_location(h3, file, 95, 63, 1883);
    			attr_dev(span0, "class", "logo floating_menu_element unselectable_text svelte-1qtrke0");
    			add_location(span0, file, 95, 4, 1824);
    			attr_dev(i0, "class", "bicon-github svelte-1qtrke0");
    			add_location(i0, file, 96, 97, 2007);
    			attr_dev(span1, "class", "logo floating_menu_element floating_menu_button clearfix svelte-1qtrke0");
    			add_location(span1, file, 96, 4, 1914);
    			attr_dev(div1, "class", "header svelte-1qtrke0");
    			add_location(div1, file, 94, 3, 1799);
    			attr_dev(i1, "class", "bicon-upload floating_menu_element_small_icon svelte-1qtrke0");
    			add_location(i1, file, 98, 39, 2092);
    			add_location(br, file, 98, 124, 2177);
    			attr_dev(span2, "class", "floating_menu_element svelte-1qtrke0");
    			add_location(span2, file, 98, 3, 2056);
    			attr_dev(i2, "class", "bicon-zoom_out_map floating_menu_element_small_icon svelte-1qtrke0");
    			add_location(i2, file, 99, 39, 2228);
    			attr_dev(span3, "class", "key_emboss key_emboss_combine_left svelte-1qtrke0");
    			add_location(span3, file, 99, 110, 2299);
    			attr_dev(span4, "class", "key_emboss key_emboss_combine_right svelte-1qtrke0");
    			add_location(span4, file, 99, 172, 2361);
    			attr_dev(span5, "class", "floating_menu_element svelte-1qtrke0");
    			add_location(span5, file, 99, 3, 2192);
    			attr_dev(i3, "class", "bicon-create floating_menu_element_small_icon svelte-1qtrke0");
    			add_location(i3, file, 100, 39, 2479);
    			attr_dev(span6, "class", "key_emboss svelte-1qtrke0");
    			add_location(span6, file, 100, 110, 2550);
    			attr_dev(span7, "class", "floating_menu_element svelte-1qtrke0");
    			add_location(span7, file, 100, 3, 2443);
    			attr_dev(div2, "class", "floating_menu svelte-1qtrke0");
    			add_location(div2, file, 93, 2, 1768);
    			attr_dev(div3, "id", "container");
    			attr_dev(div3, "class", "svelte-1qtrke0");
    			add_location(div3, file, 92, 1, 1745);
    			attr_dev(p0, "class", "terminal_element terminal_element_left unselectable_text svelte-1qtrke0");
    			add_location(p0, file, 107, 3, 2844);
    			attr_dev(span8, "class", "unselectable_text svelte-1qtrke0");
    			add_location(span8, file, 111, 4, 2979);
    			attr_dev(p1, "class", "terminal_element terminal_code svelte-1qtrke0");
    			add_location(p1, file, 110, 3, 2931);
    			attr_dev(i4, "class", "bicon-content_copy svelte-1qtrke0");
    			add_location(i4, file, 114, 4, 3164);
    			attr_dev(p2, "class", "terminal_element terminal_element_right clearfix unselectable_text svelte-1qtrke0");
    			add_location(p2, file, 113, 3, 3054);
    			attr_dev(div4, "class", "terminal svelte-1qtrke0");
    			add_location(div4, file, 106, 2, 2818);
    			attr_dev(footer, "class", "footer svelte-1qtrke0");
    			add_location(footer, file, 105, 1, 2792);
    			attr_dev(main, "class", "svelte-1qtrke0");
    			add_location(main, file, 91, 0, 1737);
    		},
    		l: function claim(nodes) {
    			throw new Error("options.hydrate only works if the component was compiled with the `hydratable: true` option");
    		},
    		m: function mount(target, anchor) {
    			insert_dev(target, div0, anchor);
    			insert_dev(target, t0, anchor);
    			insert_dev(target, main, anchor);
    			append_dev(main, div3);
    			append_dev(div3, div2);
    			append_dev(div2, div1);
    			append_dev(div1, span0);
    			append_dev(span0, h3);
    			append_dev(div1, t2);
    			append_dev(div1, span1);
    			append_dev(span1, i0);
    			append_dev(div2, t3);
    			append_dev(div2, span2);
    			append_dev(span2, i1);
    			append_dev(span2, t4);
    			append_dev(span2, br);
    			append_dev(div2, t5);
    			append_dev(div2, span5);
    			append_dev(span5, i2);
    			append_dev(span5, t6);
    			append_dev(span5, span3);
    			append_dev(span5, t8);
    			append_dev(span5, span4);
    			append_dev(span5, t10);
    			append_dev(div2, t11);
    			append_dev(div2, span7);
    			append_dev(span7, i3);
    			append_dev(span7, t12);
    			append_dev(span7, span6);
    			append_dev(div3, t14);
    			mount_component(screen, div3, null);
    			append_dev(main, t15);
    			append_dev(main, footer);
    			append_dev(footer, div4);
    			append_dev(div4, p0);
    			append_dev(div4, t17);
    			append_dev(div4, p1);
    			append_dev(p1, span8);
    			append_dev(p1, t19);
    			append_dev(div4, t20);
    			append_dev(div4, p2);
    			append_dev(p2, i4);
    			current = true;

    			if (!mounted) {
    				dispose = [
    					listen_dev(window_1, "dragenter", /*showDropZone*/ ctx[9], false, false, false),
    					listen_dev(div0, "dragenter", allowDrag, false, false, false),
    					listen_dev(div0, "dragover", allowDrag, false, false, false),
    					listen_dev(div0, "dragleave", /*hideDropZone*/ ctx[10], false, false, false),
    					listen_dev(div0, "drop", /*dropHandler*/ ctx[11], false, false, false),
    					listen_dev(span1, "click", gotoGithub, false, false, false),
    					listen_dev(p2, "click", /*copyToClipboard*/ ctx[12], false, false, false)
    				];

    				mounted = true;
    			}
    		},
    		p: function update(ctx, [dirty]) {
    			if (!current || dirty & /*fileDropping*/ 128 && div0_class_value !== (div0_class_value = "overlay dropzone " + (/*fileDropping*/ ctx[7] === true
    			? 'dropzone_show'
    			: 'dropzone_hide') + " svelte-1qtrke0")) {
    				attr_dev(div0, "class", div0_class_value);
    			}

    			const screen_changes = {};

    			if (!updating_screenW && dirty & /*screenW*/ 16) {
    				updating_screenW = true;
    				screen_changes.screenW = /*screenW*/ ctx[4];
    				add_flush_callback(() => updating_screenW = false);
    			}

    			if (!updating_screenH && dirty & /*screenH*/ 32) {
    				updating_screenH = true;
    				screen_changes.screenH = /*screenH*/ ctx[5];
    				add_flush_callback(() => updating_screenH = false);
    			}

    			if (!updating_rectStartX && dirty & /*x1*/ 1) {
    				updating_rectStartX = true;
    				screen_changes.rectStartX = /*x1*/ ctx[0];
    				add_flush_callback(() => updating_rectStartX = false);
    			}

    			if (!updating_rectStartY && dirty & /*y1*/ 2) {
    				updating_rectStartY = true;
    				screen_changes.rectStartY = /*y1*/ ctx[1];
    				add_flush_callback(() => updating_rectStartY = false);
    			}

    			if (!updating_rectW && dirty & /*x2*/ 4) {
    				updating_rectW = true;
    				screen_changes.rectW = /*x2*/ ctx[2];
    				add_flush_callback(() => updating_rectW = false);
    			}

    			if (!updating_rectH && dirty & /*y2*/ 8) {
    				updating_rectH = true;
    				screen_changes.rectH = /*y2*/ ctx[3];
    				add_flush_callback(() => updating_rectH = false);
    			}

    			if (!updating_srcProp && dirty & /*screenComponent*/ 64) {
    				updating_srcProp = true;
    				screen_changes.srcProp = /*screenComponent*/ ctx[6];
    				add_flush_callback(() => updating_srcProp = false);
    			}

    			screen.$set(screen_changes);
    			if (!current || dirty & /*outputCmd*/ 256) set_data_dev(t19, /*outputCmd*/ ctx[8]);
    		},
    		i: function intro(local) {
    			if (current) return;
    			transition_in(screen.$$.fragment, local);
    			current = true;
    		},
    		o: function outro(local) {
    			transition_out(screen.$$.fragment, local);
    			current = false;
    		},
    		d: function destroy(detaching) {
    			if (detaching) detach_dev(div0);
    			if (detaching) detach_dev(t0);
    			if (detaching) detach_dev(main);
    			destroy_component(screen);
    			mounted = false;
    			run_all(dispose);
    		}
    	};

    	dispatch_dev("SvelteRegisterBlock", {
    		block,
    		id: create_fragment.name,
    		type: "component",
    		source: "",
    		ctx
    	});

    	return block;
    }

    function gotoGithub() {
    	window.open('https://github.com/nmelihsensoy/blockevent', '_blank');
    }

    function allowDrag(e) {
    	e.dataTransfer.dropEffect = 'copy';
    	e.preventDefault();
    }

    function instance($$self, $$props, $$invalidate) {
    	let outputCmd;
    	let { $$slots: slots = {}, $$scope } = $$props;
    	validate_slots('App', slots, []);
    	let screenComponent;
    	let x1 = 0;
    	let y1 = 0;
    	let x2 = 0;
    	let y2 = 0;
    	let screenW = 0;
    	let screenH = 0;
    	let lbx = 0;
    	let lby = 0;
    	let rtx = 0;
    	let rty = 0;
    	let fileDropping = false;

    	function showDropZone(e) {
    		console.log(e);
    		$$invalidate(7, fileDropping = true);
    	}

    	function hideDropZone(e) {
    		$$invalidate(7, fileDropping = false);
    	}

    	function dropHandler(ev) {
    		console.log('File(s) dropped');
    		ev.stopPropagation();
    		ev.preventDefault();
    		hideDropZone();
    		var url = ev.dataTransfer.getData('text/plain');

    		if (url) {
    			$$invalidate(6, screenComponent = url);
    		} else {
    			var file = ev.dataTransfer.files[0];
    			var reader = new FileReader();
    			reader.readAsDataURL(file);
    			$$invalidate(6, screenComponent = file);

    			reader.onload = function () {
    				$$invalidate(6, screenComponent = reader.result);
    			};
    		}
    	}

    	function copyToClipboard() {
    		navigator.clipboard.writeText(outputCmd).then(() => {
    			console.log("copied");
    			console.log(outputCmd);
    		}).catch(() => {
    			
    		});
    	}

    	const writable_props = [];

    	Object.keys($$props).forEach(key => {
    		if (!~writable_props.indexOf(key) && key.slice(0, 2) !== '$$' && key !== 'slot') console_1.warn(`<App> was created with unknown prop '${key}'`);
    	});

    	function screen_screenW_binding(value) {
    		screenW = value;
    		$$invalidate(4, screenW);
    	}

    	function screen_screenH_binding(value) {
    		screenH = value;
    		$$invalidate(5, screenH);
    	}

    	function screen_rectStartX_binding(value) {
    		x1 = value;
    		$$invalidate(0, x1);
    	}

    	function screen_rectStartY_binding(value) {
    		y1 = value;
    		$$invalidate(1, y1);
    	}

    	function screen_rectW_binding(value) {
    		x2 = value;
    		$$invalidate(2, x2);
    	}

    	function screen_rectH_binding(value) {
    		y2 = value;
    		$$invalidate(3, y2);
    	}

    	function screen_srcProp_binding(value) {
    		screenComponent = value;
    		$$invalidate(6, screenComponent);
    	}

    	$$self.$capture_state = () => ({
    		Screen,
    		screenComponent,
    		x1,
    		y1,
    		x2,
    		y2,
    		screenW,
    		screenH,
    		lbx,
    		lby,
    		rtx,
    		rty,
    		fileDropping,
    		gotoGithub,
    		showDropZone,
    		hideDropZone,
    		allowDrag,
    		dropHandler,
    		copyToClipboard,
    		outputCmd
    	});

    	$$self.$inject_state = $$props => {
    		if ('screenComponent' in $$props) $$invalidate(6, screenComponent = $$props.screenComponent);
    		if ('x1' in $$props) $$invalidate(0, x1 = $$props.x1);
    		if ('y1' in $$props) $$invalidate(1, y1 = $$props.y1);
    		if ('x2' in $$props) $$invalidate(2, x2 = $$props.x2);
    		if ('y2' in $$props) $$invalidate(3, y2 = $$props.y2);
    		if ('screenW' in $$props) $$invalidate(4, screenW = $$props.screenW);
    		if ('screenH' in $$props) $$invalidate(5, screenH = $$props.screenH);
    		if ('lbx' in $$props) $$invalidate(13, lbx = $$props.lbx);
    		if ('lby' in $$props) $$invalidate(14, lby = $$props.lby);
    		if ('rtx' in $$props) $$invalidate(15, rtx = $$props.rtx);
    		if ('rty' in $$props) $$invalidate(16, rty = $$props.rty);
    		if ('fileDropping' in $$props) $$invalidate(7, fileDropping = $$props.fileDropping);
    		if ('outputCmd' in $$props) $$invalidate(8, outputCmd = $$props.outputCmd);
    	};

    	if ($$props && "$$inject" in $$props) {
    		$$self.$inject_state($$props.$$inject);
    	}

    	$$self.$$.update = () => {
    		if ($$self.$$.dirty & /*x2, x1*/ 5) {
    			if (x2 > 0) {
    				$$invalidate(13, lbx = x1);
    				$$invalidate(15, rtx = x1 + x2);
    			} else {
    				$$invalidate(13, lbx = x1 + x2);
    				$$invalidate(15, rtx = x1);
    			}
    		}

    		if ($$self.$$.dirty & /*y2, y1*/ 10) {
    			if (y2 > 0) {
    				$$invalidate(14, lby = y2);
    				$$invalidate(16, rty = y1);
    			} else {
    				$$invalidate(14, lby = y1);
    				$$invalidate(16, rty = y1 + y2);
    			}
    		}

    		if ($$self.$$.dirty & /*lbx, lby, rtx, rty, screenW, screenH*/ 122928) {
    			$$invalidate(8, outputCmd = `-r ${lbx.toFixed(0)},${lby.toFixed(0)},${rtx.toFixed(0)},${rty.toFixed(0)} -W ${screenW} -H ${screenH}`);
    		}
    	};

    	return [
    		x1,
    		y1,
    		x2,
    		y2,
    		screenW,
    		screenH,
    		screenComponent,
    		fileDropping,
    		outputCmd,
    		showDropZone,
    		hideDropZone,
    		dropHandler,
    		copyToClipboard,
    		lbx,
    		lby,
    		rtx,
    		rty,
    		screen_screenW_binding,
    		screen_screenH_binding,
    		screen_rectStartX_binding,
    		screen_rectStartY_binding,
    		screen_rectW_binding,
    		screen_rectH_binding,
    		screen_srcProp_binding
    	];
    }

    class App extends SvelteComponentDev {
    	constructor(options) {
    		super(options);
    		init(this, options, instance, create_fragment, safe_not_equal, {});

    		dispatch_dev("SvelteRegisterComponent", {
    			component: this,
    			tagName: "App",
    			options,
    			id: create_fragment.name
    		});
    	}
    }

    const app = new App({
    	target: document.body,
    	props: {
    	}
    });

    return app;

})();
//# sourceMappingURL=bundle.js.map
