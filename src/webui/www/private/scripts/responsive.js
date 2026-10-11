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
            rowHeight: rowHeight
        };
    };

    // the same breakpoints as responsive.css
    const queries = Object.freeze({
        phone: window.matchMedia("(width < 760px)"),
        drawer: window.matchMedia("(width < 1100px)"),
        relayout: window.matchMedia("(pointer: coarse), (width < 1100px)"),
        small: window.matchMedia("(width < 760px), (pointer: coarse) and (height < 620px)"),
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

    // Mocha relayouts only when the browser window is resized
    const relayout = () => {
        MochaUI.Desktop.setDesktopSize();
        if ((restoreProperties !== null) ? restorePropertiesPanel() : fitPropertiesPanel())
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

    const filterItems = "ul.filterList span.link";

    let filtersScrim = null;
    let filtersObserver = null;
    let releaseFiltersFocus = null;
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

        const tableDiv = await whenElement("torrentsTableDiv");
        shownRowHeight = measureRowHeight(window.torrentsTable);
        queries.coarse.addEventListener("change", (_event) => keepFirstRow());
        new ResizeObserver(scrollToFirstRow).observe(tableDiv);
    };

    const WINDOW_MARGIN = 8;
    const PHONE_WINDOW_MARGIN = 12;

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

    const roomOnScreen = (instance) => {
        const wrapper = instance.contentWrapperEl;
        const margin = queries.phone.matches ? PHONE_WINDOW_MARGIN : WINDOW_MARGIN;
        return {
            width: root.clientWidth - (2 * margin) - (instance.windowEl.offsetWidth - wrapper.offsetWidth),
            height: root.clientHeight - (2 * margin) - (instance.windowEl.offsetHeight - wrapper.offsetHeight)
        };
    };

    // what a window's fit depends on
    const fitState = (instance) => {
        const sizes = [root.clientWidth, root.clientHeight, instance.contentWrapperEl.style.width, instance.contentWrapperEl.style.height];
        const page = [...instance.contentEl.children];
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
        const fullScreen = fitWindowSize(instance, phone);

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
        else if (phone || wasFullScreen || (queries.relayout.matches && (instance.responsiveUserMoved !== true))) {
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

        let width = phone ? maxWidth : Math.min(maxWidth, wantedWidth);
        let height = Math.min(maxHeight, wantedHeight);
        const fullScreen = phone;
        if (fullScreen) {
            width = root.clientWidth - frameWidth;
            height = root.clientHeight - frameHeight;
        }

        // restore, so the window is only redrawn on a change
        wrapper.style.width = instance.responsiveFittedSize?.width ?? wanted.width;
        wrapper.style.height = instance.responsiveFittedSize?.height ?? wanted.height;
        setContentSize(instance, `${width}px`, `${height}px`);
        instance.responsiveFittedSize = { width: `${width}px`, height: `${height}px` };
        return fullScreen;
    };

    const fitWindows = () => {
        for (const instance of Object.values(MochaUI.Windows.instances))
            fitWindow(instance);
    };

    // fit a window as it opens, and again as its page or the screen changes
    const watch = (node) => {
        if (node.classList?.contains("mocha") !== true)
            return;

        const instance = MochaUI.Windows.instances[node.id];
        if (instance === undefined)
            return;

        // a window opened from the drawer would open behind it
        closeFiltersDrawer();
        fitWindow(instance);

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
        }
        instance.windowDrag?.addEvent("start", () => {
            instance.responsiveUserMoved = true;
        });

        // framed dialogs
        const iframe = instance.iframeEl;
        iframe?.addEventListener("load", (event) => {
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

        // after Mocha's own resize handling
        window.addEventListener("resize", window.qBittorrent.Misc.createDebounceHandler(50, fitWindows));
    };

    // called by client.js once the main window is built
    const init = () => {
        initLayout();
        initFiltersDrawer();
        initTouch();
        initWindows();
    };

    return exports();
})();
Object.freeze(window.qBittorrent.Responsive);
