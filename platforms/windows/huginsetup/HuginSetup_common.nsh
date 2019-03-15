; Hugin installer script
!define VERSION 0.2.11.0
; Author: thePanz (thepanz@gmail.com)

; uninstaller definitions
!define APP_NAME "Hugin"
!define INSTDIR_REG_ROOT "HKLM"
!define INSTDIR_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

;--------------------------------
;Include Modern UI
  !include "MUI2.nsh"
;Include the Uninstall log header
  !include "Plugins\AdvUninstLog.nsh"

  !addplugindir "./Plugins"
;--------------------------------
;General  
  
  ; General CP Disclaimer
  ;!define CP_LICENSE_DISCLAIMER $(License_ControlPointDisclaimer)
  
  ;Name and file
  Name "Hugin ${HUGIN_VERSION}"
  BrandingText "Hugin Setup"
  
  OutFile "HuginSetup_${HUGIN_VERSION}-${HUGIN_VERSION_BUILD}_${ARCH_TYPE}bit.exe"
  SetCompressor /SOLID lzma
  SetCompressorDictSize 128

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

;--------------------------------
;Interface Configuration
  !define MUI_VERSION "${HUGIN_VERSION}-${HUGIN_VERSION_BUILD}"
  !define MUI_COMPONENTSPAGE_SMALLDESC ; Small description in Component selection

  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\orange.bmp" ; optional
  !define MUI_HEADERIMAGE_UNBITMAP "${NSISDIR}\Contrib\Graphics\Header\orange-uninstall.bmp" ; optional
  
  !define MUI_ICON "Graphics\hugin-installer-icon.ico"
  !define MUI_UNICON "Graphics\hugin-uninstaller-icon.ico" 
  
  !define MUI_ABORTWARNING
  
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_WELCOMEPAGE_TEXT "${DEV_WARNING_TOGGLE}"
  !define MUI_WELCOMEFINISHPAGE_BITMAP "Graphics\Hugin-sidebar.bmp"
  !define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "Graphics\Hugin-sidebar.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP_NOSTRETCH
  
  !define MUI_FINISHPAGE_RUN $INSTDIR\bin\hugin.exe
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_FINISHPAGE_LINK $(TEXT_FinishPageLink)
  !define MUI_FINISHPAGE_LINK_LOCATION "http://hugin.sourceforge.net"  
  
  ;uninstaller mode
  !insertmacro INTERACTIVE_UNINSTALL
;--------------------------------
;Pages
  ;Show all languages, despite user's codepage
  !define MUI_LANGDLL_ALLLANGUAGES

  !insertmacro MUI_PAGE_WELCOME
  ;!insertmacro MUI_PAGE_LICENSE $(License_Hugin)
  !insertmacro MUI_PAGE_LICENSE "Licenses\GPLv2.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_COMPONENTS
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH  
  
;--------------------------------
; Include Functions
  !include "Functions\RegistryFunctions.nsh"
  !include "Functions\DownloadFunctions.nsh"

;--------------------------------
; Installer Sections

; Core Hugin files
Section "!Hugin ${HUGIN_VERSION}-${HUGIN_VERSION_BUILD}" SecHugin
  SectionIn RO
  
  Call CleanRegistryOnInstallIfSelected

  DetailPrint $(TEXT_HuginExtracting)
  SetDetailsPrint listonly
   
  SetOutPath "$INSTDIR"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL
  CreateDirectory "$INSTDIR\bin"
  CreateDirectory "$INSTDIR\share"
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL
  
  SetDetailsPrint both
  SetOutPath "$INSTDIR\bin"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL
  File /r "FILES\bin\*.*"      ;bin folder
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL
  SetOutPath "$INSTDIR\share"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL
  File /r "FILES\share\*.*"    ;share folder
  
  ; Call AddCPAutoPanoSiftCStacked
  ;Call AddCPAlignImageStack
  
  ; register .pto files with hugin
  WriteRegStr HKCR ".pto" "" "HuginProject"
  WriteRegStr HKCR "HuginProject" "" "Hugin PTO"
  WriteRegStr HKCR "HuginProject\DefaultIcon" "" "$INSTDIR\share\hugin\xrc\data\pto_icon.ico,0"
  
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL
  
  Call WriteUninstallRegistry
SectionEnd

Section /o $(TEXT_SecCleanRegistrySettings) SecCleanRegistrySettings
  ; done during install
SectionEnd

