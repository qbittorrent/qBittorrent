/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2009  Christophe Dumez <chris@qbittorrent.org>
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
window.qBittorrent.ContextMenu ??= (() => {
    const exports = () => {
        return {
            ContextMenu: ContextMenu,
            TorrentsTableContextMenu: TorrentsTableContextMenu,
            StatusesFilterContextMenu: StatusesFilterContextMenu,
            CategoriesFilterContextMenu: CategoriesFilterContextMenu,
            TagsFilterContextMenu: TagsFilterContextMenu,
            TrackersFilterContextMenu: TrackersFilterContextMenu,
            SearchPluginsTableContextMenu: SearchPluginsTableContextMenu,
            RssFeedContextMenu: RssFeedContextMenu,
            RssArticleContextMenu: RssArticleContextMenu,
            RssDownloaderRuleContextMenu: RssDownloaderRuleContextMenu
        };
    };

    let lastShownContextMenu = null;
    class ContextMenu {
        constructor(options) {
            this.options = {
                actions: {},
                menu: "menu_id",
                stopEvent: true,
                targets: "body",
                offsets: {
                    x: 0,
                    y: 0
                },
                onShow: () => {},
                onHide: () => {},
                onClick: () => {},
                fadeSpeed: 200,
                touchTimer: 600,
                ...options
            };

            // option diffs menu
            this.menu = $(this.options.menu);

            // fx
            this.fx = new Fx.Tween(this.menu, {
                property: "opacity",
                duration: this.options.fadeSpeed,
                onComplete: () => {
                    this.menu.style.visibility = (getComputedStyle(this.menu).opacity > 0) ? "visible" : "hidden";
                }
            });

            // hide and begin the listener
            this.hide().startListener();

            // hide the menu
            this.menu.style.position = "absolute";
            this.menu.style.top = "-900000px";
            this.menu.style.display = "block";
        }

        adjustMenuPosition(e) {
            this.updateMenuItems();

            const scrollableMenuMaxHeight = document.documentElement.clientHeight * 0.75;

            if (this.menu.classList.contains("scrollableMenu"))
                this.menu.style.maxHeight = `${scrollableMenuMaxHeight}px`;

            // draw the menu off-screen to know the menu dimensions
            this.menu.style.left = "-999em";
            this.menu.style.top = "-999em";

            // position the menu
            let xPosMenu = e.pageX + this.options.offsets.x;
            let yPosMenu = e.pageY + this.options.offsets.y;
            if ((xPosMenu + this.menu.offsetWidth) > document.documentElement.clientWidth)
                xPosMenu -= this.menu.offsetWidth;
            if ((yPosMenu + this.menu.offsetHeight) > document.documentElement.clientHeight)
                yPosMenu = document.documentElement.clientHeight - this.menu.offsetHeight;
            if (xPosMenu < 0)
                xPosMenu = 0;
            if (yPosMenu < 0)
                yPosMenu = 0;
            this.menu.style.left = `${xPosMenu}px`;
            this.menu.style.top = `${yPosMenu}px`;
            this.menu.style.position = "absolute";
            this.menu.style.zIndex = "2000";

            // position the sub-menu
            const uls = this.menu.getElementsByTagName("ul");
            for (let i = 0; i < uls.length; ++i) {
                const ul = uls[i];
                if (ul.classList.contains("scrollableMenu"))
                    ul.style.maxHeight = `${scrollableMenuMaxHeight}px`;
                const rectParent = ul.parentNode.getBoundingClientRect();
                const xPosOrigin = rectParent.left;
                const yPosOrigin = rectParent.bottom;
                let xPos = xPosOrigin + rectParent.width - 1;
                let yPos = yPosOrigin - rectParent.height - 1;
                if ((xPos + ul.offsetWidth) > document.documentElement.clientWidth)
                    xPos -= (ul.offsetWidth + rectParent.width - 2);
                if ((yPos + ul.offsetHeight) > document.documentElement.clientHeight)
                    yPos = document.documentElement.clientHeight - ul.offsetHeight;
                if (xPos < 0)
                    xPos = 0;
                if (yPos < 0)
                    yPos = 0;
                ul.style.marginLeft = `${xPos - xPosOrigin}px`;
                ul.style.marginTop = `${yPos - yPosOrigin}px`;
            }
        }

        setupEventListeners(elem) {
            elem.addEventListener("contextmenu", (e) => {
                this.triggerMenu(e, elem);
            });
            elem.addEventListener("click", (e) => {
                this.hide();
            });

            elem.addEventListener("touchstart", (e) => {
                this.hide();
                this.touchStartAt = performance.now();
                this.touchStartEvent = e;
            }, { passive: true });
            elem.addEventListener("touchend", (e) => {
                const now = performance.now();
                const touchStartAt = this.touchStartAt;
                const touchStartEvent = this.touchStartEvent;

                this.touchStartAt = null;
                this.touchStartEvent = null;

                const isTargetUnchanged = (Math.abs(e.changedTouches[0].pageX - touchStartEvent.changedTouches[0].pageX) <= 10) && (Math.abs(e.changedTouches[0].pageY - touchStartEvent.changedTouches[0].pageY) <= 10);
                if (((now - touchStartAt) >= this.options.touchTimer) && isTargetUnchanged)
                    this.triggerMenu(touchStartEvent, elem);
            }, { passive: true });
        }

        addTarget(t) {
            if (t.hasEventListeners)
                return;

            // prevent long press from selecting this text
            t.style.userSelect = "none";
            t.hasEventListeners = true;
            this.setupEventListeners(t);
        }

        searchAndAddTargets() {
            if (this.options.targets.length > 0)
                document.querySelectorAll(this.options.targets).forEach((target) => { this.addTarget(target); });
        }

        triggerMenu(e, el) {
            if (this.options.disabled)
                return;

            // prevent default, if told to
            if (this.options.stopEvent) {
                e.preventDefault();
                e.stopPropagation();
            }
            // record this as the trigger
            this.options.element = $(el);
            this.adjustMenuPosition(e);
            // show the menu
            this.show();
        }

        // get things started
        startListener() {
            /* all elements */
            this.searchAndAddTargets();

            /* menu items */
            this.menu.addEventListener("click", (e) => {
                const menuItem = e.target.closest("li");
                if (!menuItem)
                    return;

                e.preventDefault();
                if (!menuItem.classList.contains("disabled")) {
                    const anchor = menuItem.firstElementChild;
                    this.execute(anchor.href.split("#")[1], this.options.element);
                    this.options.onClick.call(this, anchor, e);
                }
                else {
                    e.stopPropagation();
                }
            });

            // hide on body click
            $(document.body).addEventListener("click", () => {
                this.hide();
                this.options.element = null;
            });
        }

        updateMenuItems() {}

        // show menu
        show(trigger) {
            if (lastShownContextMenu && (lastShownContextMenu !== this))
                lastShownContextMenu.hide();
            this.fx.start(1);
            this.options.onShow.call(this);
            lastShownContextMenu = this;
            return this;
        }

        // hide the menu
        hide(trigger) {
            if (lastShownContextMenu && (lastShownContextMenu.menu.style.visibility !== "hidden")) {
                this.fx.start(0);
                this.options.onHide.call(this);
            }
            return this;
        }

        setItemChecked(item, checked) {
            this.menu.querySelector(`a[href$="${item}"]`).firstElementChild.style.opacity =
                checked ? "1" : "0";
            return this;
        }

        getItemChecked(item) {
            return this.menu.querySelector(`a[href$="${item}"]`).firstElementChild.style.opacity !== "0";
        }

        // hide an item
        hideItem(item) {
            this.menu.querySelector(`a[href$="${item}"]`).parentNode.classList.add("invisible");
            return this;
        }

        // show an item
        showItem(item) {
            this.menu.querySelector(`a[href$="${item}"]`).parentNode.classList.remove("invisible");
            return this;
        }

        // enable/disable an item
        setEnabled(item, enabled) {
            this.menu.querySelector(`:scope a[href$="${item}"]`).parentElement.classList.toggle("disabled", !enabled);
            return this;
        }

        // disable the entire menu
        disable() {
            this.options.disabled = true;
            return this;
        }

        // enable the entire menu
        enable() {
            this.options.disabled = false;
            return this;
        }

        // execute an action
        execute(action, element) {
            if (this.options.actions[action])
                this.options.actions[action](element, this, action);
            return this;
        }
    };

    class FilterListContextMenu extends ContextMenu {
        constructor(options) {
            super(options);
            this.torrentObserver = new MutationObserver((records, observer) => {
                this.updateTorrentActions();
            });
        }

        startTorrentObserver() {
            this.torrentObserver.observe(torrentsTable.tableBody, { childList: true });
        }

        stopTorrentObserver() {
            this.torrentObserver.disconnect();
        }

        updateTorrentActions() {
            const torrentsVisible = torrentsTable.tableBody.children.length > 0;
            this.setEnabled("startTorrents", torrentsVisible)
                .setEnabled("stopTorrents", torrentsVisible)
                .setEnabled("deleteTorrents", torrentsVisible);
        }
    };

    class TorrentsTableContextMenu extends ContextMenu {
        updateMenuItems() {
            let all_are_seq_dl = true;
            let there_are_seq_dl = false;
            let all_are_f_l_piece_prio = true;
            let there_are_f_l_piece_prio = false;
            let all_are_downloaded = true;
            let all_are_stopped = true;
            let there_are_stopped = false;
            let all_are_force_start = true;
            let there_are_force_start = false;
            let all_are_super_seeding = true;
            let all_are_auto_tmm = true;
            let there_are_auto_tmm = false;
            let thereAreV1Hashes = false;
            let thereAreV2Hashes = false;
            const tagCount = new Map();
            const categoryCount = new Map();

            const selectedRows = torrentsTable.selectedRowsIds();
            selectedRows.forEach((item, index) => {
                const data = torrentsTable.getRow(item).full_data;

                if (data["seq_dl"] !== true)
                    all_are_seq_dl = false;
                else
                    there_are_seq_dl = true;

                if (data["f_l_piece_prio"] !== true)
                    all_are_f_l_piece_prio = false;
                else
                    there_are_f_l_piece_prio = true;

                if (data["progress"] !== 1.0) // not downloaded
                    all_are_downloaded = false;
                else if (data["super_seeding"] !== true)
                    all_are_super_seeding = false;

                if ((data["state"] !== "stoppedUP") && (data["state"] !== "stoppedDL"))
                    all_are_stopped = false;
                else
                    there_are_stopped = true;

                if (data["force_start"] !== true)
                    all_are_force_start = false;
                else
                    there_are_force_start = true;

                if (data["auto_tmm"] === true)
                    there_are_auto_tmm = true;
                else
                    all_are_auto_tmm = false;

                if (data["infohash_v1"] !== "")
                    thereAreV1Hashes = true;

                if (data["infohash_v2"] !== "")
                    thereAreV2Hashes = true;

                const torrentTags = data["tags"].split(", ");
                for (const tag of torrentTags) {
                    const count = tagCount.get(tag);
                    tagCount.set(tag, ((count !== undefined) ? (count + 1) : 1));
                }

                const torrentCategory = data["category"];
                const count = categoryCount.get(torrentCategory);
                categoryCount.set(torrentCategory, ((count !== undefined) ? (count + 1) : 1));
            });

            // hide renameFiles when more than 1 torrent is selected
            if (selectedRows.length === 1) {
                const data = torrentsTable.getRow(selectedRows[0]).full_data;
                const metadata_downloaded = !((data["state"] === "metaDL") || (data["state"] === "forcedMetaDL") || (data["total_size"] === -1));

                this.showItem("rename");
                // hide renameFiles when metadata hasn't been downloaded yet
                metadata_downloaded
                    ? this.showItem("renameFiles")
                    : this.hideItem("renameFiles");
            }
            else {
                this.hideItem("renameFiles");
                this.hideItem("rename");
            }

            if (all_are_downloaded) {
                this.hideItem("downloadLimit");
                this.menu.querySelector("a[href$=uploadLimit]").parentNode.classList.add("separator");
                this.hideItem("sequentialDownload");
                this.hideItem("firstLastPiecePrio");
                this.showItem("superSeeding");
                this.setItemChecked("superSeeding", all_are_super_seeding);
            }
            else {
                const show_seq_dl = (all_are_seq_dl || !there_are_seq_dl);
                const show_f_l_piece_prio = (all_are_f_l_piece_prio || !there_are_f_l_piece_prio);

                this.menu.querySelector("a[href$=firstLastPiecePrio]").parentNode.classList.toggle("separator", (!show_seq_dl && show_f_l_piece_prio));

                if (show_seq_dl)
                    this.showItem("sequentialDownload");
                else
                    this.hideItem("sequentialDownload");

                if (show_f_l_piece_prio)
                    this.showItem("firstLastPiecePrio");
                else
                    this.hideItem("firstLastPiecePrio");

                this.setItemChecked("sequentialDownload", all_are_seq_dl);
                this.setItemChecked("firstLastPiecePrio", all_are_f_l_piece_prio);

                this.showItem("downloadLimit");
                this.menu.querySelector("a[href$=uploadLimit]").parentNode.classList.remove("separator");
                this.hideItem("superSeeding");
            }

            this.showItem("start");
            this.showItem("stop");
            this.showItem("forceStart");
            if (all_are_stopped)
                this.hideItem("stop");
            else if (all_are_force_start)
                this.hideItem("forceStart");
            else if (!there_are_stopped && !there_are_force_start)
                this.hideItem("start");

            if (!all_are_auto_tmm && there_are_auto_tmm) {
                this.hideItem("autoTorrentManagement");
            }
            else {
                this.showItem("autoTorrentManagement");
                this.setItemChecked("autoTorrentManagement", all_are_auto_tmm);
            }

            this.setEnabled("copyInfohash1", thereAreV1Hashes);
            this.setEnabled("copyInfohash2", thereAreV2Hashes);

            const contextTagList = $("contextTagList");
            for (const tag of tagMap.keys()) {
                const checkbox = contextTagList.querySelector(`a[href="#Tag/${tag}"] input[type="checkbox"]`);
                const count = tagCount.get(tag);
                const hasCount = (count !== undefined);
                const isLesser = (count < selectedRows.length);
                checkbox.indeterminate = (hasCount ? isLesser : false);
                checkbox.checked = (hasCount ? !isLesser : false);
            }

            const contextCategoryList = document.getElementById("contextCategoryList");
            for (const category of categoryMap.keys()) {
                const categoryIcon = contextCategoryList.querySelector(`a[href$="#Category/${category}"] img`);
                const count = categoryCount.get(category);
                const isEqual = ((count !== undefined) && (count === selectedRows.length));
                categoryIcon.classList.toggle("highlightedCategoryIcon", isEqual);
            }
        }

        updateCategoriesSubMenu(categories) {
            const contextCategoryList = $("contextCategoryList");
            [...contextCategoryList.children].forEach((el) => { el.destroy(); });

            const createMenuItem = (text, imgURL, clickFn) => {
                const anchor = document.createElement("a");
                anchor.textContent = text;
                anchor.addEventListener("click", () => { clickFn(); });

                const img = document.createElement("img");
                img.src = imgURL;
                img.alt = text;
                anchor.prepend(img);

                const item = document.createElement("li");
                item.appendChild(anchor);

                return item;
            };
            contextCategoryList.appendChild(createMenuItem("QBT_TR(New...)QBT_TR[CONTEXT=TransferListWidget]", "images/list-add.svg", torrentNewCategoryFN));
            contextCategoryList.appendChild(createMenuItem("QBT_TR(Reset)QBT_TR[CONTEXT=TransferListWidget]", "images/edit-clear.svg", () => { torrentSetCategoryFN(""); }));

            const sortedCategories = [...categories.keys()];
            sortedCategories.sort(window.qBittorrent.Misc.naturalSortCollator.compare);

            let first = true;
            for (const categoryName of sortedCategories) {
                const anchor = document.createElement("a");
                anchor.href = `#Category/${categoryName}`;
                anchor.textContent = categoryName;
                anchor.addEventListener("click", (event) => {
                    event.preventDefault();
                    torrentSetCategoryFN(categoryName);
                });

                const img = document.createElement("img");
                img.src = "images/view-categories.svg";
                anchor.prepend(img);

                const setCategoryItem = document.createElement("li");
                setCategoryItem.appendChild(anchor);
                if (first) {
                    setCategoryItem.classList.add("separator");
                    first = false;
                }

                contextCategoryList.appendChild(setCategoryItem);
            }
        }

        updateTagsSubMenu(tags) {
            const contextTagList = $("contextTagList");
            contextTagList.replaceChildren();

            const createMenuItem = (text, imgURL, clickFn) => {
                const anchor = document.createElement("a");
                anchor.textContent = text;
                anchor.addEventListener("click", () => { clickFn(); });

                const img = document.createElement("img");
                img.src = imgURL;
                img.alt = text;
                anchor.prepend(img);

                const item = document.createElement("li");
                item.appendChild(anchor);

                return item;
            };
            contextTagList.appendChild(createMenuItem("QBT_TR(Add...)QBT_TR[CONTEXT=TransferListWidget]", "images/list-add.svg", torrentAddTagsFN));
            contextTagList.appendChild(createMenuItem("QBT_TR(Remove All)QBT_TR[CONTEXT=TransferListWidget]", "images/edit-clear.svg", torrentRemoveAllTagsFN));

            const sortedTags = [...tags.keys()];
            sortedTags.sort(window.qBittorrent.Misc.naturalSortCollator.compare);

            for (let i = 0; i < sortedTags.length; ++i) {
                const tagName = sortedTags[i];

                const input = document.createElement("input");
                input.type = "checkbox";
                input.addEventListener("click", (event) => {
                    input.checked = !input.checked;
                });

                const anchor = document.createElement("a");
                anchor.href = `#Tag/${tagName}`;
                anchor.textContent = tagName;
                anchor.addEventListener("click", (event) => {
                    event.preventDefault();
                    torrentSetTagsFN(tagName, !input.checked);
                });
                anchor.prepend(input);

                const setTagItem = document.createElement("li");
                setTagItem.appendChild(anchor);
                if (i === 0)
                    setTagItem.classList.add("separator");

                contextTagList.appendChild(setTagItem);
            }
        }
    };

    class StatusesFilterContextMenu extends FilterListContextMenu {
        updateMenuItems() {
            this.updateTorrentActions();
        }
    };

    class CategoriesFilterContextMenu extends FilterListContextMenu {
        updateMenuItems() {
            const id = this.options.element.id;
            if ((id !== CATEGORIES_ALL) && (id !== CATEGORIES_UNCATEGORIZED)) {
                this.showItem("editCategory");
                this.showItem("deleteCategory");
                if (useSubcategories)
                    this.showItem("createSubcategory");
                else
                    this.hideItem("createSubcategory");
            }
            else {
                this.hideItem("editCategory");
                this.hideItem("deleteCategory");
                this.hideItem("createSubcategory");
            }

            this.updateTorrentActions();
        }
    };

    class TagsFilterContextMenu extends FilterListContextMenu {
        updateMenuItems() {
            const id = this.options.element.id;
            if ((id !== TAGS_ALL) && (id !== TAGS_UNTAGGED))
                this.showItem("deleteTag");
            else
                this.hideItem("deleteTag");

            this.updateTorrentActions();
        }
    };

    class TrackersFilterContextMenu extends FilterListContextMenu {
        updateMenuItems() {
            const id = this.options.element.id;
            if ((id !== TRACKERS_ALL) && (id !== TRACKERS_TRACKERLESS))
                this.showItem("deleteTracker");
            else
                this.hideItem("deleteTracker");

            this.updateTorrentActions();
        }
    };

    class SearchPluginsTableContextMenu extends ContextMenu {
        updateMenuItems() {
            const enabledColumnIndex = (text) => {
                const columns = document.querySelectorAll("#searchPluginsTableFixedHeaderRow th");
                return Array.prototype.findIndex.call(columns, (column => column.textContent === "Enabled"));
            };

            this.showItem("Enabled");
            this.setItemChecked("Enabled", (this.options.element.children[enabledColumnIndex()].textContent === "Yes"));

            this.showItem("Uninstall");
        }
    };

    class RssFeedContextMenu extends ContextMenu {
        updateMenuItems() {
            const selectedRows = window.qBittorrent.Rss.rssFeedTable.selectedRowsIds();
            this.menu.querySelector("a[href$=newSubscription]").parentNode.classList.add("separator");
            switch (selectedRows.length) {
                case 0:
                    // remove separator on top of newSubscription entry to avoid double line
                    this.menu.querySelector("a[href$=newSubscription]").parentNode.classList.remove("separator");
                    // menu when nothing selected
                    this.hideItem("update");
                    this.hideItem("markRead");
                    this.hideItem("rename");
                    this.hideItem("edit");
                    this.hideItem("delete");
                    this.showItem("newSubscription");
                    this.showItem("newFolder");
                    this.showItem("updateAll");
                    this.hideItem("copyFeedURL");
                    break;
                case 1:
                    if (selectedRows[0] === "0") {
                        // menu when "unread" feed selected
                        this.showItem("update");
                        this.showItem("markRead");
                        this.hideItem("rename");
                        this.hideItem("edit");
                        this.hideItem("delete");
                        this.showItem("newSubscription");
                        this.hideItem("newFolder");
                        this.hideItem("updateAll");
                        this.hideItem("copyFeedURL");
                    }
                    else if (window.qBittorrent.Rss.rssFeedTable.getRow(selectedRows[0]).full_data.dataUid === "") {
                        // menu when single folder selected
                        this.showItem("update");
                        this.showItem("markRead");
                        this.showItem("rename");
                        this.hideItem("edit");
                        this.showItem("delete");
                        this.showItem("newSubscription");
                        this.showItem("newFolder");
                        this.hideItem("updateAll");
                        this.hideItem("copyFeedURL");
                    }
                    else {
                        // menu when single feed selected
                        this.showItem("update");
                        this.showItem("markRead");
                        this.showItem("rename");
                        this.showItem("edit");
                        this.showItem("delete");
                        this.showItem("newSubscription");
                        this.hideItem("newFolder");
                        this.hideItem("updateAll");
                        this.showItem("copyFeedURL");
                    }
                    break;
                default:
                    // menu when multiple items selected
                    this.showItem("update");
                    this.showItem("markRead");
                    this.hideItem("rename");
                    this.hideItem("edit");
                    this.showItem("delete");
                    this.hideItem("newSubscription");
                    this.hideItem("newFolder");
                    this.hideItem("updateAll");
                    this.showItem("copyFeedURL");
                    break;
            }
        }
    };

    class RssArticleContextMenu extends ContextMenu {};

    class RssDownloaderRuleContextMenu extends ContextMenu {
        adjustMenuPosition(e) {
            this.updateMenuItems();

            // draw the menu off-screen to know the menu dimensions
            this.menu.style.left = "-999em";
            this.menu.style.top = "-999em";
            // position the menu
            let xPosMenu = e.pageX + this.options.offsets.x - $("rssdownloaderpage").offsetLeft;
            let yPosMenu = e.pageY + this.options.offsets.y - $("rssdownloaderpage").offsetTop;
            if ((xPosMenu + this.menu.offsetWidth) > document.documentElement.clientWidth)
                xPosMenu -= this.menu.offsetWidth;
            if ((yPosMenu + this.menu.offsetHeight) > document.documentElement.clientHeight)
                yPosMenu = document.documentElement.clientHeight - this.menu.offsetHeight;
            xPosMenu = Math.max(xPosMenu, 0);
            yPosMenu = Math.max(yPosMenu, 0);

            this.menu.style.left = `${xPosMenu}px`;
            this.menu.style.top = `${yPosMenu}px`;
            this.menu.style.position = "absolute";
            this.menu.style.zIndex = "2000";
        }
        updateMenuItems() {
            const selectedRows = window.qBittorrent.RssDownloader.rssDownloaderRulesTable.selectedRowsIds();
            this.showItem("addRule");
            switch (selectedRows.length) {
                case 0:
                    // menu when nothing selected
                    this.hideItem("deleteRule");
                    this.hideItem("renameRule");
                    this.hideItem("clearDownloadedEpisodes");
                    break;
                case 1:
                    // menu when single item selected
                    this.showItem("deleteRule");
                    this.showItem("renameRule");
                    this.showItem("clearDownloadedEpisodes");
                    break;
                default:
                    // menu when multiple items selected
                    this.showItem("deleteRule");
                    this.hideItem("renameRule");
                    this.showItem("clearDownloadedEpisodes");
                    break;
            }
        }
    };

    return exports();
})();
Object.freeze(window.qBittorrent.ContextMenu);
