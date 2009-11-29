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
		this.table.sortedIndex = 1; // Default is NAME
		this.table.reverseSort = false;
	},
	
	sortfunction: function(tr1, tr2) {
		var i = tr2.getParent().sortedIndex;
		var reverseSort = tr2.getParent().reverseSort;
		switch(i) {
			case 1: // Name
				if(!reverseSort) {
					if(tr1.getElements('td')[i].get('html') >  tr2.getElements('td')[i].get('html'))
						return 1;
					return -1;
				} else {
					if(tr1.getElements('td')[i].get('html') <  tr2.getElements('td')[i].get('html'))
						return 1;
					return -1;
				}
			case 2: // Prio
				var prio1 = tr1.getElements('td')[i].get('html');
				if(prio1 == '*') prio1 = '-1';
				var prio2 = tr2.getElements('td')[i].get('html');
				if(prio2 == '*') prio2 = '-1';
				if(!reverseSort)
					return (prio1.toInt() -  prio2.toInt());
				else
					return (prio2.toInt() -  prio1.toInt());
			case 3: // Size
			case 7: // Up Speed
			case 8: // Down Speed
				var sizeStrToFloat = function(mystr) {
					var val1 = mystr.split(' ');
					var val1num = val1[0].toFloat()
					var unit = val1[1].capitalize();
					switch(unit[0]) {
						case 'G':
							return val1num*1073741824;
						case 'M':
							return val1num*1048576;
						case 'K':
							return val1num*1024;
						default:
							return val1num;
					}
				};
				if(!reverseSort)
					return (sizeStrToFloat(tr1.getElements('td')[i].get('html')) - sizeStrToFloat(tr2.getElements('td')[i].get('html')));
				else
					return (sizeStrToFloat(tr2.getElements('td')[i].get('html')) - sizeStrToFloat(tr1.getElements('td')[i].get('html')));
			case 4: // Progress
				if(!reverseSort)
					return (tr1.getElements('td')[i].getChildren()[0].getValue() - tr2.getElements('td')[i].getChildren()[0].getValue());
				else
					return (tr2.getElements('td')[i].getChildren()[0].getValue() - tr1.getElements('td')[i].getChildren()[0].getValue());
			case 5: // Seeds
			case 6: // Peers
				if(!reverseSort)
					return (tr1.getElements('td')[i].get('html').split(' ')[0].toInt() - tr2.getElements('td')[i].get('html').split(' ')[0].toInt());
				else
					return (tr2.getElements('td')[i].get('html').split(' ')[0].toInt() - tr1.getElements('td')[i].get('html').split(' ')[0].toInt());
			default: // Ratio
				var ratio1 = tr1.getElements('td')[i].get('html');
				if(ratio1 == '∞')
					ratio1 = '101.0';
				var ratio2 = tr2.getElements('td')[i].get('html');
				if(ratio2 == '∞')
					ratio2 = '101.0';
				if(!reverseSort)
					return (ratio1.toFloat() -  ratio2.toFloat());
				else
					return (ratio2.toFloat() -  ratio1.toFloat());
		}
	},
	
	updateSort: function() {
		var trs = this.table.getChildren('tr');
		trs.sort(this.sortfunction);
		this.table.set('html', '');
		this.table.adopt(trs);
	},
	
	setSortedColumn: function(index) {
		if(index != this.table.sortedIndex) {
			this.table.sortedIndex = index;
			this.table.reverseSort = false;
		} else {
			// Toggle sort order
			this.table.reverseSort = !this.table.reverseSort;
		}
		this.updateSort();
		this.altRow();
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
				break;
			case 'downloading':
				if(status == "downloading" || status == "stalledDL" || status == "checkingDL" || status == "pausedDL" || status == "queuedDL") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				break;
			case 'completed':
				if(status == "seeding" || status == "stalledUP" || status == "checkingUP" || status == "pausedUP" || status == "queuedUP") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				break;
			case 'active':
				if(status == "downloading" || status == "seeding") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
				break;
			case 'inactive':
				if(status != "downloading" && status != "seeding") {
					tr.removeClass("invisible");
				} else {
					tr.addClass("invisible");
				}
		}
		return !tr.hasClass('invisible');
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
				td.adopt(new ProgressBar(row[i].toFloat(), {'id': 'pb_'+id, 'width':80}));
			} else {
				if(i==0) {
					td.adopt(new Element('img', {'src':row[i], 'class': 'statusIcon'}));
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
		var trs = this.table.getChildren('tr');
		var i=0;
		while(i<trs.length && this.sortfunction(tr, trs[i]) > 0) {
			i++;
		}
		if(i==trs.length) {
			tr.inject(this.table);
		} else {
			tr.inject(trs[i], 'before');
		}
		//tr.injectInside(this.table);
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
		if(this.applyFilterOnRow(tr, status)) {
			var tds = tr.getElements('td');
			for(var i=0; i<row.length; i++) {
				if(i==1) continue; // Do not refresh name
				if(i==this.progressIndex) {
					$('pb_'+id).setValue(row[i].toFloat());
				} else {
					if(i==0) {
						tds[i].getChildren('img')[0].set('src', row[i]);
					} else {
						tds[i].set('html', row[i]);
					}
				}
			};
		}
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
