# WebUI Translation
isEmpty(QMAKE_LRELEASE) {
    win32: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease.exe
    else: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
        equals(QT_MAJOR_VERSION, 5) {
            !exists($$QMAKE_LRELEASE): QMAKE_LRELEASE = lrelease-qt5
        }
    }
    else {
        !exists($$QMAKE_LRELEASE): QMAKE_LRELEASE = lrelease
    }
}

WEBUI_TRANSLATIONS = $$files(webui_*.ts)
WEBUI_TRANSLATIONS_NOEXT = $$replace(WEBUI_TRANSLATIONS, ".ts", "")
message("Building WebUI translations...")
for(L, WEBUI_TRANSLATIONS_NOEXT) {
    message("Processing $${L}")
    system("$$QMAKE_LRELEASE -silent $${L}.ts -qm $${L}.qm")
    !exists("$${L}.qm"): error("Building WebUI translations failed, cannot continue!")
}