; Hugin documentation section
Section $(TEXT_SecHuginDoc) SecHuginDoc
  SetOutPath "$INSTDIR"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL
  CreateDirectory "$INSTDIR\doc"
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

  DetailPrint $(TEXT_HuginDocExtracting)
  SetDetailsPrint listonly
  
  SetDetailsPrint both
  
  SetOutPath "$INSTDIR\doc"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL 
  File /r "FILES\doc\*.*"     ;doc folder
    
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL
SectionEnd

SectionGroup /e $(TEXT_SecShortcuts) SecShortcuts
  Section $(TEXT_SecShortcutPrograms) SecShortcutPrograms
    AddSize 1
    ; SetShellVarContext all
    SetOutPath "$INSTDIR\bin"
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "${UNINST_EXE}"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Hugin.lnk" "$INSTDIR\bin\hugin.exe"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Calibrate Lens GUI.lnk" "$INSTDIR\bin\calibrate_lens_gui.exe"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Batch Processor.lnk" "$INSTDIR\bin\PTBatcherGUI.exe"
  SectionEnd
  
  Section $(TEXT_SecShortcutDesktop) SecShortcutDesktop
    AddSize 1
    SetOutPath "$INSTDIR\bin"
    CreateShortCut "$DESKTOP\Hugin.lnk" "$INSTDIR\bin\hugin.exe" ""
  SectionEnd
SectionGroupEnd

;--------------------------------
;Descriptions  
  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecHugin} $(DESC_SecHugin)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecHuginDoc} $(DESC_SecHuginDoc)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCleanRegistrySettings} $(DESC_SecCleanRegistrySettings)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} $(DESC_SecShortcuts)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcutPrograms} $(DESC_SecShortcutPrograms)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcutDesktop} $(DESC_SecShortcutDesktop)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "un.Uninstall Hugin"
  !insertmacro UNINSTALL.LOG_BEGIN_UNINSTALL
  SectionIn RO
  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR"
  ; SetShellVarContext all  
  Delete "$SMPROGRAMS\${APP_NAME}\Hugin.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"

  !insertmacro UNINSTALL.LOG_END_UNINSTALL

  ;Delete Desktop shortcut (if exists)
  Delete "$DESKTOP\Hugin.lnk"
   
  ;Delete Uninstaller And Uninstall Registry Entries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hugin" 
  
  ;Delete .pto file assiciation
  DeleteRegKey HKLM "SOFTWARE\Classes\.pto"
  DeleteRegKey HKLM "SOFTWARE\Classes\HuginProject"
SectionEnd

Section /o "un.$(TEXT_SecCleanRegistrySettings)"
  DeleteRegKey  HKCU "Software\Hugin"
SectionEnd

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Italian"

  ; Language Files
  !include "Licenses\licenses.nsh"
  
  !include "Languages\EN.nsh"
  !include "Languages\DE.nsh"
  !include "Languages\IT.nsh"    
  
; ----------------------
; Installer versions
  VIProductVersion ${VERSION}
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Hugin ${HUGIN_VERSION}-${HUGIN_VERSION_BUILD}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" ${VERSION}
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Hugin Setup"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" ""

; --------------------------
; Functions

Function .onInit

  !insertmacro MUI_LANGDLL_DISPLAY
  
  ;prepare uninstall log
  !insertmacro UNINSTALL.LOG_PREPARE_INSTALL
  
FunctionEnd

Function .onInstSuccess

  ;create log file for use with uninstaller
  !insertmacro UNINSTALL.LOG_UPDATE_INSTALL

FunctionEnd

;Clean registry on install if selected
Function CleanRegistryOnInstallIfSelected
 
  SectionGetFlags ${SecCleanRegistrySettings} $R0 
  IntOp $R0 $R0 & ${SF_SELECTED} 
  IntCmp $R0 ${SF_SELECTED} CleanRegistry DoNotCleanRegistry
 
  CleanRegistry: 
    DeleteRegKey  HKCU "Software\Hugin" 
 
  DoNotCleanRegistry:
    ;do nothing
FunctionEnd

; Add Align_Image_Stack control point generator settings to registry
; Align_image_stack.exe is provided directly from Hugin 
;Function AddCPAlignImageStack
;  StrCpy $R0 1 ; R0 = Type
;  StrCpy $R1 1 ; R1 = Option
;  StrCpy $R2 "$INSTDIR\bin\align_image_stack.exe" ; R2 = Program
;  StrCpy $R3 "-f %v -v -p %o %i" ; R3 = Arguments
;  StrCpy $R4 "Align image stack";  R4 = Description
;  Call ControlPointRegistryAdd
;FunctionEnd
