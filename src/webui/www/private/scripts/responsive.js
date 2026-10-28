/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.Responsive ??= (() => {
    const exports = () => {
        return {
            init: init,
            queries: queries,
            addFrame: addFrame,
            rowHeight: rowHeight,
            contextMenuShown: contextMenuShown,
            contextMenuHidden: contextMenuHidden
        };
    };

    // the same breakpoints as responsive.css
    const queries = Object.freeze({
        phone: window.matchMedia("(width < 760px)"),
        drawer: window.matchMedia("(width < 1100px)"),
        relayout: window.matchMedia("(pointer: coarse), (width < 1100px)"),
        small: window.matchMedia("(width < 760px), (height < 620px), (pointer: coarse) and (height < 900px)"),
        coarse: window.matchMedia("(pointer: coarse)")
    });

    const root = document.documentElement;
    const localPreferences = new window.qBittorrent.LocalPreferences.LocalPreferences();

    const cssLength = (name) => Number.parseFloat(getComputedStyle(root).getPropertyValue(name));

    const frameDocuments = new Set();

    const setClasses = (doc) => {
        doc.documentElement.classList.toggle("responsivePhone", queries.phone.matches);
        doc.documentElement.classList.toggle("responsiveRelayout", queries.relayout.matches);
    };

    // called by color-scheme.js in dialog frames
    const addFrame = (doc) => {
        const stylesheet = doc.createElement("link");
        stylesheet.rel = "stylesheet";
        stylesheet.type = "text/css";
        stylesheet.href = document.getElementById("responsiveStylesheet").href;
        doc.head.append(stylesheet);
        setClasses(doc);
        frameDocuments.add(doc);
        doc.defaultView.addEventListener("pagehide", (_event) => frameDocuments.delete(doc));
    };

    for (const query of [queries.phone, queries.relayout]) {
        query.addEventListener("change", (_event) => {
            setClasses(document);
            for (const doc of frameDocuments)
                setClasses(doc);
        });
    }
    setClasses(document);

    const addScrim = (onClick) => {
        const scrim = document.createElement("div");
        scrim.className = "responsiveScrim";
        scrim.addEventListener("click", (_event) => onClick());
        document.body.append(scrim);
        return scrim;
    };

    const whenElement = (id) => new Promise((resolve) => {
        const found = document.getElementById(id);
        if (found !== null) {
            resolve(found);
            return;
        }
        const observer = new MutationObserver(() => {
            const element = document.getElementById(id);
            if (element === null)
                return;

            observer.disconnect();
            resolve(element);
        });
        observer.observe(document.body, { childList: true, subtree: true });
    });

    const focusableSelector = "a[href], button:not([disabled]), input:not([disabled], [type='hidden']), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex='-1'])";
    // keep the focus inside an open drawer
    const trapFocus = (container) => {
        const returnTo = document.activeElement;
        // shown, and not hidden by visibility, as the toggles of categories without subcategories are
        const focusable = () => [...container.querySelectorAll(focusableSelector)].filter((element) => (element.getClientRects().length > 0) && (getComputedStyle(element).visibility !== "hidden"));
        const first = focusable()[0];
        if (first !== undefined) {
            first.focus();
        }
        else {
            container.tabIndex = -1;
            container.focus();
        }
        const onKeyDown = (event) => {
            if (event.key !== "Tab")
                return;

            const items = focusable();
            if (items.length === 0) {
                event.preventDefault();
                return;
            }
            const index = items.indexOf(document.activeElement);
            if (event.shiftKey && (index <= 0)) {
                event.preventDefault();
                items.at(-1).focus();
            }
            else if (!event.shiftKey && ((index === -1) || (index === (items.length - 1)))) {
                event.preventDefault();
                items[0].focus();
            }
        };

        const listening = new AbortController();
        document.addEventListener("keydown", ((event) => onKeyDown(event)), { capture: true, signal: listening.signal });
        return () => {
            listening.abort();
            if ((returnTo !== null) && returnTo.isConnected)
                returnTo.focus();
        };
    };

    // make a drawer's items reachable with Tab while it is open
    const setKeyboardItems = (container, selector, on) => {
        for (const item of container.querySelectorAll(selector)) {
            if (on)
                item.tabIndex = 0;
            else
                item.removeAttribute("tabindex");
        }
    };

    const chooseWithKeys = (container, selector) => {
        container.addEventListener("keydown", (event) => {
            switch (event.key) {
                case "Enter":
                case " ":
                    if (!event.target.matches(selector) || !event.target.hasAttribute("tabindex"))
                        break;

                    event.preventDefault();
                    event.target.click();
                    break;
            }
        });
    };

    // on small screens, the properties panel shrinks until the list has this share of the page
    const MIN_TRANSFER_LIST_SHARE = 0.45;
    const MIN_PROPERTIES_HEIGHT = 60;

    const fitPropertiesPanel = () => {
        const mainColumn = document.getElementById("mainColumn");
        const list = document.getElementById("transferList");
        const properties = document.getElementById("propertiesPanel");
        if (!queries.small.matches)
            return false;

        // hidden or collapsed
        if ((mainColumn.offsetHeight === 0) || (properties.offsetHeight === 0))
            return false;

        const missing = Math.round(mainColumn.offsetHeight * MIN_TRANSFER_LIST_SHARE) - list.offsetHeight;
        if (missing <= 0)
            return false;

        const height = Math.max(MIN_PROPERTIES_HEIGHT, properties.offsetHeight - missing);
        const given = properties.offsetHeight - height;
        if (given <= 0)
            return false;

        // Mocha shares the height in proportion, so set both
        properties.style.height = `${height}px`;
        list.style.height = `${list.offsetHeight + given}px`;
        return true;
    };

    // leaving the small layout restores the panel height the user set (this share), once the panel has a height
    let restoreProperties = null;
    let propertiesObserver = null;

    const restorePropertiesPanel = () => {
        const list = document.getElementById("transferList");
        const properties = document.getElementById("propertiesPanel");
        if ((list === null) || (properties === null))
            return false;
        // collapsed, or hidden behind another tab
        if (properties.offsetHeight === 0) {
            propertiesObserver.observe(properties);
            return false;
        }

        const share = restoreProperties;
        restoreProperties = null;
        const total = list.offsetHeight + properties.offsetHeight;
        const height = Math.min(Math.round(share * root.clientHeight), total - MIN_PROPERTIES_HEIGHT);
        properties.style.height = `${height}px`;
        list.style.height = `${total - height}px`;
        return true;
    };

    // while the header slides, the properties panel keeps this height and the list gets all of the header's (Mocha
    // would share it between both)
    let keptPropertiesHeight = 0;

    const keepPropertiesPanel = () => {
        const list = document.getElementById("transferList");
        const properties = document.getElementById("propertiesPanel");
        if ((keptPropertiesHeight === 0) || (properties.offsetHeight === 0) || (properties.offsetHeight === keptPropertiesHeight))
            return false;

        list.style.height = `${list.offsetHeight + properties.offsetHeight - keptPropertiesHeight}px`;
        properties.style.height = `${keptPropertiesHeight}px`;
        return true;
    };

    // Mocha relayouts only when the browser window is resized
    const relayout = () => {
        MochaUI.Desktop.setDesktopSize();
        const kept = keepPropertiesPanel();
        if (((restoreProperties !== null) ? restorePropertiesPanel() : fitPropertiesPanel()) || kept)
            MochaUI.Desktop.setDesktopSize();
    };

    const initLayout = () => {
        const relayoutSoon = window.qBittorrent.Misc.createDebounceHandler(50, relayout);
        const observer = new ResizeObserver(relayoutSoon);
        observer.observe(document.getElementById("desktopHeader"));
        observer.observe(document.getElementById("desktopFooterWrapper"));
        observer.observe(document.getElementById("filtersColumn"));
        // after Mocha's own resize handling
        window.addEventListener("resize", (_event) => relayoutSoon());
        // read now, as client.js stores the share the panel has after the resize
        queries.small.addEventListener("change", (_event) => {
            restoreProperties = queries.small.matches ? null : Number(localPreferences.get("properties_height_rel", 0.5));
            relayoutSoon();
        });
        propertiesObserver = new ResizeObserver(() => {
            if (restoreProperties !== null)
                relayoutSoon();
        });
    };

    // drawers, and windows on phones, cover the page as pages do, so Back closes the one on top rather than leave the
    // WebUI: each adds a history entry as it opens, and one that closes some other way takes its entry back
    const historyLayers = [];
    let ignoredPops = 0;
    // a Back takes its time: an entry pushed before it arrives is the one it goes back from, so the entries of layers
    // added meanwhile, as a window that a menu drawer entry opens, wait for it
    let waitingPushes = 0;

    const pushLayerEntry = () => {
        if (ignoredPops > 0)
            ++waitingPushes;
        else
            history.pushState({ responsiveLayer: true }, "");
    };

    const addHistoryLayer = (close) => {
        const layer = { close: close };
        historyLayers.push(layer);
        pushLayerEntry();
        return layer;
    };

    const removeHistoryLayer = (layer) => {
        const index = historyLayers.indexOf(layer);
        // Back closed it, and took its entry
        if (index === -1)
            return;

        historyLayers.splice(index, 1);
        // the layers' entries are alike, so one under the top takes the current one back too, unless one is still
        // waiting
        if (waitingPushes > 0) {
            --waitingPushes;
        }
        else {
            ++ignoredPops;
            history.back();
        }
    };

    const initHistoryLayers = () => {
        window.addEventListener("popstate", (_event) => {
            if (ignoredPops > 0) {
                --ignoredPops;
                while ((ignoredPops === 0) && (waitingPushes > 0)) {
                    --waitingPushes;
                    history.pushState({ responsiveLayer: true }, "");
                }
                return;
            }

            const layer = historyLayers.pop();
            if (layer === undefined)
                return;

            layer.close();
        });
    };

    const filterItems = "ul.filterList span.link";

    let filtersScrim = null;
    let filtersObserver = null;
    let releaseFiltersFocus = null;
    let filtersLayer = null;
    // the rest of a double click or tap that closed a drawer would reach what the drawer covered
    let drawerClosed = false;

    const openFiltersDrawer = () => {
        if (!queries.drawer.matches || root.classList.contains("filtersDrawerOpen"))
            return;

        const column = document.getElementById("filtersColumn");
        const filters = document.getElementById("Filters");
        root.classList.add("filtersDrawerOpen");
        filtersScrim = addScrim(closeFiltersDrawer);
        column.setAttribute("role", "dialog");
        column.setAttribute("aria-modal", "true");
        column.setAttribute("aria-labelledby", "filtersDrawerTitle");
        document.getElementById("filtersButton").setAttribute("aria-expanded", "true");
        setKeyboardItems(filters, filterItems, true);
        // the filter lists are rebuilt as torrents change
        filtersObserver = new MutationObserver(() => setKeyboardItems(filters, filterItems, true));
        filtersObserver.observe(filters, { childList: true, subtree: true });
        releaseFiltersFocus = trapFocus(column);
        filtersLayer = addHistoryLayer(closeFiltersDrawer);
    };

    const closeFiltersDrawer = () => {
        if (!root.classList.contains("filtersDrawerOpen"))
            return;

        const column = document.getElementById("filtersColumn");
        root.classList.remove("filtersDrawerOpen");
        filtersScrim.remove();
        filtersScrim = null;
        column.removeAttribute("role");
        column.removeAttribute("aria-modal");
        column.removeAttribute("aria-labelledby");
        document.getElementById("filtersButton").setAttribute("aria-expanded", "false");
        filtersObserver.disconnect();
        filtersObserver = null;
        setKeyboardItems(document.getElementById("Filters"), filterItems, false);
        releaseFiltersFocus();
        releaseFiltersFocus = null;
        removeHistoryLayer(filtersLayer);
        filtersLayer = null;
        drawerClosed = true;
    };

    const initFiltersDrawer = () => {
        const button = document.getElementById("filtersButton");
        const column = document.getElementById("filtersColumn");
        button.setAttribute("aria-controls", "filtersColumn");
        button.addEventListener("click", (_event) => openFiltersDrawer());
        column.addEventListener("click", (event) => {
            if (event.target.closest("ul.filterList li, #filtersDrawerClose") !== null)
                closeFiltersDrawer();
        });
        for (const type of ["click", "dblclick"]) {
            document.addEventListener(type, (event) => {
                if (!drawerClosed)
                    return;
                // a new click
                if ((event.type === "click") && (event.detail < 2)) {
                    drawerClosed = false;
                    return;
                }

                event.preventDefault();
                event.stopPropagation();
            }, true);
        }
        chooseWithKeys(column, filterItems);
        queries.drawer.addEventListener("change", (_event) => {
            if (!queries.drawer.matches)
                closeFiltersDrawer();
        });
        document.addEventListener("keydown", (event) => {
            switch (event.key) {
                case "Escape":
                    // close the drawer, not also the window behind it
                    if (root.classList.contains("filtersDrawerOpen"))
                        event.stopPropagation();
                    closeFiltersDrawer();
                    break;
            }
        });

        // the button moves to the menubar while the toolbar is hidden
        const toolbar = document.getElementById("mochaToolbar");
        const home = button.parentElement;
        const homeNext = button.nextSibling;
        const placeButton = () => {
            if (toolbar.classList.contains("invisible"))
                document.getElementById("desktopNavbar").append(button);
            else if (button.parentElement !== home)
                home.insertBefore(button, homeNext);
        };
        new MutationObserver(placeButton).observe(toolbar, { attributes: true, attributeFilter: ["class"] });
        placeButton();
    };

    // the virtual list's row height for dynamicTable.js, or undefined for its own
    const rowHeight = (table) => {
        if (queries.phone.matches && (table.dynamicTableDivId === "torrentsTableDiv"))
            return cssLength("--responsive-card-height");
        if (queries.coarse.matches)
            return cssLength("--responsive-touch-row-height");
        return undefined;
    };

    // keep the first visible row in view while row heights change
    const FIRST_ROW_TIME = 1000;
    let shownRowHeight = null;
    let firstRow = null;

    const measureRowHeight = (table) => {
        if (table.useVirtualList)
            return table.rowHeight;
        const height = table.tableBody.querySelector("tr")?.offsetHeight ?? 0;
        return (height > 0) ? height : table.rowHeight;
    };

    const scrollToFirstRow = () => {
        if ((firstRow === null) || (performance.now() > firstRow.until)) {
            firstRow = null;
            return;
        }
        window.torrentsTable.dynamicTableDiv.scrollTop = Math.round(firstRow.index * shownRowHeight);
    };

    const keepFirstRow = () => {
        const table = window.torrentsTable;
        if (shownRowHeight === null)
            return;

        firstRow = { index: table.dynamicTableDiv.scrollTop / shownRowHeight, until: performance.now() + FIRST_ROW_TIME };
        if (table.useVirtualList)
            table.rerender();
        shownRowHeight = measureRowHeight(table);
        scrollToFirstRow();
    };

    let mochaTitleHeights = null;

    // new windows take their title bar height from responsive.css
    const applyWindowTitleHeight = () => {
        mochaTitleHeights ??= {
            window: MochaUI.Window.prototype.options.headerHeight,
            modal: MochaUI.Modal.prototype.options.headerHeight
        };
        const height = cssLength("--responsive-window-title-height");
        MochaUI.Window.prototype.options.headerHeight = (height > 0) ? height : mochaTitleHeights.window;
        MochaUI.Modal.prototype.options.headerHeight = (height > 0) ? height : mochaTitleHeights.modal;
    };

    const initTouch = async () => {
        applyWindowTitleHeight();
        queries.coarse.addEventListener("change", (_event) => applyWindowTitleHeight());

        // touch screens put Remove after Start and Stop, set apart as in the transfer list's menu, so that a finger
        // aimed at the buttons used most doesn't land on it; it moves in the page, so that the keyboard follows, and
        // takes the space before it along
        const removeButton = document.getElementById("deleteButton");
        const removeSpace = removeButton.previousSibling;
        const removeHome = removeButton.nextSibling;
        const placeRemoveButton = () => {
            if (queries.coarse.matches)
                document.getElementById("stopButton").after(removeSpace, removeButton);
            else
                removeHome.before(removeSpace, removeButton);
            removeButton.classList.toggle("divider", queries.coarse.matches);
        };
        queries.coarse.addEventListener("change", (_event) => placeRemoveButton());
        placeRemoveButton();

        const tableDiv = await whenElement("torrentsTableDiv");
        shownRowHeight = measureRowHeight(window.torrentsTable);
        queries.coarse.addEventListener("change", (_event) => keepFirstRow());
        new ResizeObserver(scrollToFirstRow).observe(tableDiv);
    };

    const WINDOW_MARGIN = 8;
    const PHONE_WINDOW_MARGIN = 12;
    const MIN_WINDOW_WIDTH = 300;
    const MIN_WINDOW_HEIGHT = 240;
    const MAX_WINDOW_WIDTH = 1200;
    const MAX_WINDOW_HEIGHT = 800;
    // windows whose tabs share one size
    const ONE_SIZE_WINDOWS = new Set(["preferencesPage"]);

    // resize like an edge drag, without storing the size
    const setContentSize = (instance, width, height) => {
        const wrapper = instance.contentWrapperEl;
        if ((wrapper.style.width === width) && (wrapper.style.height === height))
            return;

        wrapper.style.width = width;
        wrapper.style.height = height;
        instance.drawWindow();
        if (instance.options.resizable)
            instance.adjustHandles();
        MochaUI.rWidth(wrapper);
        for (const column of wrapper.querySelectorAll(":scope > .column"))
            MochaUI.panelHeight(column);
    };

    // the size the window asks for, rather than the one it was fitted to
    const wantedSize = (instance) => {
        const { width, height } = instance.contentWrapperEl.style;
        const fitted = instance.responsiveFittedSize;
        if ((fitted === undefined) || (width !== fitted.width) || (height !== fitted.height))
            instance.responsiveWantedSize = { width: width, height: height };
        return instance.responsiveWantedSize;
    };

    // the frame's document or the content box; null while the page loads
    const pageOf = (instance) => {
        const iframe = instance.iframeEl;
        if (iframe !== undefined) {
            const loading = (iframe.contentDocument?.readyState !== "complete") || (iframe.contentWindow.location.href === "about:blank");
            return loading ? null : iframe.contentDocument.documentElement;
        }
        return (instance.contentEl.childElementCount === 0) ? null : instance.contentWrapperEl;
    };

    // measure the page at the given size, then restore its size and scroll positions
    const measurePage = (instance, width, height, measure) => {
        const page = pageOf(instance);
        if (page === null)
            return null;

        const box = instance.iframeEl ?? page;
        const scrolled = [box, page, ...page.querySelectorAll("*")]
            .filter((element) => (element.scrollTop > 0) || (element.scrollLeft > 0))
            .map((element) => ({ element: element, top: element.scrollTop, left: element.scrollLeft }));
        const { width: oldWidth, height: oldHeight } = box.style;
        const oldOverflow = [page.style.getPropertyValue("overflow"), page.style.getPropertyPriority("overflow")];
        box.style.width = `${width}px`;
        box.style.height = `${height}px`;
        // no scrollbar while measuring
        page.style.setProperty("overflow", "hidden", "important");
        const result = measure(page);
        page.style.setProperty("overflow", ...oldOverflow);
        box.style.width = oldWidth;
        box.style.height = oldHeight;
        for (const { element, top, left } of scrolled) {
            element.scrollTop = top;
            element.scrollLeft = left;
        }
        return result;
    };

    // heights of the scrolling boxes and leaf elements
    const contentHeights = (page) => {
        const view = page.ownerDocument.defaultView;
        const heights = [];
        for (const el of page.querySelectorAll("*")) {
            if ((el === page.ownerDocument.body) || ((el.firstElementChild !== null) && !/auto|scroll/.test(view.getComputedStyle(el).overflowY)))
                continue;

            heights.push(el.getBoundingClientRect().height);
        }
        return heights;
    };

    const pageBottomPadding = (instance) => {
        const content = (instance.iframeEl !== undefined) ? null : instance.contentEl;
        if ((content === null) || (content.scrollHeight <= content.clientHeight))
            return 0;

        const padding = Number.parseFloat(getComputedStyle(content).paddingBottom);
        return Number.isNaN(padding) ? 0 : padding;
    };

    // whether the page stretches to any height it gets, as a list or field that fills the rest does
    const pageFills = (instance, width, tallHeight) => {
        if (instance.responsiveFills === undefined) {
            const short = measurePage(instance, width, 1, contentHeights);
            if (short === null)
                return null;

            const tall = measurePage(instance, width, tallHeight, contentHeights);
            instance.responsiveFills = tall.some((height, i) => Math.abs(height - short[i]) > 1);
        }
        return instance.responsiveFills;
    };

    // 0 for a page that stretches, null while it loads
    const pageHeight = (instance, width, tallHeight) => {
        const fills = pageFills(instance, width, tallHeight);
        if (fills === null)
            return null;
        if (fills)
            return 0;

        return measurePage(instance, width, 1, (page) => page.scrollHeight + pageBottomPadding(instance));
    };

    // the size a page needs not to scroll, 0 where it fits
    const pageOverflow = (instance, width, height) => measurePage(instance, width, height, (page) => ({
        width: (page.scrollWidth > page.clientWidth) ? page.scrollWidth : 0,
        height: (page.scrollHeight > page.clientHeight) ? page.scrollHeight : 0
    })) ?? { width: 0, height: 0 };

    // the width of a vertical scrollbar that would make the page scroll sideways, else 0
    const sidewaysScrollbarWidth = (instance, width, height) => measurePage(instance, width, height, (page) => {
        const fullWidth = page.clientWidth;
        page.style.overflowY = "scroll";
        return (page.scrollWidth > page.clientWidth) ? (fullWidth - page.clientWidth) : 0;
    }) ?? 0;

    const roomOnScreen = (instance) => {
        const wrapper = instance.contentWrapperEl;
        const margin = queries.phone.matches ? PHONE_WINDOW_MARGIN : WINDOW_MARGIN;
        return {
            width: root.clientWidth - (2 * margin) - (instance.windowEl.offsetWidth - wrapper.offsetWidth),
            height: root.clientHeight - (2 * margin) - (instance.windowEl.offsetHeight - wrapper.offsetHeight)
        };
    };

    // a resize can't make a window smaller than its page needs; measured as the drag starts
    const limitResize = (instance) => {
        const wrapper = instance.contentWrapperEl;
        const room = roomOnScreen(instance);
        const ownHeight = pageHeight(instance, wrapper.offsetWidth, room.height) ?? 0;
        instance.responsivePageFits = (ownHeight > 0) && (ownHeight <= Math.min(room.height, MAX_WINDOW_HEIGHT));
        const page = {
            width: Math.min(room.width, Math.max(MIN_WINDOW_WIDTH, pageOverflow(instance, MIN_WINDOW_WIDTH, wrapper.offsetHeight).width)),
            height: Math.min(room.height, (instance.responsivePageFits ? ownHeight : MIN_WINDOW_HEIGHT))
        };
        const frameWidth = instance.windowEl.offsetWidth - wrapper.offsetWidth;
        const frameHeight = instance.windowEl.offsetHeight - wrapper.offsetHeight;
        // the limits the window was opened with
        const own = instance.responsiveResizeLimit ??= { x: [...instance.options.resizeLimit.x], y: [...instance.options.resizeLimit.y] };
        const min = {
            width: Math.min(wrapper.offsetWidth, Math.max(page.width, own.x[0] - frameWidth)),
            height: Math.min(wrapper.offsetHeight, Math.max(page.height, own.y[0] - frameHeight))
        };
        instance.options.resizeLimit = { x: [min.width + frameWidth, own.x[1]], y: [min.height + frameHeight, own.y[1]] };
        for (const drag of [instance.resizable2, instance.resizable3, instance.resizable4]) {
            if (drag?.options.limit.x !== undefined)
                drag.options.limit.x[0] = min.width;
            if (drag?.options.limit.y !== undefined)
                drag.options.limit.y[0] = min.height;
        }
    };

    // a page shown whole needs more height as a drag makes it narrower
    const keepPageHeight = (instance) => {
        if (!instance.responsivePageFits)
            return;

        const wrapper = instance.contentWrapperEl;
        const height = measurePage(instance, wrapper.offsetWidth, 1, (page) => page.scrollHeight + pageBottomPadding(instance)) ?? 0;
        const minHeight = Math.min(roomOnScreen(instance).height, height);
        if (wrapper.offsetHeight >= minHeight)
            return;

        wrapper.style.height = `${minHeight}px`;
        instance.drawWindow();
    };

    // the name saveWindowSize() stores the size under: Add Torrent's id has a suffix
    const storageName = (instance) => instance.options.id.split("-", 1)[0];

    const hasStoredSize = (instance) => {
        const name = storageName(instance);
        return (localPreferences.get(`window_${name}_width`) !== null) || (localPreferences.get(`window_${name}_height`) !== null);
    };

    // sizes stored before windows had a minimum size can be too small
    const raiseStoredSize = (instance) => {
        if (!hasStoredSize(instance))
            return;

        const wrapper = instance.contentWrapperEl;
        const room = roomOnScreen(instance);
        const width = Math.max(wrapper.offsetWidth, Math.min(room.width, MIN_WINDOW_WIDTH));
        const height = Math.max(wrapper.offsetHeight, Math.min(room.height, MIN_WINDOW_HEIGHT));
        if ((width !== wrapper.offsetWidth) || (height !== wrapper.offsetHeight))
            setContentSize(instance, `${width}px`, `${height}px`);
    };

    // on the desktop, dialogs are fitted to their page
    const isDialog = (instance) => !ONE_SIZE_WINDOWS.has(instance.windowEl.id)
        && ((instance.iframeEl !== undefined) || !instance.options.resizable || (instance.contentEl.querySelector(".dialogFooter") !== null));

    // what a window's fit depends on
    const fitState = (instance) => {
        const sizes = [root.clientWidth, root.clientHeight, instance.contentWrapperEl.style.width, instance.contentWrapperEl.style.height];
        const page = ONE_SIZE_WINDOWS.has(instance.windowEl.id) ? [] : [...instance.contentEl.children];
        for (const element of [...page, instance.iframeEl?.contentDocument?.body ?? null, instance.toolbarWrapperEl ?? null]) {
            if (element !== null)
                sizes.push(element.offsetWidth, element.offsetHeight);
        }
        return sizes.join();
    };

    const fitWindow = (instance) => {
        const windowEl = instance.windowEl;
        if (!windowEl.isConnected || (windowEl.style.display === "none") || instance.isMaximized)
            return;

        const phone = queries.phone.matches;
        const { fullScreen, fitted } = fitWindowSize(instance, phone);

        // no dragging on phones
        if (phone)
            instance.windowDrag?.detach();
        else
            instance.windowDrag?.attach();

        const margin = fullScreen ? 0 : (phone ? PHONE_WINDOW_MARGIN : WINDOW_MARGIN);
        const wasFullScreen = windowEl.classList.contains("responsiveFullScreen");
        windowEl.classList.toggle("responsiveFullScreen", fullScreen);
        const maxLeft = root.clientWidth - windowEl.offsetWidth - margin;
        const maxTop = root.clientHeight - windowEl.offsetHeight - margin;
        if (fullScreen) {
            windowEl.style.left = "0px";
            windowEl.style.top = "0px";
        }
        // Mocha centers a window at its opening size: recenter fitted windows until the user moves them
        else if (phone || wasFullScreen || ((queries.relayout.matches || fitted) && (instance.responsiveUserMoved !== true))) {
            windowEl.style.left = `${Math.max(margin, Math.round((maxLeft + margin) / 2))}px`;
            windowEl.style.top = `${Math.max(margin, Math.round((maxTop + margin) / 2))}px`;
        }
        else {
            windowEl.style.left = `${Math.max(margin, Math.min(windowEl.offsetLeft, maxLeft))}px`;
            windowEl.style.top = `${Math.max(margin, Math.min(windowEl.offsetTop, maxTop))}px`;
        }
        instance.responsiveFitState = fitState(instance);
    };

    // returns whether the window is full screen and whether it was fitted to its page
    const fitWindowSize = (instance, phone) => {
        const windowEl = instance.windowEl;
        const wrapper = instance.contentWrapperEl;
        const toolbar = instance.toolbarWrapperEl;
        // Mocha draws the window around the toolbar's inline height
        if (queries.relayout.matches && (toolbar !== undefined) && (toolbar.style.height !== `${toolbar.offsetHeight}px`)) {
            toolbar.style.height = `${toolbar.offsetHeight}px`;
            instance.drawWindow();
        }
        const wanted = wantedSize(instance);
        // title bar, toolbar and borders
        const frameWidth = windowEl.offsetWidth - wrapper.offsetWidth;
        const frameHeight = windowEl.offsetHeight - wrapper.offsetHeight;
        wrapper.style.width = wanted.width;
        wrapper.style.height = wanted.height;
        const wantedWidth = wrapper.offsetWidth;
        const wantedHeight = wrapper.offsetHeight;
        const margin = phone ? PHONE_WINDOW_MARGIN : WINDOW_MARGIN;
        const maxWidth = root.clientWidth - (2 * margin) - frameWidth;
        const maxHeight = root.clientHeight - (2 * margin) - frameHeight;
        const fitWidth = Math.min(maxWidth, MAX_WINDOW_WIDTH);
        const fitHeight = Math.min(maxHeight, MAX_WINDOW_HEIGHT);

        let width = phone ? maxWidth : Math.min(maxWidth, wantedWidth);
        let height = Math.min(maxHeight, wantedHeight);
        let fitted = false;
        let ownHeight = null;
        // a size the user gave a window is kept, except on phones
        const userSized = (instance.responsiveUserResized === true) || hasStoredSize(instance);
        if (queries.relayout.matches && (phone || !userSized)) {
            width = phone ? maxWidth : Math.min(fitWidth, Math.max(width, MIN_WINDOW_WIDTH, pageOverflow(instance, width, wantedHeight).width));
            if (ONE_SIZE_WINDOWS.has(windowEl.id)) {
                ownHeight = (pageOf(instance) === null) ? null : 0;
                height = fitHeight;
            }
            else {
                ownHeight = pageHeight(instance, width, maxHeight);
                height = Math.min(fitHeight, (ownHeight > 0) ? ownHeight : Math.max(wantedHeight, pageOverflow(instance, width, wantedHeight).height));
            }
            // room for the scrollbar
            if (pageOverflow(instance, width, height).height > 0)
                width = Math.min(fitWidth, width + sidewaysScrollbarWidth(instance, width, height));
            fitted = true;
        }
        // on the desktop, a dialog takes its page's height where it fits
        else if (!queries.relayout.matches && !userSized && isDialog(instance)) {
            const pageNeeds = pageHeight(instance, width, maxHeight);
            if ((pageNeeds > 0) && (pageNeeds <= fitHeight)) {
                height = pageNeeds;
                fitted = true;
            }
        }
        // a phone shows a page that fits as a card, else full screen
        const fullScreen = phone && (ownHeight !== null) && ((ownHeight === 0) || (ownHeight > fitHeight));
        if (fullScreen) {
            width = root.clientWidth - frameWidth;
            height = root.clientHeight - frameHeight;
        }

        // restore, so the window is only redrawn on a change
        wrapper.style.width = instance.responsiveFittedSize?.width ?? wanted.width;
        wrapper.style.height = instance.responsiveFittedSize?.height ?? wanted.height;
        setContentSize(instance, `${width}px`, `${height}px`);
        instance.responsiveFittedSize = { width: `${width}px`, height: `${height}px` };
        return { fullScreen: fullScreen, fitted: fitted };
    };

    const fitWindows = () => {
        for (const instance of Object.values(MochaUI.Windows.instances))
            fitWindow(instance);
    };

    // on phones and touch screens, where Back is how a page is left, Back closes what shows on top of the page
    const backCloses = () => queries.phone.matches || queries.coarse.matches;

    // a window there gains that as it opens, or as the screen turns into such a screen, and loses it as it turns back
    const addWindowLayer = (instance) => {
        if ((instance.responsiveLayer === undefined) && !instance.isClosing)
            instance.responsiveLayer = addHistoryLayer(() => instance.close());
    };

    const removeWindowLayer = (instance) => {
        if (instance.responsiveLayer === undefined)
            return;

        removeHistoryLayer(instance.responsiveLayer);
        instance.responsiveLayer = undefined;
    };

    // and a context menu there, while it shows; contextmenu.js tells as it shows and hides one
    let contextMenuLayer = null;

    const contextMenuShown = (contextMenu) => {
        if ((contextMenuLayer === null) && backCloses())
            contextMenuLayer = addHistoryLayer(() => contextMenu.hide());
    };

    const contextMenuHidden = () => {
        if (contextMenuLayer === null)
            return;

        const layer = contextMenuLayer;
        contextMenuLayer = null;
        removeHistoryLayer(layer);
    };

    // fit a window as it opens, and again as its page or the screen changes; and on phones and touch screens let Back
    // close it
    const watch = (node) => {
        if (node.classList?.contains("mocha") !== true)
            return;

        const instance = MochaUI.Windows.instances[node.id];
        if (instance === undefined)
            return;

        // a window opened from the drawer would open behind it
        closeFiltersDrawer();
        raiseStoredSize(instance);
        fitWindow(instance);
        if (backCloses())
            addWindowLayer(instance);
        instance.addEvent("closeComplete", () => removeWindowLayer(instance));

        // refit only when something the fit depends on has changed
        const fitSoon = window.qBittorrent.Misc.createDebounceHandler(50, () => {
            if (fitState(instance) !== instance.responsiveFitState)
                fitWindow(instance);
        });
        const sizeObserver = new ResizeObserver(fitSoon);
        const pageObserver = new MutationObserver((mutations) => {
            for (const mutation of mutations) {
                for (const added of mutation.addedNodes) {
                    if (added.nodeType === Node.ELEMENT_NODE)
                        sizeObserver.observe(added);
                }
                for (const removed of mutation.removedNodes) {
                    if (removed.nodeType === Node.ELEMENT_NODE)
                        sizeObserver.unobserve(removed);
                }
            }
            instance.responsiveFitState = null;
            instance.responsiveFills = undefined;
            fitSoon();
        });
        for (const child of instance.contentEl.children)
            sizeObserver.observe(child);
        pageObserver.observe(instance.contentEl, { childList: true });
        if (instance.toolbarWrapperEl !== undefined)
            sizeObserver.observe(instance.toolbarWrapperEl);
        // not creation options: windows come here once created elsewhere
        instance.addEvent("closeComplete", () => {
            sizeObserver.disconnect();
            pageObserver.disconnect();
        });

        // keep a size or place the user chose, from the start of the drag
        instance.addEvent("resize", () => {
            instance.responsiveUserResized = true;
        });
        // not creation options: MochaUI has none for its drags
        for (const drag of [instance.resizable1, instance.resizable2, instance.resizable3, instance.resizable4, instance.resizable5]) {
            drag?.addEvent("start", () => {
                instance.responsiveUserMoved = true;
                // Mocha signals the resize only once the drag ends
                instance.responsiveUserResized = true;
            });
            drag?.addEvent("beforeStart", () => limitResize(instance));
        }
        // the edges that change the width
        for (const drag of [instance.resizable2, instance.resizable3, instance.resizable5])
            drag?.addEvent("drag", () => keepPageHeight(instance));
        instance.windowDrag?.addEvent("start", () => {
            instance.responsiveUserMoved = true;
        });

        // framed dialogs
        const iframe = instance.iframeEl;
        iframe?.addEventListener("load", (event) => {
            instance.responsiveFills = undefined;
            fitWindow(instance);
            new iframe.contentWindow.ResizeObserver(fitSoon).observe(iframe.contentDocument.body);
        });
    };

    const initWindows = () => {
        const windowObserver = new MutationObserver((mutations) => {
            for (const mutation of mutations) {
                for (const node of mutation.addedNodes)
                    watch(node);
            }
        });
        for (const container of new Set([document.getElementById("desktop"), document.body]))
            windowObserver.observe(container, { childList: true });
        for (const node of document.querySelectorAll(".mocha"))
            watch(node);
        const updateWindowLayers = () => {
            for (const node of document.querySelectorAll(".mocha")) {
                const instance = MochaUI.Windows.instances[node.id];
                if (instance === undefined)
                    continue;

                if (backCloses())
                    addWindowLayer(instance);
                else
                    removeWindowLayer(instance);
            }
        };
        queries.phone.addEventListener("change", (_event) => updateWindowLayers());
        queries.coarse.addEventListener("change", (_event) => updateWindowLayers());

        // after Mocha's own resize handling
        window.addEventListener("resize", window.qBittorrent.Misc.createDebounceHandler(50, fitWindows));
    };

    // phones show only the view tabs' icons: name them on hover
    const initViewTabs = () => {
        const name = () => {
            for (const img of document.querySelectorAll("#mainWindowTabsList img")) {
                if (queries.phone.matches)
                    img.parentElement.title = img.alt;
                else
                    img.parentElement.removeAttribute("title");
            }
        };
        queries.phone.addEventListener("change", (_event) => name());
        name();
    };

    // a little longer than the transition in responsive.css
    const SLIDE_TIME = 250;

    // on small screens the header slides away once the first row scrolls out of view
    const initHeaderAutoHide = async () => {
        const header = document.getElementById("desktopHeader");
        const list = await whenElement("torrentsTableDiv");
        let lockedUntil = 0;
        let slideTimer = -1;
        let slideFrame = 0;

        // the list and the panels below the header follow its height while it slides, rather than catching up at the
        // end: sliding in, the header would otherwise push them under the status bar
        const followSlide = () => {
            relayout();
            if (header.classList.contains("sliding"))
                slideFrame = requestAnimationFrame(followSlide);
        };

        const endSlide = () => {
            clearTimeout(slideTimer);
            cancelAnimationFrame(slideFrame);
            header.classList.remove("sliding");
            header.style.removeProperty("height");
            relayout();
            keptPropertiesHeight = 0;
            // ignore the scroll the relayout causes
            lockedUntil = performance.now() + SLIDE_TIME;
        };

        const setHidden = (hidden) => {
            if (root.classList.contains("headerHidden") === hidden)
                return;

            clearTimeout(slideTimer);
            // the height before the first of overlapping slides
            if (keptPropertiesHeight === 0)
                keptPropertiesHeight = document.getElementById("propertiesPanel").offsetHeight;
            header.classList.add("sliding");
            // slide between fixed heights, as auto can't be animated
            header.style.height = `${header.offsetHeight}px`;
            void header.offsetHeight;
            root.classList.toggle("headerHidden", hidden);
            if (!hidden)
                header.style.height = `${header.scrollHeight}px`;
            if (getComputedStyle(header).transitionDuration === "0s") {
                endSlide();
            }
            else {
                slideTimer = setTimeout(endSlide, SLIDE_TIME);
                cancelAnimationFrame(slideFrame);
                slideFrame = requestAnimationFrame(followSlide);
            }
            // ignore the scrolls of the slide
            lockedUntil = performance.now() + SLIDE_TIME;
        };

        const busy = () => header.contains(document.activeElement) && document.activeElement.matches("input, select");

        // capturing: read the sizes before the list renders its rows
        list.parentElement.addEventListener("scroll", (event) => {
            if ((event.target !== list) || !queries.small.matches || (performance.now() < lockedUntil))
                return;

            if (list.scrollTop <= window.torrentsTable.rowHeight)
                setHidden(false);
            else if (!root.classList.contains("headerHidden") && !busy() && ((list.scrollHeight - list.clientHeight) > (header.offsetHeight + 24)))
                setHidden(true);
        }, { capture: true, passive: true });
        queries.small.addEventListener("change", (_event) => {
            if (!queries.small.matches)
                setHidden(false);
        });
        header.addEventListener("focusin", (event) => setHidden(false));
    };

    // the card's checkbox zone at its left, its menu zone at its top right, or null
    const cardZone = (event) => {
        const tr = queries.phone.matches ? event.target.closest?.("#torrentsTableDiv tbody tr") : null;
        if ((tr === undefined) || (tr === null))
            return null;

        const point = event.changedTouches?.[0] ?? event;
        const box = tr.getBoundingClientRect();
        if (point.clientX < (box.left + cssLength("--responsive-card-check-width")))
            return "check";
        if ((point.clientX >= (box.right - cssLength("--responsive-card-menu-width"))) && (point.clientY <= (box.top + cssLength("--responsive-card-menu-height"))))
            return "menu";
        return null;
    };

    // each cell is tagged with its column for the card layout
    const initCards = async () => {
        const tableDiv = await whenElement("torrentsTableDiv");
        const table = window.torrentsTable;
        const tag = (rows, force = false) => {
            if (!queries.phone.matches)
                return;

            for (const tr of rows) {
                if (!force && (tr.dataset.cardCells === "1"))
                    continue;

                for (const [i, td] of [...tr.children].entries()) {
                    td.dataset.col = table.columns[i].name;
                    td.dataset.label = table.columns[i].caption;
                }
                tr.dataset.cardCells = "1";
            }
        };

        new MutationObserver((mutations) => {
            tag(mutations.flatMap((mutation) => [...mutation.addedNodes]));
        }).observe(table.tableBody, { childList: true });
        // rows and cards differ in height
        queries.phone.addEventListener("change", (_event) => {
            keepFirstRow();
            tag(table.tableBody.children, true);
        });
        tag(table.tableBody.children, true);

        // the menu zone opens the torrent's menu, as a long press does
        tableDiv.addEventListener("click", (event) => {
            if (cardZone(event) !== "menu")
                return;

            event.preventDefault();
            event.stopPropagation();
            event.target.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, button: 2, clientX: event.clientX, clientY: event.clientY }));
        }, true);
        // a double click on a zone doesn't start or stop the torrent
        tableDiv.addEventListener("dblclick", (event) => {
            if (cardZone(event) !== null)
                event.stopPropagation();
        }, true);
    };

    // a card's checkbox toggles its torrent's selection, as a Ctrl+click does
    const initCardCheckboxes = async () => {
        const tableDiv = await whenElement("torrentsTableDiv");
        const table = window.torrentsTable;

        // a touch on the checkbox doesn't select the card alone or start a long press
        tableDiv.addEventListener("touchstart", (event) => {
            if (cardZone(event) === "check")
                event.stopPropagation();
        }, { capture: true, passive: true });
        // dynamicTable.js selects a touched row alone at once, for its long press menu; while cards are selected,
        // a card waits until the finger rests (less than a long press), so a swipe keeps the selection
        let holdTimer = -1;
        table.tableBody.addEventListener("touchstart", (event) => {
            const rowId = event.target.closest("tr")?.rowId;
            if (!queries.phone.matches || (rowId === undefined) || (table.selectedRowsIds().length === 0) || table.isRowSelected(rowId))
                return;

            event.stopPropagation();
            clearTimeout(holdTimer);
            holdTimer = setTimeout(() => {
                table.deselectAll();
                table.selectRow(rowId);
            }, 400);
        }, { passive: true });
        for (const type of ["touchend", "touchcancel"])
            table.tableBody.addEventListener(type, (_event) => clearTimeout(holdTimer), { passive: true });
        tableDiv.addEventListener("scroll", (_event) => clearTimeout(holdTimer), { passive: true });
        tableDiv.addEventListener("click", (event) => {
            if (event.ctrlKey || (cardZone(event) !== "check"))
                return;

            event.stopPropagation();
            event.target.dispatchEvent(new MouseEvent("click", { bubbles: true, cancelable: true, ctrlKey: true, clientX: event.clientX, clientY: event.clientY }));
        }, true);

        const selectAll = document.getElementById("torrentsSelectAll");
        const count = document.getElementById("torrentsSelectionCount");
        const update = () => {
            if (!queries.phone.matches)
                return;

            const selected = table.selectedRowsIds().length;
            const all = (selected > 0) ? table.filteredRowCount : 0;
            selectAll.checked = (selected > 0) && (selected >= all);
            selectAll.indeterminate = (selected > 0) && (selected < all);
            // a live region, which may be read out again when its text is rewritten
            const [before, after = ""] = count.dataset.format.split("%1");
            const text = (selected > 0) ? `${before}${selected}${after}` : "";
            if (count.textContent !== text) {
                // narrow phones show only the number, and the words around it only to screen readers
                const words = [before, after].map((part) => {
                    const span = document.createElement("span");
                    span.textContent = part;
                    return span;
                });
                count.replaceChildren(...((selected > 0) ? [words[0], selected.toString(), words[1]] : []));
            }
            for (const tr of table.tableBody.children)
                tr.setAttribute("aria-selected", tr.classList.contains("selected"));
        };
        selectAll.addEventListener("click", (event) => {
            if (selectAll.checked) {
                table.selectAll();
            }
            else {
                table.deselectAll();
                table.setRowClass();
            }
            update();
        });
        const updateSoon = window.qBittorrent.Misc.createDebounceHandler(100, update);
        new MutationObserver(updateSoon).observe(table.tableBody, { childList: true, subtree: true, attributes: true, attributeFilter: ["class"] });
        queries.phone.addEventListener("change", (_event) => update());
        update();
    };

    // the bar above the cards sorts them and opens the columns menu
    const initCardSorting = async () => {
        await whenElement("torrentsTableDiv");
        const table = window.torrentsTable;
        const select = document.getElementById("torrentsSortSelect");
        const order = document.getElementById("torrentsSortOrder");
        const update = () => {
            if (!queries.phone.matches)
                return;

            const options = table.columns.filter((column) => column.caption !== "").map((column) => new Option(column.caption, column.name));
            select.replaceChildren(...options);
            select.value = table.columns[table.sortedColumn]?.name ?? "";
            order.setAttribute("aria-pressed", (table.reverseSort === "1"));
        };
        select.addEventListener("change", (event) => {
            table.setSortedColumn(select.value, table.reverseSort);
            update();
        });
        order.addEventListener("click", (event) => {
            table.setSortedColumn(table.sortedColumn);
            update();
        });
        // the header's menu, below the button
        document.getElementById("torrentsColumnsButton").addEventListener("click", (event) => {
            event.stopPropagation();
            const box = event.currentTarget.getBoundingClientRect();
            table.fixedTableHeader.firstElementChild.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, button: 2, clientX: box.left, clientY: box.bottom }));
        });
        queries.phone.addEventListener("change", (_event) => update());
        update();
    };

    // "Add Torrent File..." is a label in a div, not a link
    const menuItems = "#desktopNavbar a, #uploadLink";

    let menuDrawerScrim = null;
    let releaseMenuDrawerFocus = null;
    let menuDrawerLayer = null;

    const openMenuDrawer = () => {
        if (!queries.phone.matches || root.classList.contains("menuDrawerOpen"))
            return;

        const navbar = document.getElementById("desktopNavbar");
        root.classList.add("menuDrawerOpen");
        menuDrawerScrim = addScrim(closeMenuDrawer);
        navbar.setAttribute("role", "dialog");
        navbar.setAttribute("aria-modal", "true");
        navbar.setAttribute("aria-labelledby", "menuDrawerTitle");
        document.getElementById("menuDrawerButton").setAttribute("aria-expanded", "true");
        setKeyboardItems(navbar, menuItems, true);
        releaseMenuDrawerFocus = trapFocus(navbar);
        menuDrawerLayer = addHistoryLayer(closeMenuDrawer);
    };

    const closeMenuDrawer = () => {
        if (!root.classList.contains("menuDrawerOpen"))
            return;

        const navbar = document.getElementById("desktopNavbar");
        root.classList.remove("menuDrawerOpen");
        for (const li of navbar.querySelectorAll(":scope > ul > li.menuDrawerExpanded"))
            li.classList.remove("menuDrawerExpanded");
        menuDrawerScrim.remove();
        menuDrawerScrim = null;
        navbar.removeAttribute("role");
        navbar.removeAttribute("aria-modal");
        navbar.removeAttribute("aria-labelledby");
        document.getElementById("menuDrawerButton").setAttribute("aria-expanded", "false");
        setKeyboardItems(navbar, menuItems, false);
        releaseMenuDrawerFocus();
        releaseMenuDrawerFocus = null;
        removeHistoryLayer(menuDrawerLayer);
        menuDrawerLayer = null;
        drawerClosed = true;
    };

    const initMenuDrawer = () => {
        const navbar = document.getElementById("desktopNavbar");
        document.getElementById("menuDrawerButton").addEventListener("click", (_event) => openMenuDrawer());
        document.getElementById("menuDrawerClose").addEventListener("click", (_event) => closeMenuDrawer());
        // a menu's name opens its menu and closes any other
        for (const li of navbar.querySelectorAll(":scope > ul > li")) {
            li.firstElementChild.addEventListener("click", (event) => {
                if (!root.classList.contains("menuDrawerOpen"))
                    return;

                const open = !li.classList.contains("menuDrawerExpanded");
                for (const other of navbar.querySelectorAll(":scope > ul > li.menuDrawerExpanded"))
                    other.classList.remove("menuDrawerExpanded");
                li.classList.toggle("menuDrawerExpanded", open);
            });
        }
        // choosing an item closes the drawer; capturing, as the items' handlers stop the click
        navbar.addEventListener("click", (event) => {
            if (event.target.closest("li li") !== null)
                closeMenuDrawer();
        }, true);
        chooseWithKeys(navbar, menuItems);
        queries.phone.addEventListener("change", (_event) => {
            if (!queries.phone.matches)
                closeMenuDrawer();
        });
        document.addEventListener("keydown", (event) => {
            switch (event.key) {
                case "Escape":
                    // close the drawer, not also the window behind it
                    if (root.classList.contains("menuDrawerOpen"))
                        event.stopPropagation();
                    closeMenuDrawer();
                    break;
            }
        });
    };

    // small screens start with the properties panel collapsed; larger ones undo a collapse that only this made
    const initPropertiesPanel = async () => {
        const collapsed = localPreferences.get("properties_panel_collapsed");
        if (queries.small.matches) {
            if (collapsed === null) {
                localPreferences.set("properties_panel_collapsed", "true");
                localPreferences.set("properties_panel_collapsed_by_layout", "true");
            }
        }
        else if (localPreferences.get("properties_panel_collapsed_by_layout") !== null) {
            localPreferences.remove("properties_panel_collapsed_by_layout");
            if (collapsed === "true")
                localPreferences.remove("properties_panel_collapsed");
        }
        if (localPreferences.get("properties_panel_collapsed_by_layout") === null)
            return;

        // once the user's own click expands or collapses the panel, the stored state is theirs
        const header = await whenElement("propertiesPanel_header");
        let stored = null;
        header.addEventListener("click", (event) => {
            if (event.isTrusted)
                stored = localPreferences.get("properties_panel_collapsed");
        }, true);
        header.addEventListener("click", (event) => {
            if (event.isTrusted && (localPreferences.get("properties_panel_collapsed") !== stored))
                localPreferences.remove("properties_panel_collapsed_by_layout");
        });
    };

    // called by client.js once the main window is built
    const init = () => {
        initLayout();
        initFiltersDrawer();
        initTouch();
        initWindows();
        initViewTabs();
        initHeaderAutoHide();
        initCards();
        initCardCheckboxes();
        initCardSorting();
        initMenuDrawer();
        initPropertiesPanel();
        initHistoryLayers();
    };

    return exports();
})();
Object.freeze(window.qBittorrent.Responsive);
