TS_IN = $$fromfile(src/src.pro,TRANSLATIONS)
TS_IN_NOEXT = $$replace(TS_IN,".ts","")
             
isEmpty(QMAKE_LRELEASE) {
    win32|os2:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease-qt4 }
    } else {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
    }
}

message("Building translations")
for(L,TS_IN_NOEXT) {
    message("Processing $${L}")
    system("$$QMAKE_LRELEASE -silent src/$${L}.ts -qm src/$${L}.qm")
    !exists("src/$${L}.qm"):error("Building translations failed, cannot continue")
}
