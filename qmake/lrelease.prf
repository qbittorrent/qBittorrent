# Automatically generate .qm files out of .ts files in TRANSLATIONS and
# EXTRA_TRANSLATIONS.
#
# If embed_translations is enabled, the generated .qm files are made available
# in the resource system under :/i18n/.
#
# Otherwise, the .qm files are available in the build directory in LRELEASE_DIR.
# They can also be automatically installed by setting QM_FILES_INSTALL_PATH.

qtPrepareTool(QMAKE_LRELEASE, lrelease)

isEmpty(LRELEASE_DIR): LRELEASE_DIR = .qm
isEmpty(QM_FILES_RESOURCE_PREFIX): QM_FILES_RESOURCE_PREFIX = i18n

lrelease.name = lrelease
lrelease.input = TRANSLATIONS EXTRA_TRANSLATIONS
lrelease.output = $$LRELEASE_DIR/${QMAKE_FILE_IN_BASE}.qm
lrelease.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} $$QMAKE_LRELEASE_FLAGS -qm ${QMAKE_FILE_OUT}
silent: lrelease.commands = @echo lrelease ${QMAKE_FILE_IN} && $$lrelease.commands
lrelease.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += lrelease

all_translations = $$TRANSLATIONS $$EXTRA_TRANSLATIONS
for (translation, all_translations) {
    # mirrors $$LRELEASE_DIR/${QMAKE_FILE_IN_BASE}.qm above
    translation = $$basename(translation)
    QM_FILES += $$OUT_PWD/$$LRELEASE_DIR/$$replace(translation, \\..*$, .qm)
}
embed_translations {
    qmake_qm_files.files = $$QM_FILES
    qmake_qm_files.base = $$OUT_PWD/$$LRELEASE_DIR
    qmake_qm_files.prefix = $$QM_FILES_RESOURCE_PREFIX
    RESOURCES += qmake_qm_files
} else {
    !isEmpty(QM_FILES_INSTALL_PATH) {
        qm_files.files = $$QM_FILES
        qm_files.path = $$QM_FILES_INSTALL_PATH
        INSTALLS += qm_files
    }
    lrelease.CONFIG += target_predeps no_clean
}
