var ContextMenu = new Class({

    //implements
    Implements: [Options, Events],

    //options
    options: {
        actions: {},
        menu: 'contextmenu',
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
        this.setOptions(options)

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
                //position the menu
                this.menu.setStyles({
                    top: (e.page.y + this.options.offsets.y),
                    left: (e.page.x + this.options.offsets.x),
                    position: 'absolute',
                    'z-index': '2000'
                });
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
                    //position the menu
                    this.menu.setStyles({
                        top: (e.page.y + this.options.offsets.y),
                        left: (e.page.x + this.options.offsets.x),
                        position: 'absolute',
                        'z-index': '2000'
                    });
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

    updateMenuItems: function () {
        all_are_seq_dl = true;
        there_are_seq_dl = false;
        all_are_f_l_piece_prio = true;
        there_are_f_l_piece_prio = false;
        all_are_downloaded = true;
        all_are_paused = true;
        there_are_paused = false;
        all_are_super_seeding = true;
        all_are_force_start = true;

        var h = myTable.selectedIds();
        h.each(function(item, index){
            var data = myTable.rows.get(item).full_data;

            if (data['seq_dl'] != true)
                all_are_seq_dl = false;
            else
                there_are_seq_dl = true;

            if (data['f_l_piece_prio'] != true)
                all_are_f_l_piece_prio = false;
            else
                there_are_f_l_piece_prio = true;

            if (data['force_start'] != true)
                all_are_force_start = false;

            if (data['progress'] != 1.0) // not downloaded
                all_are_downloaded = false;
            else if (data['super_seeding'] != true)
                all_are_super_seeding = false;

            state = data['state'];
            if ((state != 'pausedUP') && (state != 'pausedDL'))
                all_are_paused = false;
            else
                there_are_paused = true;
        });

        show_seq_dl = true;

        if (!all_are_seq_dl && there_are_seq_dl)
            show_seq_dl = false;

        show_f_l_piece_prio = true;

        if (!all_are_f_l_piece_prio && there_are_f_l_piece_prio)
            show_f_l_piece_prio = false;

        this.setItemChecked('ForceStart', all_are_force_start);

        if (all_are_downloaded) {
            this.hideItem('SequentialDownload');
            this.hideItem('FirstLastPiecePrio');
            this.showItem('SuperSeeding');
            this.setItemChecked('SuperSeeding', all_are_super_seeding);
        } else {
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

            this.hideItem('SuperSeeding');
        }

        if (all_are_paused) {
            this.showItem('Start');
            this.hideItem('Pause');
        } else {
            if (there_are_paused) {
                this.showItem('Start');
                this.showItem('Pause');
            } else {
                this.hideItem('Start');
                this.showItem('Pause');
            }
        }
    },

    //show menu
    show: function(trigger) {
        this.updateMenuItems();
        this.fx.start(1);
        this.fireEvent('show');
        this.shown = true;
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
            this.options.actions[action](element, this);
        }
        return this;
    }

});