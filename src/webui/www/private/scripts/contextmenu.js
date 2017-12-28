var lastShownContexMenu = null;
var ContextMenu = new Class({
    //implements
    Implements: [Options, Events],

    //options
    options: {
        actions: {},
        menu: 'menu_id',
        stopEvent: true,
        targets: 'body',
        trigger: 'contextmenu',
        offsets: {
            x: 0,
            y: 0
        },
        onShow: $empty,
        onHide: $empty,
        onClick: $empty,
        fadeSpeed: 200
    },

    //initialization
    initialize: function(options) {
        //set options
        this.setOptions(options);

        //option diffs menu
        this.menu = $(this.options.menu);
        this.targets = $$(this.options.targets);

        //fx
        this.fx = new Fx.Tween(this.menu, {
            property: 'opacity',
            duration: this.options.fadeSpeed,
            onComplete: function() {
                if (this.getStyle('opacity')) {
                    this.setStyle('visibility', 'visible');
                }
                else {
                    this.setStyle('visibility', 'hidden');
                }
            }.bind(this.menu)
        });

        //hide and begin the listener
        this.hide().startListener();

        //hide the menu
        this.menu.setStyles({
            'position': 'absolute',
            'top': '-900000px',
            'display': 'block'
        });
    },

    adjustMenuPosition: function(e) {
        this.updateMenuItems();

        var scrollableMenuMaxHeight = document.documentElement.clientHeight * 0.75;

        if (this.menu.hasClass('scrollableMenu'))
            this.menu.setStyle('max-height', scrollableMenuMaxHeight);

        // draw the menu off-screen to know the menu dimensions
        this.menu.setStyles({
            left: '-999em',
            top: '-999em'
        });

        // position the menu
        var xPosMenu = e.page.x + this.options.offsets.x;
        var yPosMenu = e.page.y + this.options.offsets.y;
        if (xPosMenu + this.menu.offsetWidth > document.documentElement.clientWidth)
            xPosMenu -= this.menu.offsetWidth;
        if (yPosMenu + this.menu.offsetHeight > document.documentElement.clientHeight)
            yPosMenu = document.documentElement.clientHeight - this.menu.offsetHeight;
        if (xPosMenu < 0)
            xPosMenu = 0;
        if (yPosMenu < 0)
            yPosMenu = 0;
        this.menu.setStyles({
            left: xPosMenu,
            top: yPosMenu,
            position: 'absolute',
            'z-index': '2000'
        });

        // position the sub-menu
        var uls = this.menu.getElementsByTagName('ul');
        for (var i = 0; i < uls.length; ++i) {
            var ul = uls[i];
            if (ul.hasClass('scrollableMenu'))
                ul.setStyle('max-height', scrollableMenuMaxHeight);
            var rectParent = ul.parentNode.getBoundingClientRect();
            var xPosOrigin = rectParent.left;
            var yPosOrigin = rectParent.bottom;
            var xPos = xPosOrigin + rectParent.width - 1;
            var yPos = yPosOrigin - rectParent.height - 1;
            if (xPos + ul.offsetWidth > document.documentElement.clientWidth)
                xPos -= (ul.offsetWidth + rectParent.width - 2);
            if (yPos + ul.offsetHeight > document.documentElement.clientHeight)
                yPos = document.documentElement.clientHeight - ul.offsetHeight;
            if (xPos < 0)
                xPos = 0;
            if (yPos < 0)
                yPos = 0;
            ul.setStyles({
                'margin-left': xPos - xPosOrigin,
                'margin-top': yPos - yPosOrigin
            });
        }
    },

    addTarget: function(t) {
        this.targets[this.targets.length] = t;
        t.addEvent(this.options.trigger, function(e) {
            //enabled?
            if (!this.options.disabled) {
                //prevent default, if told to
                if (this.options.stopEvent) {
                    e.stop();
                }
                //record this as the trigger
                this.options.element = $(t);
                this.adjustMenuPosition(e);
                //show the menu
                this.show();
            }
        }.bind(this));
        t.addEvent('click', function(e) {
            this.hide();
        }.bind(this));
    },

    //get things started
    startListener: function() {
        /* all elements */
        this.targets.each(function(el) {
            /* show the menu */
            el.addEvent(this.options.trigger, function(e) {
                //enabled?
                if (!this.options.disabled) {
                    //prevent default, if told to
                    if (this.options.stopEvent) {
                        e.stop();
                    }
                    //record this as the trigger
                    this.options.element = $(el);
                    this.adjustMenuPosition(e);
                    //show the menu
                    this.show();
                }
            }.bind(this));
            el.addEvent('click', function(e) {
                this.hide();
            }.bind(this));
        }, this);

        /* menu items */
        this.menu.getElements('a').each(function(item) {
            item.addEvent('click', function(e) {
                e.preventDefault();
                if (!item.hasClass('disabled')) {
                    this.execute(item.get('href').split('#')[1], $(this.options.element));
                    this.fireEvent('click', [item, e]);
                }
            }.bind(this));
        }, this);

        //hide on body click
        $(document.body).addEvent('click', function() {
            this.hide();
        }.bind(this));
    },

    updateMenuItems: function() {},

    //show menu
    show: function(trigger) {
        if (lastShownContexMenu && lastShownContexMenu != this)
            lastShownContexMenu.hide();
        this.fx.start(1);
        this.fireEvent('show');
        this.shown = true;
        lastShownContexMenu = this;
        return this;
    },

    //hide the menu
    hide: function(trigger) {
        if (this.shown) {
            this.fx.start(0);
            //this.menu.fade('out');
            this.fireEvent('hide');
            this.shown = false;
        }
        return this;
    },

    setItemChecked: function(item, checked) {
        this.menu.getElement('a[href$=' + item + ']').firstChild.style.opacity =
            checked ? '1' : '0';
        return this;
    },

    getItemChecked: function(item) {
        return '0' != this.menu.getElement('a[href$=' + item + ']').firstChild.style.opacity;
    },

    //hide an item
    hideItem: function(item) {
        this.menu.getElement('a[href$=' + item + ']').parentNode.addClass('invisible');
        return this;
    },

    //show an item
    showItem: function(item) {
        this.menu.getElement('a[href$=' + item + ']').parentNode.removeClass('invisible');
        return this;
    },

    //disable the entire menu
    disable: function() {
        this.options.disabled = true;
        return this;
    },

    //enable the entire menu
    enable: function() {
        this.options.disabled = false;
        return this;
    },

    //execute an action
    execute: function(action, element) {
        if (this.options.actions[action]) {
            this.options.actions[action](element, this, action);
        }
        return this;
    }
});

