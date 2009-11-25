/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org> & Christophe Dumez <chris@qbittorrent.org>
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

/**************************************************************

	Script		: Dynamic Table
	Version		: 0.5
	Authors		: Ishan Arora & Christophe Dumez
	Desc			: Programable sortable table
	Licence		: Open Source MIT Licence

**************************************************************/

var dynamicTable = new Class	({

	initialize: function(){
	},
	
	setup: function(table, progressIndex, context_menu){
		this.table = $(table);
		this.rows = new Hash();
		this.cur = new Array();
		this.priority_hidden = false;
		this.progressIndex = progressIndex;
		this.filter = 'all';
		this.context_menu = context_menu;
	},
	
	getCurrentTorrentHash: function() {
		if(this.cur.length > 0)
			return this.cur[0];
		return '';
	},

	altRow: function()
	{
		var trs = this.table.getElements('tr');
			trs.each(function(el,i){
				if(i % 2){
					el.addClass('alt');
				}else{
					el.removeClass('alt');
				}
			}.bind(this));
	},

	hidePriority: function(){
		if(this.priority_hidden) return;
		$('prioHeader').addClass('invisible');
		 var trs = this.table.getElements('tr');
                        trs.each(function(tr,i){
				var tds = tr.getElements('td');
				tds[2].addClass('invisible');
                        }.bind(this));
		this.priority_hidden = true;
	},
	
	setFilter: function(f) {
		this.filter = f;
	},

	showPriority: function(){
                if(!this.priority_hidden) return;
		$('prioHeader').removeClass('invisible');
                 var trs = this.table.getElements('tr');
                        trs.each(function(tr,i){
                                var tds = tr.getElements('td');
                                tds[2].removeClass('invisible');
                        }.bind(this));
                this.priority_hidden = false;
        },
	
	applyFilterOnRow: function(tr, status) {
		switch(this.filter) {
			case 'all':
				tr.removeClass("invisible");
				return;
			case 'downloading':
				if(status == "downloading" || status == "stalledDL" || status == "checkingDL" || status == "pausedDL" || status == "queuedDL") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				return;
			case 'completed':
				if(status == "seeding" || status == "stalledUP" || status == "checkingUP" || status == "pausedUP" || status == "queuedUP") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				return;
			case 'active':
				if(status == "downloading" || status == "seeding") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				return;
			case 'inactive':
				if(status != "downloading" && status != "seeding") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				return;
		}
	},

	insertRow: function(id, row, status){
		if(this.rows.has(id)) {
			return;
		}
		var tr = new Element('tr');
		tr.addClass("menu-target");
		this.rows.set(id, tr);
		for(var i=0; i<row.length; i++)
		{
			var td = new Element('td');
			if(i==this.progressIndex) {
				td.adopt(new ProgressBar(row[i].toFloat(), {width:80}));
			} else {
				if(i==0) {
					td.adopt(new Element('img', {'src':row[i]}));
				} else {
					td.set('html', row[i]);
				}
			}
			td.injectInside(tr);
		};

			tr.addEvent('mouseover', function(e){
				tr.addClass('over');
			}.bind(this));
			tr.addEvent('mouseout', function(e){
				tr.removeClass('over');
			}.bind(this));
		tr.addEvent('contextmenu', function(e) {
			if(!this.cur.contains(id)) {
				// Remove selected style from previous ones
				for(i=0; i<this.cur.length; i++) {
					if(this.rows.has(this.cur[i])) {
						var temptr = this.rows.get(this.cur[i]);
						temptr.removeClass('selected');
					}
				}
				this.cur.empty();
				this.cur[this.cur.length] = id;
				temptr = this.rows.get(id);
				temptr.addClass("selected");
			}
			return true;
		}.bind(this));
		tr.addEvent('click', function(e){
			e.stop();
			if(e.control) {
				// CTRL key was pressed
				if(this.cur.contains(id)) {
				  // remove it
					this.cur.erase(id);
					// Remove selected style
					if(this.rows.has(id)) {
						temptr = this.rows.get(id);
						temptr.removeClass('selected');
					}
				} else {
					this.cur[this.cur.length] = id;
					// Add selected style
					if(this.rows.has(id)) {
						temptr = this.rows.get(id);
						temptr.addClass('selected');
					}
				}
			} else {
				if(e.shift && this.cur.length == 1) {
					// Shift key was pressed
					ids = this.getRowIds();
					beginIndex = ids.indexOf(this.cur[0]);
					endIndex = ids.indexOf(id);
					if(beginIndex > endIndex) {
						// Backward shift
						tmp = beginIndex;
						beginIndex = endIndex-1;
						endIndex = tmp-1;
					}
					for(i=beginIndex+1; i<(endIndex+1); i++) {
						curID = ids[i];
						this.cur[this.cur.length] = curID;
						// Add selected style
						if(this.rows.has(curID)) {
						temptr = this.rows.get(curID);
							temptr.addClass('selected');
						}
					}
				} else {
					// Simple selection
					// Remove selected style from previous ones
					for(i=0; i<this.cur.length; i++) {
						if(this.rows.has(this.cur[i])) {
							var temptr = this.rows.get(this.cur[i]);
							temptr.removeClass('selected');
						}
					}
					this.cur.empty();
					// Add selected style to new one
					if(this.rows.has(id)) {
						temptr = this.rows.get(id);
						temptr.addClass('selected');
					}
					this.cur[0] = id;
					// TODO: Warn Properties panel
				}
			}
			return false;
		}.bind(this));
		// Apply filter
		this.applyFilterOnRow(tr, status);
		// Insert
		tr.injectInside(this.table);
		this.altRow();
		// Update context menu
		this.context_menu.addTarget(tr);
	},
	
	selectAll: function() {
		this.cur.empty();
		this.rows.each(function(tr, id){
			this.cur[this.cur.length] = id;
			if(!tr.hasClass('selected')) {
				tr.addClass('selected');
			}
		});
	},

	updateRow: function(id, row, status){
		if(!this.rows.has(id)) {
			return false;
		}
		var tr = this.rows.get(id);
		// Apply filter
		this.applyFilterOnRow(tr, status);
		var tds = tr.getElements('td');
		for(var i=0; i<row.length; i++) {
			if(i==this.progressIndex) {
				new ProgressBar(row[i].toFloat(), {width:80}).replaces(tds[i].getChildren()[0]);
				/*tds[i].set('html', '');
				tds[i].adopt(new ProgressBar(row[i].toFloat(), {width:80}));*/
			} else {
				if(i==0) {
					tds[i].getChildren('img')[0].set('src', row[i]);
				} else {
					tds[i].set('html', row[i]);
				}
			}
		};
		return true;
	},

	removeRow: function(id){
		if(this.cur.contains(id))
		{
			this.cur.erase(id);
		}
		if(this.rows.has(id)) {
			var tr = this.rows.get(id);
			tr.dispose();
			this.altRow();
			this.rows.erase(id);
			return true;
		}
		return false;
	},

	selectedIds: function(){
		return this.cur;
	},
	
	getRowIds: function(){
		var ids = new Array();
		var i = 0;
		this.rows.each(function(tr, id) {
			ids[i] = id;
			i++;
		}.bind(this));
		return ids;
	}
});
//dynamicTable.implement(new Options);
//dynamicTable.implement(new Events);

/*************************************************************/
