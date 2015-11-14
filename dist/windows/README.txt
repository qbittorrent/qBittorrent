TRANSLATORS:

1. Use an editor that has NSIS syntax highlighting(eg Notepad++/Geany). This will 
   make your life easier.
2. Open the relevant .nsi file that exists in the folder named 
   "installer-translations"
3. Lines starting with ";" are considered comments. These include the 
   english message to help you with the translation.
4. Edit only the part inside the quotation marks(""). Unless you know 
   what you are doing.
5. Save the files with utf8 encoding and BOM.
6. Submit your changes: 1) as a pull request to the official git repo or
   2) open an issue to the bugtracker and attach them or 3) via email or
   4)the same way you provide the translations for qbt itself

PACKAGERS:

You will need NSIS and upx to make the installer. You need a unicode version of NSIS.
I tested with NSIS 3.0a0. 

1. Open the options.nsi file in an editor and change line that contains
   "!define PROG_VERSION "3.0.3"" to the version of qbittorrent you just built. 
2. Extract the plugins found in the folder "nsis plugins" into your 
   NSIS's unicode Plugin directory(usually C:\Program Files\NSIS\Plugins\x86-unicode).
   Only the *.dll files are needed. Don't use the .dll from UAC.zip, use the one from "UAC Unicode.zip".
   NOTE: To build the unicode version of UAC with MSVC2008 you need:
    a) the sources from UAC.zip
    b) apply the util.cpp.diff from "UAC Unicode.zip" to util.cpp
    c) in a msvc command prompt issue: cl.exe /O1s /GS- /GR- /EHs-c- /Zl /LD /DUNICODE RunAs.cpp uac.cpp util.cpp /link kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib /DLL /MANIFEST:NO /OUT:uac.dll 
3. The script you need to compile is "qbittorrent.nsi". It includes all other necessary scripts.
4. The script expects the following file tree:

The installer script expects the following file tree:

Root:
installer-translations
	afrikaans.nsi
	....
	(all the .nsi files found here in every source release)
	welsh.nsi
translations
	qt_ar.qm
	...
        (all the .qm files found in dist/qt-translations in every source release)
	qt_zh_TW.qm
installer.nsi
license.txt
options.nsi
qbittorrent.exe
qbittorrent.nsi
qt.conf
translations.nsi
uninstaller.nsi


5. "license.txt" is a text file that contains the text rendered 
   from src\gui\gpl.html or the text contained in COPYING
6. "qbittorrent.exe" is the compiled binary file.

SCRIPT HACKERS:

If you add any new LangString variable to the scripts you NEED to provide
"translations" of it to all the .nsi files inside "installer-translations.
You can always leave the english string but you have to use all the LANG_<lang name> 
for the given variable. Otherwise, if the user chooses a language that you 
haven't provided a LANG_<lang name> for your variable then your string will be empty. 
Don't worry though, NSIS throws warnings for this when compiling the scripts.
