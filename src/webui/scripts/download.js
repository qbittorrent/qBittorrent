window.addEvent('domready', function(){
	$('downButton').addEvent('click', function(e){
		new Event(e).stop();
		new Ajax('/command/download', {method: 'post', data: {urls: $('urls').value},
			onComplete: function() {
				window.parent.document.getElementById('downloadPage').parentNode.removeChild(window.parent.document.getElementById('downloadPage'));
			}
		}).request();
	});
});
