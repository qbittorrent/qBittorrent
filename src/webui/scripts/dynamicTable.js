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
	
	initialize: function(table, options){
		this.setOptions({
			overCls: false,
			selectCls: false,
			altCls: false
		}, options);
		this.table = $(table);
		this.rows = new Object();
		this.cur = new Array();
	},

	altRow: function()
	{
		var trs = this.table.getElements('tr');
		if(this.options.altCls)
		{
			trs.each(function(el,i){
				if(i % 2){
					el.addClass(this.options.altCls);
				}else{
					el.removeClass(this.options.altCls);
				}
			}.bind(this));
		}
	},

	insertRow: function(id, row){
		var tr = this.rows[id];
		if($defined(tr))
		  return;
		//this.removeRow(id);
		var tr = new Element('tr');
		this.rows[id] = tr;
		for(var i=0; i<row.length; i++)
		{
			var td = new Element('td');
			td.set('html', row[i]);
			td.injectInside(tr);
		};

		if(this.options.overCls){
			tr.addEvent('mouseover', function(e){
				tr.addClass(this.options.overCls);
			}.bind(this));
			tr.addEvent('mouseout', function(e){
				tr.removeClass(this.options.overCls);
			}.bind(this));
		}
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
				    temptr.removeClass(this.options.selectCls);
			    }
				} else {
					this.cur[this.cur.length] = id;
					// Add selected style
					temptr = this.rows[id];
					if(temptr){
						temptr.addClass(this.options.selectCls);
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
							temptr.addClass(this.options.selectCls);
						}
					}
				} else {
					// Simple selection
					// Remove selected style from previous ones
					for(i=0; i<this.cur.length; i++) {
						var temptr = this.rows[this.cur[i]];
						if(temptr){
							temptr.removeClass(this.options.selectCls);
						}
					}
					this.cur.empty();
					// Add selected style to new one
					temptr = this.rows[id];
					if(temptr){
						temptr.addClass(this.options.selectCls);
					}
					this.cur[0] = id;
				}
			}
			return false;
		}.bind(this));

		tr.injectInside(this.table);
		this.altRow();
	},
	
	selectAll: function() {
		this.cur.empty();
		for (var id in this.rows) {
			this.cur[this.cur.length] = id;
			temptr = this.rows[id];
			if(temptr){
				if(!temptr.hasClass(this.options.selectCls)) {
					temptr.addClass(this.options.selectCls);
				}
			}
		}
	},

	updateRow: function(id, row){
		var tr = this.rows[id];
		if($defined(tr))
		{
			var tds = tr.getElements('td');
			row.each(function(el, i){
                                if(tds[i].get('html') != el) {
				      tds[i].set('html', el);
                                }
			});
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
dynamicTable.implement(new Options);
dynamicTable.implement(new Events);

/*************************************************************/
