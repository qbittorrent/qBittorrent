module.exports = function(grunt) {
 
	grunt.initConfig({
		svg2png: {
			all: {
				files: [
					{ 
						src: ['icons/*.svg']
					}
				]
			}
		}
	});

	grunt.loadNpmTasks('grunt-svg2png');
	
	grunt.registerTask('default', ['svg2png']);

}