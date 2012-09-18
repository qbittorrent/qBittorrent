You will need NSIS and upx to make the installer.

Open the qbittorrent.nsi file in an editor and change line that contains "!define PROG_VERSION "3.0.3"" to the version of qbittorrent you just built. 

The installer script expects the following file tree:

Root:
translations
	qt_ar.qm
	qt_bg.qm
	qt_ca.qm
	qt_cs.qm
	qt_da.qm
	qt_de.qm
	qt_es.qm	
	qt_fi.qm
	qt_fr.qm
	qt_gl.qm
	qt_he.qm
	qt_hu.qm
	qt_it.qm
	qt_ja.qm
	qt_ko.qm
	qt_lt.qm
	qt_nl.qm
	qt_pl.qm
	qt_pt.qm
	qt_pt_BR.qm
	qt_ru.qm
	qt_sk.qm	
	qt_sv.qm
	qt_tr.qm
	qt_uk.qm
	qt_zh_CN.qm
	qt_zh_TW.qm
license.txt
qbittorrent.exe
qbittorrent.nsi
qt.conf


license.txt is a text file that contains the text rendered from src\gpl.html
