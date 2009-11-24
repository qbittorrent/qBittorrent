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
	
	setup: function(table, progressIndex){
		this.table = $(table);
		this.rows = new Object();
		this.cur = new Array();
		this.priority_hidden = false;
		this.progressIndex = progressIndex;
		this.filter = 'all';
		this.current_hash = '';
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
		var tr = this.rows[id];
		if($defined(tr))
		  return;
		//this.removeRow(id);
		var tr = new Element('tr');
		this.rows[id] = tr;
		for(var i=0; i<row.length; i++)
		{
			var td = new Element('td');
			if(i==this.progressIndex) {
				td.adopt(new ProgressBar(row[i].toFloat(), {width:80}));
			} else {
				td.set('html', row[i]);
			}
			td.injectInside(tr);
		};

			tr.addEvent('mouseover', function(e){
				tr.addClass('over');
			}.bind(this));
			tr.addEvent('mouseout', function(e){
				tr.removeClass('over');
			}.bind(this));
		tr.addEvent('click', function(e){
			e.stop();
			if(e.control) {
				// CTRL key was pressed
				if(this.cur.contains(id)) {
				  // remove it
					this.cur.erase(id);
					// Remove selected style
					temptr = this.rows[id];
			    if(temptr){
				    temptr.removeClass('selected');
			    }
				} else {
					this.cur[this.cur.length] = id;
					// Add selected style
					temptr = this.rows[id];
					if(temptr){
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
						temptr = this.rows[curID];
						if(temptr){
							temptr.addClass('selected');
						}
					}
				} else {
					// Simple selection
					// Remove selected style from previous ones
					for(i=0; i<this.cur.length; i++) {
						var temptr = this.rows[this.cur[i]];
						if(temptr){
							temptr.removeClass('selected');
						}
					}
					this.cur.empty();
					// Add selected style to new one
					temptr = this.rows[id];
					if(temptr){
						temptr.addClass('selected');
					}
					this.cur[0] = id;
					// TODO: Warn Properties panel
				}
			}
			return false;
		}.bind(this));

		tr.injectInside(this.table);
		this.altRow();
		// Apply filter
		//this.applyFilterOnRow(tr, status);
	},
	
	selectAll: function() {
		this.cur.empty();
		for (var id in this.rows) {
			this.cur[this.cur.length] = id;
			temptr = this.rows[id];
			if(temptr){
				if(!temptr.hasClass('selected')) {
					temptr.addClass('selected');
				}
			}
		}
	},

	updateRow: function(id, row, status){
		var tr = this.rows[id];
		if($defined(tr))
		{
			// Apply filter
			this.applyFilterOnRow(tr, status);
			var tds = tr.getElements('td');
			for(var i=0; i<row.length; i++) {
        	                if(i==this.progressIndex) {
					tds[i].set('html', '');
                	                tds[i].adopt(new ProgressBar(row[i].toFloat(), {width:80}));
                        	} else {
                                	tds[i].set('html', row[i]);
	                        }
                	};
			return true;
		}
		return false;
	},

	removeRow: function(id){
		if(this.cur.contains(id))
		{
			this.cur.erase(id);
		}
		var tr = this.rows[id];
		if($defined(tr))
		{
			tr.dispose();
			this.altRow();
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
		for (var id in this.rows) {
			ids[i] = id;
			i++
		}
		return ids;
	}
});
//dynamicTable.implement(new Options);
//dynamicTable.implement(new Events);

/*************************************************************/
