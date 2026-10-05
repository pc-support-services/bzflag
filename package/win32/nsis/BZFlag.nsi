;NSIS Modern User Interface version 1.69
;Original templates by Joost Verburg
;Redesigned for BZFlag by blast007
;Rewritten 2026-10 for the fork's CI mingw64 layout: the installer is
;compiled from a staging dir (pkg\) that CI populates from a mingw64
;build -- exes + all runtime DLLs + data\ flat side by side. The old
;MSVC bin_Release_${PLATFORM} layout is gone. CI defines BUILD_64.
;The CI "Build NSIS" step in .github/workflows/build-check.yml invokes
;THIS script; keep it in sync with that workflow's staging.

;--------------------------------
;BZFlag Version Variables

  !define VER_MAJOR 2
  !define VER_MINOR 4
  !define VER_REVISION 31

  !define TYPE "devel"

  !define TYPE_REVISION "0"

;--------------------------------
;Includes

  ; Modern UI
  !include "MUI2.nsh"

  ; Windows Version Detection
  !include "WinVer.nsh"

;--------------------------------
;Automatically generated version variables

  !if ${TYPE} != "release"
	!if ${TYPE} != "devel"
		!define VERSION_TAIL "-${TYPE}${TYPE_REVISION}"
	!else
		!define VERSION_TAIL ""
	!endif
    !define /date VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_REVISION}.%Y%m%d${VERSION_TAIL}"
  !else
    !define VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_REVISION}"
  !endif

  ; Bitness: define BUILD_64 at compile time for x64 packages
  ; (mingw64 toolchain = x64; pass on the makensis command line as
  ; `-DBUILD_64`). Default in the absence of the define is 32-bit.
  !ifdef BUILD_64
    !define PLATFORM x64
    !define BITNESS 64Bit
  !else
    !define PLATFORM Win32
    !define BITNESS 32Bit
  !endif

;--------------------------------
;Compression options

  SetCompress auto
  SetCompressor /SOLID lzma

;--------------------------------
;Configuration

  ; Installer output file; the staging dir (pkg\) sits next to this
  ; script, exactly as CI assembles it. Name is STABLE (no ${VERSION})
  ; because CI's uploader/release references the exact file name
  ; bzflag-windows-x64-setup.exe — do not make it date-dependent.
  Name "BZFlag ${VERSION} ${BITNESS}"
  OutFile "bzflag-windows-x64-setup.exe"
  InstallDir "$PROGRAMFILES64\BZFlag"

  ; Make it look pretty in XP
  XPStyle on

  ; The installer needs administrative rights
  RequestExecutionLevel admin

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER

