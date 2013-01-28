TEMPLATE = subdirs

SUBDIRS += src

include(version.pri)
include(qm_gen.pri)

# Dist
dist.commands += rm -fR ../$${PROJECT_NAME}-$${PROJECT_VERSION}/ &&
dist.commands += git clone . ../$${PROJECT_NAME}-$${PROJECT_VERSION} &&
dist.commands += rm -fR ../$${PROJECT_NAME}-$${PROJECT_VERSION}/.git &&
dist.commands += rm -f ../$${PROJECT_NAME}-$${PROJECT_VERSION}/.gitignore &&
dist.commands += cd .. &&
dist.commands += tar czf $${PROJECT_NAME}-$${PROJECT_VERSION}.tar.gz $${PROJECT_NAME}-$${PROJECT_VERSION} &&
dist.commands += tar cf $${PROJECT_NAME}-$${PROJECT_VERSION}.tar $${PROJECT_NAME}-$${PROJECT_VERSION} &&
dist.commands += xz -f $${PROJECT_NAME}-$${PROJECT_VERSION}.tar &&
dist.commands += rm -fR $${PROJECT_NAME}-$${PROJECT_VERSION}

QMAKE_EXTRA_TARGETS += dist
