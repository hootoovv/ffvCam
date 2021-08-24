; Script generated with the Venis Install Wizard

; Define your application name
!define APPNAME "ffvCam"
!define APPNAMEANDVERSION "ffvCam 1.0"

; Main Install settings
Name "${APPNAMEANDVERSION}"
InstallDir "$PROGRAMFILES64\ffvCam"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
OutFile "ffvCam_Installer.exe"

Caption "ffvCam Intallation"
Icon ".\install.ico"
UninstallIcon ".\install.ico"
SetDateSave on
SetDatablockOptimize on
SetPluginUnload manual
CRCCheck off
BGGradient FFFFFF 000080 000000
BrandingText "(C)2021, Kevin Wu"

; Modern interface settings
!include "MUI.nsh"
!include "x64.nsh"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

;--------------------------------
;Version Information

  VIProductVersion "1.0.0.1"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "ffvCam"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "Virtual Camera based on ffmpeg"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "exlearn"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalTrademarks" "exlearn"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(C)2021, Kevin Wu"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Virtual Camera based on ffmpeg"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "1.0.1"

Section "ffvCam" ffvCam

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\"
	File "out\avcodec-58.dll"
	File "out\avdevice-58.dll"
	File "out\avfilter-7.dll"
	File "out\avformat-58.dll"
	File "out\avutil-56.dll"
	File "out\postproc-55.dll"
	File "out\swresample-3.dll"
	File "out\swscale-5.dll"
	File "out\ffvCam.dll"
	File "out\AmCap_x64.exe"

SectionEnd

Section -FinishSection
	nsExec::ExecToLog /TIMEOUT=5000 '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\ffvCam.dll"'

	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${ffvCam} ""
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section Uninstall

	nsExec::ExecToLog /TIMEOUT=5000 '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\ffvCam.dll"'

	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Clean up Virtual Camera Streaming Push Service
	Delete "$INSTDIR\avcodec-58.dll"
	Delete "$INSTDIR\avdevice-58.dll"
	Delete "$INSTDIR\avfilter-7.dll"
	Delete "$INSTDIR\avformat-58.dll"
	Delete "$INSTDIR\avutil-56.dll"
	Delete "$INSTDIR\postproc-55.dll"
	Delete "$INSTDIR\swresample-3.dll"
	Delete "$INSTDIR\swscale-5.dll"
	Delete "$INSTDIR\ffvCam.dll"
	Delete "$INSTDIR\AmCap_x64.exe"

	; Remove remaining directories
	RMDir "$INSTDIR\"

SectionEnd

; On initialization
Function .onInit

	;!insertmacro MUI_LANGDLL_DISPLAY
	
	${DisableX64FSRedirection}

FunctionEnd


; eof