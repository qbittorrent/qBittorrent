HEADERS += $$PWD/powermanagement.h
SOURCES += $$PWD/powermanagement.cpp

unix:!macx:dbus {
  HEADERS += $$PWD/powermanagement_x11.h
  SOURCES += $$PWD/powermanagement_x11.cpp
}
