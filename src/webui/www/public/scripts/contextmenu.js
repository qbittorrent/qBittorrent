var ContextMenu = new Class({

	//implements
	Implements: [Options,Events],

	//options
	options: {
		actions: {},
		menu: 'contextmenu',
		stopEvent: true,
		targets: 'body',
		trigger: 'contextmenu',
		offsets: { x:0, y:0 },
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
			duration:this.options.fadeSpeed,
			onComplete: function() {
				if(this.getStyle('opacity')){
					this.setStyle('visibility','visible');
				}else{
					this.setStyle('visibility','hidden');
				}
			}.bind(this.menu)
		});
		
		//hide and begin the listener
		this.hide().startListener();
		
		//hide the menu
		this.menu.setStyles({ 'position':'absolute','top':'-900000px', 'display':'block' });
	},
	
	addTarget: function(t) {
		this.targets[this.targets.length] = t;
		t.addEvent(this.options.trigger,function(e) {
				//enabled?
				if(!this.options.disabled) {
					//prevent default, if told to
					if(this.options.stopEvent) { e.stop(); }
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
		t.addEvent('click',function(e) {
				this.hide();
			}.bind(this));
	},
	
	//get things started
	startListener: function() {
		/* all elements */
		this.targets.each(function(el) {
			/* show the menu */
			el.addEvent(this.options.trigger,function(e) {
				//enabled?
				if(!this.options.disabled) {
					//prevent default, if told to
					if(this.options.stopEvent) { e.stop(); }
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
			el.addEvent('click',function(e) {
				this.hide();
			}.bind(this));
		},this);
		
		/* menu items */
		this.menu.getElements('a').each(function(item) {
			item.addEvent('click',function(e) {
				if(!item.hasClass('disabled')) {
					this.execute(item.get('href').split('#')[1],$(this.options.element));
					this.fireEvent('click',[item,e]);
				}
			}.bind(this));
		},this);
		
		//hide on body click
		$(document.body).addEvent('click', function() {
			this.hide();
		}.bind(this));
	},
	
	//show menu
	show: function(trigger) {
		//this.menu.fade('in');
		this.fx.start(1);
		this.fireEvent('show');
		this.shown = true;
		return this;
	},
	
	//hide the menu
	hide: function(trigger) {
		if(this.shown)
		{
			this.fx.start(0);
			//this.menu.fade('out');
			this.fireEvent('hide');
			this.shown = false;
		}
		return this;
	},
	
	//disable an item
	disableItem: function(item) {
		this.menu.getElements('a[href$=' + item + ']').addClass('disabled');
		return this;
	},
	
	//enable an item
	enableItem: function(item) {
		this.menu.getElements('a[href$=' + item + ']').removeClass('disabled');
		return this;
	},
	
	//diable the entire menu
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
	execute: function(action,element) {
		if(this.options.actions[action]) {
			this.options.actions[action](element,this);
		}
		return this;
	}
	
});
