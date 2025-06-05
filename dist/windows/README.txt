TRANSLATORS:

1. Use an editor that has NSIS syntax highlighting(eg Notepad++/Geany). This will
   make your life easier.
2. Open the relevant .nsi file that exists in the folder named
   "installer-translations"
3. Lines starting with ";" are considered comments. These include the
   english message to help you with the translation.
4. Edit only the part inside the quotation marks(""). Unless you know
   what you are doing.
5. Save the files with UTF-8 encoding, without BOM
   (this should be the default in any modern text editor).
6. Submit your changes: 1) as a pull request to the official git repo or
   2) open an issue to the bugtracker and attach them or 3) via email or
   4)the same way you provide the translations for qbt itself

PACKAGERS:

You will need NSIS 3 to make the installer. UPX is an optional requirement.

1. Open the config.nsi file in an editor and change line that contains
   "!define QBT_VERSION "3.0.3"" to the version of qbittorrent you just built.
2. config.nsi contains some other defines that control the installer output. Read the comments in that file.
3. Extract "NSISPlugins.zip" next to "qbittorrent.nsi" script or into your
   NSIS's unicode Plugin directory (usually "C:\Program Files\NSIS\Plugins\x86-unicode").
4. The script you need to compile is "qbittorrent.nsi". It includes all other necessary scripts.
5. The script expects the following file tree:

installer-translations
    afrikaans.nsh
    ....
    (all the .nsh files found here in every source release)
    welsh.nsh
installer.nsh
license.txt
config.nsh
qbittorrent.nsi
qt.conf
translations.nsh
UAC.nsh
uninstaller.nsh
qBittorrent
    qbittorrent.exe (required)
    qbittorrent.pdb (optional but recommended)
    translations (optional)
        qt_ar.qm
        ..
        (All the .qm files found in the 'translations' folder of your Qt install. Those files differ between Qt5 and Qt6.
         You will need the files that conform to this globbing expression 'qt_??.qm qt_??_??.qm qtbase_??.qm qtbase_??_??.qm'.
         Some of those files will be stubs. Filter any file that is smaller than 10KB in size.
         Alternatively you can use the 'gather_qt_translations.py' script found in the same folder as this file.
         Run it with '--help' to see its usage.)
        qt_zh_TW.qm
    (Any other files including the ones from all subdirectories will be also included in the installer.)

6. "qbittorrent.exe" is the compiled binary file.
7. "qbittorrent.pdb" is the compiled binary's PDB file.

SCRIPT HACKERS:

If you add any new LangString variable to the scripts you NEED to provide
"translations" of it to all the .nsi files inside "installer-translations.
You can always leave the english string but you have to use all the LANG_<lang name>
for the given variable. Otherwise, if the user chooses a language that you
haven't provided a LANG_<lang name> for your variable then your string will be empty.
Don't worry though, NSIS throws warnings for this when compiling the scripts.