;--------------------------------
;Interface Settings

  ;Icons: NSIS defaults; the repo no longer ships MSVC/bzflag.ico
  !define MUI_UNICON uninstall.ico

  ;Bitmaps
  !define MUI_WELCOMEFINISHPAGE_BITMAP "side.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "side.bmp"

  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "header.bmp"
  !define MUI_COMPONENTSPAGE_CHECKBITMAP "${NSISDIR}\Contrib\Graphics\Checks\simple-round2.bmp"

  !define MUI_COMPONENTSPAGE_SMALLDESC

  ;Show a warning before aborting install
  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  ;Welcome page configuration
  !define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of BZFlag ${VERSION} ${BITNESS}.$\r$\n$\r$\nBZFlag is a free multiplayer multiplatform 3D tank battle game. The name stands for Battle Zone capture Flag. It runs on Irix, Linux, *BSD, Windows, Mac OS X and other platforms. It's one of the most popular games ever on Silicon Graphics machines.$\r$\n$\r$\nClick Next to continue."

  !insertmacro MUI_PAGE_WELCOME
  !define MUI_LICENSEPAGE_TEXT_TOP "License"
  !insertmacro MUI_PAGE_LICENSE "copying.rtf"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\BZFlag ${VERSION} ${BITNESS}"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER

  !insertmacro MUI_PAGE_INSTFILES

  ;Finished page configuration
  !define MUI_FINISHPAGE_NOAUTOCLOSE

  !define MUI_FINISHPAGE_RUN
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_FINISHPAGE_RUN_TEXT "Play BZFlag now!"
  !define MUI_FINISHPAGE_RUN_FUNCTION "LaunchLink"

  !define MUI_FINISHPAGE_SHOWREADME "https://www.bzflag.org/documentation/getting_started"
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "Read Getting Started"
  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

  !define MUI_FINISHPAGE_LINK "BZFlag Home Page"
  !define MUI_FINISHPAGE_LINK_LOCATION "https://www.bzflag.org"

  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !define MUI_UNFINISHPAGE_NOAUTOCLOSE
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "!BZFlag (Required)" BZFlag
  ;Make it required
  SectionIn RO

  ; CI stages the flat pkg\ tree (exes + DLLs + data\); install it all.
  SetOutPath $INSTDIR
  File /r pkg\*.*

  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\BZFlag ${VERSION}" "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  !define UNINSTALL_REG_ROOT "Software\Microsoft\Windows\CurrentVersion\Uninstall\BZFlag ${VERSION} ${BITNESS}"
  WriteRegStr HKLM "${UNINSTALL_REG_ROOT}" "DisplayName" "BZFlag ${VERSION} ${BITNESS}"

  WriteRegStr HKLM "${UNINSTALL_REG_ROOT}" "DisplayIcon" "$INSTDIR\bzflag.exe"
  ; We're roughly 30MB installed
  WriteRegDWORD HKLM "${UNINSTALL_REG_ROOT}" "EstimatedSize" 30720
  WriteRegStr HKLM "${UNINSTALL_REG_ROOT}" "HelpLink" "https://www.bzflag.org/"
  WriteRegStr HKLM "${UNINSTALL_REG_ROOT}" "Comments" "Online multiplayer tank battle game"
  WriteRegDWORD HKLM "${UNINSTALL_REG_ROOT}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNINSTALL_REG_ROOT}" "NoModify" 1
  WriteRegStr HKLM "${UNINSTALL_REG_ROOT}" "UninstallString" '"$INSTDIR\uninstall.exe"'

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    ;Create for all users
    SetShellVarContext all

    ;Main start menu shortcuts
    SetOutPath $INSTDIR
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\BZFlag ${VERSION}.lnk" "$INSTDIR\bzflag.exe" "" "$INSTDIR\bzflag.exe" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\BZFlag ${VERSION} (800x600 Windowed).lnk" "$INSTDIR\bzflag.exe"  "-window 800x600" "$INSTDIR\bzflag.exe" 0

    ; Local User Data
    Var /GLOBAL UserData
    StrCpy $UserData "%LOCALAPPDATA%\BZFlag"

    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Browse User Data.lnk" "$UserData"

    ; Server shortcuts (bzfs ships in the flat pkg\ tree; CI disables
    ; plugins, so no plugin/config sections here)
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER\Server"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Server\Start Server (Simple Jump Teleport 1 shot).lnk" "$INSTDIR\bzfs.exe" "-p 5154 -j -t -s 32 +s 16 -h" "$INSTDIR\bzflag.exe" 0
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Server\Start Server (Simple Jump Teleport 3 shots).lnk" "$INSTDIR\bzfs.exe" "-p 5154 -j -t -ms 3 -s 32 +s 16 -h" "$INSTDIR\bzflag.exe" 0

  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

Section "Desktop Icon" Desktop
  ; Install for all users
  SetShellVarContext all

  ;shortcut on the "desktop"
  SetOutPath $INSTDIR
  CreateShortCut "$DESKTOP\BZFlag ${VERSION} ${BITNESS}.lnk" "$INSTDIR\bzflag.exe" "" "$INSTDIR\bzflag.exe" 0
SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_BZFlag ${LANG_ENGLISH} "Installs the main 3D application, the server, and the associated data files."
  LangString DESC_Desktop ${LANG_ENGLISH} "Adds a shortcut on the desktop."

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${BZFlag} $(DESC_BZFlag)
    !insertmacro MUI_DESCRIPTION_TEXT ${Desktop} $(DESC_Desktop)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ;Remove for all users
  SetShellVarContext all

  ; remove files (flat layout: exes + DLLs at top level, data\ beside them)
  Delete $INSTDIR\*.*
  Delete $INSTDIR\data\*.*

  ; MUST REMOVE UNINSTALLER, too
  Delete $INSTDIR\uninstall.exe

  ; remove directories used.
  RMDir /r "$INSTDIR\data"
  RMDir "$INSTDIR"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP

  ;remove shortcuts, if any.
  Delete "$SMPROGRAMS\$MUI_TEMP\*.*"
  Delete "$SMPROGRAMS\$MUI_TEMP\Server\*.*"
  RMDir "$SMPROGRAMS\$MUI_TEMP\Server"
  RMDir "$SMPROGRAMS\$MUI_TEMP"

  ;Delete empty start menu parent directories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"

  startMenuDeleteLoop:
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."

    IfErrors startMenuDeleteLoopDone

    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:

  ; Remove desktop shortcut
  Delete "$DESKTOP\BZFlag ${VERSION} ${BITNESS}.lnk"

  ;remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BZFlag ${VERSION} ${BITNESS}"
  DeleteRegKey HKLM "SOFTWARE\BZFlag ${VERSION} ${BITNESS}"
  DeleteRegKey HKLM "SOFTWARE\BZFlag ${VERSION}"
  ; This deletes a key that stored the current running path of BZFlag, which was/is used by Xfire
  DeleteRegKey HKCU "Software\BZFlag"

SectionEnd

Function LaunchLink
  ExecShell "" "$INSTDIR\bzflag.exe"
FunctionEnd

Function .onInit
  ${If} ${AtMostWinXP}
    MessageBox MB_OK "BZFlag requires Windows Vista or higher."
    Quit
  ${EndIf}
FunctionEnd