HEADERS += $$TRAYICON_CPP/trayicon.h
SOURCES += $$TRAYICON_CPP/trayicon.cpp
QT += xml

unix:!mac {
	SOURCES += $$TRAYICON_CPP/trayicon_x11.cpp
}
win32: {
	SOURCES += $$TRAYICON_CPP/trayicon_win.cpp
	win32-g++: {
	# Probably MinGW
		LIBS += libgdi32 libuser32 libshell32
	}
	else {
	# Assume msvc compiler
		LIBS += Gdi32.lib User32.lib shell32.lib
	}
}
mac: {
	SOURCES += $$TRAYICON_CPP/trayicon_mac.cpp
}
