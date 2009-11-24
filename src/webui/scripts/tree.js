/*

Script: Tree.js
	Create folder trees.

Copyright:
	Copyright (c) 2007-2008 Greg Houston, <http://greghoustondesign.com/>.	

License:
	MIT-style license.	

*/

function buildTree(treeID){

	$$('#'+treeID+' li.folder').each(function(folder){
		var folderContents = folder.getChildren('ul');
		var folderImage = new Element('img', {
			'src': '../images/tree/_open.gif',
			'width': 18,
			'height': 18
		}).inject(folder, 'top');

		// Determine which open and close graphic each folder gets
		
		if (folder.hasClass('root')) {
			folder.minus = '../images/tree/Rminus.gif'
			folder.plus = '../images/tree/Rplus.gif'
		}
		else 
			if (folder.getNext()) {
				folder.minus = '../images/tree/Tminus.gif'
				folder.plus = '../images/tree/Tplus.gif'
			}
			else {
				folder.minus = '../images/tree/Lminus.gif'
				folder.plus = '../images/tree/Lplus.gif'
			}
		
		var image = new Element('img', {
			'src': folder.minus,
			'width': 18,
			'height': 18
		}).addEvent('click', function(){
			if (folder.hasClass('f-open')) {
				image.setProperty('src', folder.plus);
				folderImage.setProperty('src', '../images/tree/_closed.gif');
				folderContents.each(function(el){
					el.setStyle('display', 'none');
				});
				folder.removeClass('f-open');
			}
			else {
				image.setProperty('src', folder.minus);
				folderImage.setProperty('src', '../images/tree/_open.gif');
				folderContents.each(function(el){
					el.setStyle('display', 'block');
				});
				folder.addClass('f-open');
			}
		}).inject(folder, 'top');
		
		if (!folder.hasClass('f-open')) {
			image.setProperty('src', folder.plus);
			folderContents.each(function(el){
				el.setStyle('display', 'none');
			});
			folder.removeClass('f-open');
		}

		// Add connecting branches to each file node

		folderContents.each(function(element){
			var docs = element.getChildren('li.doc');
			docs.each(function(el){
				if (el == docs.getLast() && !el.getNext()) {
					new Element('img', {
						'src': '../images/tree/L.gif',
						'width': 18,
						'height': 18
					}).inject(el.getElement('span'), 'before');
				}
				else {
					new Element('img', {
						'src': '../images/tree/T.gif',
						'width': 18,
						'height': 18
					}).inject(el.getElement('span'), 'before');
				}
			});
		});
		
	});
	
	// Add connecting branches to each node

	$$('#'+treeID+' li').each(function(node){
		node.getParents('li').each(function(parent){
			if (parent.getNext()) {
				new Element('img', {
					'src': '../images/tree/I.gif',
					'width': 18,
					'height': 18
				}).inject(node, 'top');
			}
			else {
				new Element('img', {
					'src': 'images/spacer.gif',
					'width': 18,
					'height': 18
				}).inject(node, 'top');
			}
		});
	});

	$$('#'+treeID+' li.doc').each(function(el){
		new Element('img', {
			'src': '../images/tree/_doc.gif',
			'width': 18,
			'height': 18
		}).inject(el.getElement('span'), 'before');
	});
	
}
