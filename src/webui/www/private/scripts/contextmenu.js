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

if (window.qBittorrent === undefined)
    window.qBittorrent = {};

window.qBittorrent.ContextMenu = (function() {
    const exports = function() {
        return {
            ContextMenu: ContextMenu,
            TorrentsTableContextMenu: TorrentsTableContextMenu,
            CategoriesFilterContextMenu: CategoriesFilterContextMenu,
            TagsFilterContextMenu: TagsFilterContextMenu,
            SearchPluginsTableContextMenu: SearchPluginsTableContextMenu,
            RssFeedContextMenu: RssFeedContextMenu,
            RssArticleContextMenu: RssArticleContextMenu,
            RssDownloaderRuleContextMenu: RssDownloaderRuleContextMenu
        };
    };

    let lastShownContextMenu = null;
    const ContextMenu = new Class({
        // implements
        Implements: [Options, Events],

        // options
        options: {
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
            touchTimer: 600
        },

        // initialization
        initialize: function(options) {
            // set options
            this.setOptions(options);

            // option diffs menu
            this.menu = $(this.options.menu);

            // fx
            this.fx = new Fx.Tween(this.menu, {
                property: "opacity",
                duration: this.options.fadeSpeed,
                onComplete: function() {
                    if (this.getStyle("opacity"))
                        this.setStyle("visibility", "visible");
                    else
                        this.setStyle("visibility", "hidden");
                }.bind(this.menu)
            });

            // hide and begin the listener
            this.hide().startListener();

            // hide the menu
            this.menu.setStyles({
                "position": "absolute",
                "top": "-900000px",
                "display": "block"
            });
        },

        adjustMenuPosition: function(e) {
            this.updateMenuItems();

            const scrollableMenuMaxHeight = document.documentElement.clientHeight * 0.75;

            if (this.menu.hasClass("scrollableMenu"))
                this.menu.setStyle("max-height", scrollableMenuMaxHeight);

            // draw the menu off-screen to know the menu dimensions
            this.menu.setStyles({
                left: "-999em",
                top: "-999em"
            });

            // position the menu
            let xPosMenu = e.page.x + this.options.offsets.x;
            let yPosMenu = e.page.y + this.options.offsets.y;
            if ((xPosMenu + this.menu.offsetWidth) > document.documentElement.clientWidth)
                xPosMenu -= this.menu.offsetWidth;
            if ((yPosMenu + this.menu.offsetHeight) > document.documentElement.clientHeight)
                yPosMenu = document.documentElement.clientHeight - this.menu.offsetHeight;
            if (xPosMenu < 0)
                xPosMenu = 0;
            if (yPosMenu < 0)
                yPosMenu = 0;
            this.menu.setStyles({
                left: xPosMenu,
                top: yPosMenu,
                position: "absolute",
                "z-index": "2000"
            });

            // position the sub-menu
            const uls = this.menu.getElementsByTagName("ul");
            for (let i = 0; i < uls.length; ++i) {
                const ul = uls[i];
                if (ul.hasClass("scrollableMenu"))
                    ul.setStyle("max-height", scrollableMenuMaxHeight);
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
                ul.setStyles({
                    "margin-left": xPos - xPosOrigin,
                    "margin-top": yPos - yPosOrigin
                });
            }
        },

        setupEventListeners: function(elem) {
            elem.addEvent("contextmenu", (e) => {
                this.triggerMenu(e, elem);
            });
            elem.addEvent("click", (e) => {
                this.hide();
            });

            elem.addEvent("touchstart", (e) => {
                this.hide();
                this.touchStartAt = performance.now();
                this.touchStartEvent = e;
            });
            elem.addEvent("touchend", (e) => {
                const now = performance.now();
                const touchStartAt = this.touchStartAt;
                const touchStartEvent = this.touchStartEvent;

                this.touchStartAt = null;
                this.touchStartEvent = null;

                const isTargetUnchanged = (Math.abs(e.event.pageX - touchStartEvent.event.pageX) <= 10) && (Math.abs(e.event.pageY - touchStartEvent.event.pageY) <= 10);
                if (((now - touchStartAt) >= this.options.touchTimer) && isTargetUnchanged)
                    this.triggerMenu(touchStartEvent, elem);
            });
        },

        addTarget: function(t) {
            if (t.hasEventListeners)
                return;

            // prevent long press from selecting this text
            t.style.setProperty("user-select", "none");
            t.style.setProperty("-webkit-user-select", "none");
            t.hasEventListeners = true;

            this.setupEventListeners(t);
        },

        triggerMenu: function(e, el) {
            if (this.options.disabled)
                return;

            // prevent default, if told to
            if (this.options.stopEvent)
                e.stop();
            // record this as the trigger
            this.options.element = $(el);
            this.adjustMenuPosition(e);
            // show the menu
            this.show();
        },

        // get things started
        startListener: function() {
            /* all elements */
            $$(this.options.targets).each((el) => {
                this.addTarget(el);
            }, this);

            /* menu items */
            this.menu.getElements("a").each(function(item) {
                item.addEvent("click", (e) => {
                    e.preventDefault();
                    if (!item.hasClass("disabled")) {
                        this.execute(item.get("href").split("#")[1], $(this.options.element));
                        this.fireEvent("click", [item, e]);
                    }
                });
            }, this);

            // hide on body click
            $(document.body).addEvent("click", () => {
                this.hide();
            });
        },

        updateMenuItems: function() {},

        // show menu
        show: function(trigger) {
            if (lastShownContextMenu && (lastShownContextMenu !== this))
                lastShownContextMenu.hide();
            this.fx.start(1);
            this.fireEvent("show");
            lastShownContextMenu = this;
            return this;
        },

        // hide the menu
        hide: function(trigger) {
            if (lastShownContextMenu && (lastShownContextMenu.menu.style.visibility !== "hidden")) {
                this.fx.start(0);
                // this.menu.fade('out');
                this.fireEvent("hide");
            }
            return this;
        },

        setItemChecked: function(item, checked) {
            this.menu.getElement("a[href$=" + item + "]").firstChild.style.opacity =
                checked ? "1" : "0";
            return this;
        },

        getItemChecked: function(item) {
            return this.menu.getElement("a[href$=" + item + "]").firstChild.style.opacity !== "0";
        },

        // hide an item
        hideItem: function(item) {
            this.menu.getElement("a[href$=" + item + "]").parentNode.addClass("invisible");
            return this;
        },

        // show an item
        showItem: function(item) {
            this.menu.getElement("a[href$=" + item + "]").parentNode.removeClass("invisible");
            return this;
        },

        // disable the entire menu
        disable: function() {
            this.options.disabled = true;
            return this;
        },

        // enable the entire menu
        enable: function() {
            this.options.disabled = false;
            return this;
        },

        // execute an action
        execute: function(action, element) {
            if (this.options.actions[action])
                this.options.actions[action](element, this, action);
            return this;
        }
    });

    const TorrentsTableContextMenu = new Class({
        Extends: ContextMenu,

        updateMenuItems: function() {
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
            const tagCount = new Map();

            const selectedRows = torrentsTable.selectedRowsIds();
            selectedRows.forEach((item, index) => {
                const data = torrentsTable.rows.get(item).full_data;

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

                const torrentTags = data["tags"].split(", ");
                for (const tag of torrentTags) {
                    const count = tagCount.get(tag);
                    tagCount.set(tag, ((count !== undefined) ? (count + 1) : 1));
                }
            });

            // hide renameFiles when more than 1 torrent is selected
            if (selectedRows.length === 1) {
                const data = torrentsTable.rows.get(selectedRows[0]).full_data;
                const metadata_downloaded = !((data["state"] === "metaDL") || (data["state"] === "forcedMetaDL") || (data["total_size"] === -1));

                // hide renameFiles when metadata hasn't been downloaded yet
                metadata_downloaded
                    ? this.showItem("renameFiles")
                    : this.hideItem("renameFiles");
            }
            else {
                this.hideItem("renameFiles");
            }

            if (all_are_downloaded) {
                this.hideItem("downloadLimit");
                this.menu.getElement("a[href$=uploadLimit]").parentNode.addClass("separator");
                this.hideItem("sequentialDownload");
                this.hideItem("firstLastPiecePrio");
                this.showItem("superSeeding");
                this.setItemChecked("superSeeding", all_are_super_seeding);
            }
            else {
                const show_seq_dl = (all_are_seq_dl || !there_are_seq_dl);
                const show_f_l_piece_prio = (all_are_f_l_piece_prio || !there_are_f_l_piece_prio);

                if (!show_seq_dl && show_f_l_piece_prio)
                    this.menu.getElement("a[href$=firstLastPiecePrio]").parentNode.addClass("separator");
                else
                    this.menu.getElement("a[href$=firstLastPiecePrio]").parentNode.removeClass("separator");

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
                this.menu.getElement("a[href$=uploadLimit]").parentNode.removeClass("separator");
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

            const contextTagList = $("contextTagList");
            tagList.forEach((tag, tagHash) => {
                const checkbox = contextTagList.getElement(`a[href="#Tag/${tagHash}"] input[type="checkbox"]`);
                const count = tagCount.get(tag.name);
                const hasCount = (count !== undefined);
                const isLesser = (count < selectedRows.length);
                checkbox.indeterminate = (hasCount ? isLesser : false);
                checkbox.checked = (hasCount ? !isLesser : false);
            });
        },

        updateCategoriesSubMenu: function(categoryList) {
            const contextCategoryList = $("contextCategoryList");
            contextCategoryList.getChildren().each(c => c.destroy());
            contextCategoryList.appendChild(new Element("li", {
                html: '<a href="javascript:torrentNewCategoryFN();"><img src="images/list-add.svg" alt="QBT_TR(New...)QBT_TR[CONTEXT=TransferListWidget]"/> QBT_TR(New...)QBT_TR[CONTEXT=TransferListWidget]</a>'
            }));
            contextCategoryList.appendChild(new Element("li", {
                html: '<a href="javascript:torrentSetCategoryFN(0);"><img src="images/edit-clear.svg" alt="QBT_TR(Reset)QBT_TR[CONTEXT=TransferListWidget]"/> QBT_TR(Reset)QBT_TR[CONTEXT=TransferListWidget]</a>'
            }));

            const sortedCategories = [];
            categoryList.forEach((category, hash) => sortedCategories.push({
                categoryName: category.name,
                categoryHash: hash
            }));
            sortedCategories.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(
                left.categoryName, right.categoryName));

            let first = true;
            for (const { categoryName, categoryHash } of sortedCategories) {
                const el = new Element("li", {
                    html: `<a href="javascript:torrentSetCategoryFN(${categoryHash});"><img src="images/view-categories.svg"/>${window.qBittorrent.Misc.escapeHtml(categoryName)}</a>`
                });
                if (first) {
                    el.addClass("separator");
                    first = false;
                }
                contextCategoryList.appendChild(el);
            }
        },

        updateTagsSubMenu: function(tagList) {
            const contextTagList = $("contextTagList");
            while (contextTagList.firstChild !== null)
                contextTagList.removeChild(contextTagList.firstChild);

            contextTagList.appendChild(new Element("li", {
                html: '<a href="javascript:torrentAddTagsFN();">'
                    + '<img src="images/list-add.svg" alt="QBT_TR(Add...)QBT_TR[CONTEXT=TransferListWidget]"/>'
                    + " QBT_TR(Add...)QBT_TR[CONTEXT=TransferListWidget]"
                    + "</a>"
            }));
            contextTagList.appendChild(new Element("li", {
                html: '<a href="javascript:torrentRemoveAllTagsFN();">'
                    + '<img src="images/edit-clear.svg" alt="QBT_TR(Remove All)QBT_TR[CONTEXT=TransferListWidget]"/>'
                    + " QBT_TR(Remove All)QBT_TR[CONTEXT=TransferListWidget]"
                    + "</a>"
            }));

            const sortedTags = [];
            tagList.forEach((tag, hash) => sortedTags.push({
                tagName: tag.name,
                tagHash: hash
            }));
            sortedTags.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(left.tagName, right.tagName));

            for (let i = 0; i < sortedTags.length; ++i) {
                const { tagName, tagHash } = sortedTags[i];
                const el = new Element("li", {
                    html: `<a href="#Tag/${tagHash}" onclick="event.preventDefault(); torrentSetTagsFN(${tagHash}, !event.currentTarget.getElement('input[type=checkbox]').checked);">`
                        + '<input type="checkbox" onclick="this.checked = !this.checked;"> ' + window.qBittorrent.Misc.escapeHtml(tagName)
                        + "</a>"
                });
                if (i === 0)
                    el.addClass("separator");
                contextTagList.appendChild(el);
            }
        }
    });

    const CategoriesFilterContextMenu = new Class({
        Extends: ContextMenu,
        updateMenuItems: function() {
            const id = Number(this.options.element.id);
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
        }
    });

    const TagsFilterContextMenu = new Class({
        Extends: ContextMenu,
        updateMenuItems: function() {
            const id = Number(this.options.element.id);
            if ((id !== TAGS_ALL) && (id !== TAGS_UNTAGGED))
                this.showItem("deleteTag");
            else
                this.hideItem("deleteTag");
        }
    });

    const SearchPluginsTableContextMenu = new Class({
        Extends: ContextMenu,

        updateMenuItems: function() {
            const enabledColumnIndex = function(text) {
                const columns = $("searchPluginsTableFixedHeaderRow").getChildren("th");
                for (let i = 0; i < columns.length; ++i) {
                    if (columns[i].get("html") === "Enabled")
                        return i;
                }
            };

            this.showItem("Enabled");
            this.setItemChecked("Enabled", this.options.element.getChildren("td")[enabledColumnIndex()].get("html") === "Yes");

            this.showItem("Uninstall");
        }
    });

    const RssFeedContextMenu = new Class({
        Extends: ContextMenu,
        updateMenuItems: function() {
            const selectedRows = window.qBittorrent.Rss.rssFeedTable.selectedRowsIds();
            this.menu.getElement("a[href$=newSubscription]").parentNode.addClass("separator");
            switch (selectedRows.length) {
                case 0:
                    // remove separator on top of newSubscription entry to avoid double line
                    this.menu.getElement("a[href$=newSubscription]").parentNode.removeClass("separator");
                    // menu when nothing selected
                    this.hideItem("update");
                    this.hideItem("markRead");
                    this.hideItem("rename");
                    this.hideItem("delete");
                    this.showItem("newSubscription");
                    this.showItem("newFolder");
                    this.showItem("updateAll");
                    this.hideItem("copyFeedURL");
                    break;
                case 1:
                    if (selectedRows[0] === 0) {
                        // menu when "unread" feed selected
                        this.showItem("update");
                        this.showItem("markRead");
                        this.hideItem("rename");
                        this.hideItem("delete");
                        this.showItem("newSubscription");
                        this.hideItem("newFolder");
                        this.hideItem("updateAll");
                        this.hideItem("copyFeedURL");
                    }
                    else if (window.qBittorrent.Rss.rssFeedTable.rows[selectedRows[0]].full_data.dataUid === "") {
                        // menu when single folder selected
                        this.showItem("update");
                        this.showItem("markRead");
                        this.showItem("rename");
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
                    this.showItem("delete");
                    this.hideItem("newSubscription");
                    this.hideItem("newFolder");
                    this.hideItem("updateAll");
                    this.showItem("copyFeedURL");
                    break;
            }
        }
    });

    const RssArticleContextMenu = new Class({
        Extends: ContextMenu
    });

    const RssDownloaderRuleContextMenu = new Class({
        Extends: ContextMenu,
        adjustMenuPosition: function(e) {
            this.updateMenuItems();

            // draw the menu off-screen to know the menu dimensions
            this.menu.setStyles({
                left: "-999em",
                top: "-999em"
            });
            // position the menu
            let xPosMenu = e.page.x + this.options.offsets.x - $("rssdownloaderpage").offsetLeft;
            let yPosMenu = e.page.y + this.options.offsets.y - $("rssdownloaderpage").offsetTop;
            if ((xPosMenu + this.menu.offsetWidth) > document.documentElement.clientWidth)
                xPosMenu -= this.menu.offsetWidth;
            if ((yPosMenu + this.menu.offsetHeight) > document.documentElement.clientHeight)
                yPosMenu = document.documentElement.clientHeight - this.menu.offsetHeight;
            xPosMenu = Math.max(xPosMenu, 0);
            yPosMenu = Math.max(yPosMenu, 0);

            this.menu.setStyles({
                left: xPosMenu,
                top: yPosMenu,
                position: "absolute",
                "z-index": "2000"
            });
        },
        updateMenuItems: function() {
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
    });

    return exports();
})();

Object.freeze(window.qBittorrent.ContextMenu);