var TorrentsTableContextMenu = new Class({
    Extends: ContextMenu,

    updateMenuItems: function() {
        var all_are_seq_dl = true;
        var there_are_seq_dl = false;
        var all_are_f_l_piece_prio = true;
        var there_are_f_l_piece_prio = false;
        var all_are_downloaded = true;
        var all_are_paused = true;
        var there_are_paused = false;
        var all_are_force_start = true;
        var there_are_force_start = false;
        var all_are_super_seeding = true;
        var all_are_auto_tmm = true;
        var there_are_auto_tmm = false;

        var h = torrentsTable.selectedRowsIds();
        h.each(function(item, index) {
            var data = torrentsTable.rows.get(item).full_data;

            if (data['seq_dl'] !== true)
                all_are_seq_dl = false;
            else
                there_are_seq_dl = true;

            if (data['f_l_piece_prio'] !== true)
                all_are_f_l_piece_prio = false;
            else
                there_are_f_l_piece_prio = true;

            if (data['progress'] != 1.0) // not downloaded
                all_are_downloaded = false;
            else if (data['super_seeding'] !== true)
                all_are_super_seeding = false;

            if (data['state'] != 'pausedUP' && data['state'] != 'pausedDL')
                all_are_paused = false;
            else
                there_are_paused = true;

            if (data['force_start'] !== true)
                all_are_force_start = false;
            else
                there_are_force_start = true;

            if (data['auto_tmm'] === true)
                there_are_auto_tmm = true;
            else
                all_are_auto_tmm = false;
        });

        var show_seq_dl = true;

        if (!all_are_seq_dl && there_are_seq_dl)
            show_seq_dl = false;

        var show_f_l_piece_prio = true;

        if (!all_are_f_l_piece_prio && there_are_f_l_piece_prio)
            show_f_l_piece_prio = false;

        if (all_are_downloaded) {
            this.hideItem('DownloadLimit');
            this.menu.getElement('a[href$=UploadLimit]').parentNode.addClass('separator');
            this.hideItem('SequentialDownload');
            this.hideItem('FirstLastPiecePrio');
            this.showItem('SuperSeeding');
            this.setItemChecked('SuperSeeding', all_are_super_seeding);
        }
        else {
            if (!show_seq_dl && show_f_l_piece_prio)
                this.menu.getElement('a[href$=FirstLastPiecePrio]').parentNode.addClass('separator');
            else
                this.menu.getElement('a[href$=FirstLastPiecePrio]').parentNode.removeClass('separator');

            if (show_seq_dl)
                this.showItem('SequentialDownload');
            else
                this.hideItem('SequentialDownload');

            if (show_f_l_piece_prio)
                this.showItem('FirstLastPiecePrio');
            else
                this.hideItem('FirstLastPiecePrio');

            this.setItemChecked('SequentialDownload', all_are_seq_dl);
            this.setItemChecked('FirstLastPiecePrio', all_are_f_l_piece_prio);

            this.showItem('DownloadLimit');
            this.menu.getElement('a[href$=UploadLimit]').parentNode.removeClass('separator');
            this.hideItem('SuperSeeding');
        }

        this.showItem('Start');
        this.showItem('Pause');
        this.showItem('ForceStart');
        if (all_are_paused)
            this.hideItem('Pause');
        else if (all_are_force_start)
            this.hideItem('ForceStart');
        else if (!there_are_paused && !there_are_force_start)
            this.hideItem('Start');

        if (!all_are_auto_tmm && there_are_auto_tmm) {
            this.hideItem('AutoTorrentManagement');
        }
        else {
            this.showItem('AutoTorrentManagement');
            this.setItemChecked('AutoTorrentManagement', all_are_auto_tmm);
        }

    },

    updateCategoriesSubMenu: function(category_list) {
        var categoryList = $('contextCategoryList');
        categoryList.empty();
        categoryList.appendChild(new Element('li', {
            html: '<a href="javascript:torrentNewCategoryFN();"><img src="images/qbt-theme/list-add.svg" alt="QBT_TR(New...)QBT_TR[CONTEXT=TransferListWidget]"/> QBT_TR(New...)QBT_TR[CONTEXT=TransferListWidget]</a>'
        }));
        categoryList.appendChild(new Element('li', {
            html: '<a href="javascript:torrentSetCategoryFN(0);"><img src="images/qbt-theme/edit-clear.svg" alt="QBT_TR(Reset)QBT_TR[CONTEXT=TransferListWidget]"/> QBT_TR(Reset)QBT_TR[CONTEXT=TransferListWidget]</a>'
        }));

        var sortedCategories = [];
        Object.each(category_list, function(category) {
            sortedCategories.push(category.name);
        });
        sortedCategories.sort();

        var first = true;
        Object.each(sortedCategories, function(categoryName) {
            var categoryHash = genHash(categoryName);
            var el = new Element('li', {
                html: '<a href="javascript:torrentSetCategoryFN(\'' + categoryHash + '\');"><img src="images/qbt-theme/inode-directory.svg"/> ' + escapeHtml(categoryName) + '</a>'
            });
            if (first) {
                el.addClass('separator');
                first = false;
            }
            categoryList.appendChild(el);
        });
    }
});

var CategoriesFilterContextMenu = new Class({
    Extends: ContextMenu,
    updateMenuItems: function() {
        var id = this.options.element.id;
        if ((id != CATEGORIES_ALL) && (id != CATEGORIES_UNCATEGORIZED)) {
            this.showItem('EditCategory');
            this.showItem('DeleteCategory');
        }
        else {
            this.hideItem('EditCategory');
            this.hideItem('DeleteCategory');
        }
    }
});

var SearchPluginsTableContextMenu = new Class({
    Extends: ContextMenu,

    updateMenuItems: function () {
        var enabledColumnIndex = function(text) {
            var columns = $("searchPluginsTableFixedHeaderRow").getChildren("th");
            for (var i = 0; i < columns.length; ++i)
                if (columns[i].get("html") === "Enabled")
                    return i;
        };

        this.showItem('Enabled');
        this.setItemChecked('Enabled', this.options.element.getChildren("td")[enabledColumnIndex()].get("html") === "Yes");

        this.showItem('Uninstall');
    }
});
