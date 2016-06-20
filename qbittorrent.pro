TEMPLATE = subdirs

SUBDIRS += src

include(version.pri)
include(qm_gen.pri)

# Make target to create release tarball. Use 'make tarball'
tarball.commands += rm -fR ../$${PROJECT_NAME}-$${PROJECT_VERSION}/ &&
tarball.commands += git clone . ../$${PROJECT_NAME}-$${PROJECT_VERSION} &&
tarball.commands += rm -fR ../$${PROJECT_NAME}-$${PROJECT_VERSION}/.git &&
tarball.commands += rm -f ../$${PROJECT_NAME}-$${PROJECT_VERSION}/.gitignore &&
tarball.commands += cd .. &&
tarball.commands += tar czf $${PROJECT_NAME}-$${PROJECT_VERSION}.tar.gz $${PROJECT_NAME}-$${PROJECT_VERSION} &&
tarball.commands += tar cf $${PROJECT_NAME}-$${PROJECT_VERSION}.tar $${PROJECT_NAME}-$${PROJECT_VERSION} &&
tarball.commands += xz -f $${PROJECT_NAME}-$${PROJECT_VERSION}.tar &&
tarball.commands += rm -fR $${PROJECT_NAME}-$${PROJECT_VERSION}

QMAKE_EXTRA_TARGETS += tarball
