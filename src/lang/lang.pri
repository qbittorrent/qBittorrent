TS_FILES += $$files(qbittorrent_*.ts)

# need to use full path, otherwise running
# `lupdate` will generate *.ts files in project root directory
for(file, TS_FILES) {
    TRANSLATIONS += "$${PWD}/$${file}"
}

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

message("Building translations")
TS_FILES_NOEXT = $$replace(TS_FILES, ".ts", "")
for(file, TS_FILES_NOEXT) {
    message("Processing $${file}")
    system("$$QMAKE_LRELEASE -silent $${file}.ts -qm $${file}.qm")
    !exists("$${file}.qm"):error("Building translations failed, cannot continue")
}
