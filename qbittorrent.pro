TEMPLATE = subdirs

SUBDIRS += src

include(version.pri)

# For Qt Creator beautifier
DISTFILES += \
    uncrustify.cfg
