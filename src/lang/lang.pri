TRANSLATIONS += $$files(qbittorrent_*.ts)
TS_IN_NOEXT = $$replace(TRANSLATIONS,".ts","")

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
      equals(QT_MAJOR_VERSION, 5) {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease-qt5 }
      }
    } else {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
    }
}

message("Building translations")
for(L,TS_IN_NOEXT) {
    message("Processing $${L}")
    system("$$QMAKE_LRELEASE -silent $${L}.ts -qm $${L}.qm")
    !exists("$${L}.qm"):error("Building translations failed, cannot continue")
}
