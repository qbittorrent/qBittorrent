/*
 * MIT License
 * Copyright (c) 2008 Christophe Dumez <chris@qbittorrent.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Original code from http://www.silverscripting.com/mootabs/
 * Ported to Mootools 1.2 by Christophe Dumez
 */

var mootabs = new Class({
 
	Implements: [Options],
 
	options: {
		width:				'300px',
		height:				'200px',
		changeTransition:	Fx.Transitions.Bounce.easeOut,
		duration:			1000,
		mouseOverClass:		'over',
		activateOnLoad:		'first',
		useAjax: 			false,
		ajaxUrl: 			'',
		ajaxOptions: 		{method:'get', evalScripts: true},
		ajaxLoadingText: 	'Loading...'
	},
 
	initialize: function(element, options) {
 
		if(options) this.setOptions(options);
 
		this.el = $(element);
		this.elid = element;
 
		this.el.setStyles({
			height: this.options.height,
			width: this.options.width
		});
 
		this.titles = $$('#' + this.elid + ' ul li');
		this.panelHeight = this.el.getSize().y - (this.titles[0].getSize().y + 4);
		this.panels = $$('#' + this.elid + ' .mootabs_panel');
 
 
		this.panels.setStyle('height', this.panelHeight);
 
		this.titles.each(function(item) {
			item.addEvent('click', function(){
				if(item != this.activeTitle)
				{
					item.removeClass(this.options.mouseOverClass);
					this.activate(item);
				}
 
			}.bind(this));
 
			item.addEvent('mouseover', function() {
				if(item != this.activeTitle)
				{
					item.addClass(this.options.mouseOverClass);
				}
			}.bind(this));
 
			item.addEvent('mouseout', function() {
				if(item != this.activeTitle)
				{
					item.removeClass(this.options.mouseOverClass);
				}
			}.bind(this));
		}.bind(this));
 
 
		if(this.options.activateOnLoad != 'none')
		{
			if(this.options.activateOnLoad == 'first')
			{
				this.activate(this.titles[0], true);
			}
			else
			{
				this.activate(this.options.activateOnLoad, true);	
			}
		}
	},
 
	activate: function(tab, skipAnim){
		if(! $defined(skipAnim))
		{
			skipAnim = false;
		}
		if($type(tab) == 'string') 
		{
			myTab = $$('#' + this.elid + ' ul li').filter('[title=' + tab + ']')[0];
			tab = myTab;
		}
 
		if($type(tab) == 'element')
		{
			var newTab = tab.getProperty('title');
 
			this.panels.removeClass('active');
 
			this.activePanel = this.panels.filter('#' + newTab)[0];
 
			this.activePanel.addClass('active');
 
			if(this.options.changeTransition != 'none' && skipAnim==false)
			{
				this.panels.filter('#' + newTab).setStyle('height', 0);
				var changeEffect = new Fx.Elements(this.panels.filter('#' + newTab), {duration: this.options.duration, transition: this.options.changeTransition});
				changeEffect.start({
					'0': {
						'height': [0, this.panelHeight]
					}
				});
			}
 
			this.titles.removeClass('active');
 
			tab.addClass('active');
 
			this.activeTitle = tab;
 
			if(this.options.useAjax)
			{
				this._getContent();
			}
		}
	},
 
	_getContent: function(){
		this.activePanel.setHTML(this.options.ajaxLoadingText);
		var newOptions = {
			url: this.options.ajaxUrl + '?tab=' + this.activeTitle.getProperty('title'),
			update: this.activePanel.getProperty('id')
		};
		this.options.ajaxOptions = $merge(this.options.ajaxOptions, newOptions);
		var tabRequest = new Request.HTML(this.options.ajaxOptions);
		tabRequest.send();
	},
 
	addTab: function(title, label, content){
		//the new title
		var newTitle = new Element('li', {
			'title': title
		});
		newTitle.appendText(label);
		this.titles.include(newTitle);
		$$('#' + this.elid + ' ul').adopt(newTitle);
		newTitle.addEvent('click', function() {
			this.activate(newTitle);
		}.bind(this));
 
		newTitle.addEvent('mouseover', function() {
			if(newTitle != this.activeTitle)
			{
				newTitle.addClass(this.options.mouseOverClass);
			}
		}.bind(this));
		newTitle.addEvent('mouseout', function() {
			if(newTitle != this.activeTitle)
			{
				newTitle.removeClass(this.options.mouseOverClass);
			}
		}.bind(this));
		//the new panel
		var newPanel = new Element('div', {
			'style': {'height': this.options.panelHeight},
			'id': title,
			'class': 'mootabs_panel'
		});
		if(!this.options.useAjax)
		{
			newPanel.setHTML(content);
		}
		this.panels.include(newPanel);
		this.el.adopt(newPanel);
	},
 
	removeTab: function(title){
		if(this.activeTitle.title == title)
		{
			this.activate(this.titles[0]);
		}
		$$('#' + this.elid + ' ul li').filter('[title=' + title + ']')[0].remove();
 
		$$('#' + this.elid + ' .mootabs_panel').filter('#' + title)[0].remove();
	},
 
	next: function(){
		var nextTab = this.activeTitle.getNext();
		if(!nextTab) {
			nextTab = this.titles[0];
		}
		this.activate(nextTab);
	},
 
	previous: function(){
		var previousTab = this.activeTitle.getPrevious();
		if(!previousTab) {
			previousTab = this.titles[this.titles.length - 1];
		}
		this.activate(previousTab);
	}
});