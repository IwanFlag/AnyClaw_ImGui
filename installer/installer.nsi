; installer.nsi — AnyClaw NSIS Installer Script
; Modern UI 2 installer with EULA and OpenClaw install method selection

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

; ── Defines ──────────────────────────────────────────────────────────────
!ifndef VERSION
  !define VERSION "1.0.0"
!endif

!define APPNAME "AnyClaw_ImGui"
!define COMPANY "AetherOS Project"
!define URL "https://github.com/aetheros/anyclaw"
!define UNINST_REG "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define APP_REG "Software\${APPNAME}"

Name "${APPNAME} ${VERSION}"
OutFile "AnyClaw-${VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKCU "${APP_REG}" "InstallDir"
RequestExecutionLevel admin

; ── MUI Settings ─────────────────────────────────────────────────────────
!define MUI_ABORTWARNING
!define MUI_ICON "anyclaw.ico"
!define MUI_UNICON "anyclaw.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "eula.txt"
Page custom OpenClawInstallPage OpenClawInstallPageLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.UninstallOptionsPage un.UninstallOptionsPageLeave
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ── Variables ────────────────────────────────────────────────────────────
Var OpenClawInstallMethod  ; "npm" or "exe" or "skip"
Var CheckboxNpm
Var CheckboxExe
Var CheckboxSkip
Var UninstallAnyClaw
Var UninstallOpenClaw

; ── Installer Section ────────────────────────────────────────────────────
Section "AnyClaw" SecMain
  SetOutPath "$INSTDIR"

  ; Copy main executable
  File "bin\Release\AnyClaw.exe"

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Registry: Add/Remove Programs
  WriteRegStr HKLM "${UNINST_REG}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINST_REG}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINST_REG}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKLM "${UNINST_REG}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_REG}" "DisplayIcon" "$INSTDIR\AnyClaw.exe"
  WriteRegStr HKLM "${UNINST_REG}" "Publisher" "${COMPANY}"
  WriteRegStr HKLM "${UNINST_REG}" "URLInfoAbout" "${URL}"
  WriteRegStr HKLM "${UNINST_REG}" "DisplayVersion" "${VERSION}"

  ; Registry: App settings
  WriteRegStr HKCU "${APP_REG}" "InstallDir" "$INSTDIR"

  ; Auto-start
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}" "$INSTDIR\AnyClaw.exe"

  ; Create config directory
  CreateDirectory "$APPDATA\${APPNAME}"

  ; Calculate installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_REG}" "EstimatedSize" "$0"

  ; Create Start Menu shortcut
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\AnyClaw.exe"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\卸载 ${APPNAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

; ── OpenClaw Installation ────────────────────────────────────────────────
Section /o "Install OpenClaw" SecOpenClaw
  ${If} $OpenClawInstallMethod == "npm"
    ; Check if npm is available
    nsExec::ExecToStack 'npm --version'
    Pop $0
    ${If} $0 == 0
      DetailPrint "Installing OpenClaw via npm..."
      nsExec::ExecToLog 'npm install -g openclaw'
      Pop $0
      ${If} $0 != 0
        MessageBox MB_OK "npm 安装 OpenClaw 失败。请手动安装。"
      ${Else}
        DetailPrint "OpenClaw installed successfully via npm."
      ${EndIf}
    ${Else}
      MessageBox MB_OK "未找到 npm。请先安装 Node.js，然后手动运行: npm install -g openclaw"
    ${EndIf}

  ${ElseIf} $OpenClawInstallMethod == "exe"
    DetailPrint "Opening OpenClaw download page..."
    ExecShell "open" "https://github.com/openclaw/openclaw/releases/latest"
    MessageBox MB_OK "请在浏览器中下载并运行 OpenClaw 安装程序。$\n安装完成后，AnyClaw 将自动检测。"

  ${Else}
    DetailPrint "Skipping OpenClaw installation."
  ${EndIf}
SectionEnd

; ── OpenClaw Install Method Page (Custom) ────────────────────────────────
Function OpenClawInstallPage
  ; Check if OpenClaw is already installed
  nsExec::ExecToStack 'where openclaw 2>nul'
  Pop $0
  ${If} $0 == 0
    ; Already installed, skip this page
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "OpenClaw 安装" "选择 OpenClaw 的安装方式"

  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 30u "检测到您的电脑未安装 OpenClaw。$\n请选择安装方式（或跳过后自行安装）："
  Pop $0

  ${NSD_CreateRadioButton} 10u 45u 90% 15u "通过 npm 安装（推荐，需要已安装 Node.js）"
  Pop $CheckboxNpm
  ${NSD_Check} $CheckboxNpm
  StrCpy $OpenClawInstallMethod "npm"

  ${NSD_CreateRadioButton} 10u 65u 90% 15u "通过 EXE 安装（打开浏览器下载官方安装程序）"
  Pop $CheckboxExe

  ${NSD_CreateRadioButton} 10u 85u 90% 15u "跳过，稍后自行安装"
  Pop $CheckboxSkip

  nsDialogs::Show
FunctionEnd

Function OpenClawInstallPageLeave
  ${NSD_GetState} $CheckboxNpm $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $OpenClawInstallMethod "npm"
  ${EndIf}

  ${NSD_GetState} $CheckboxExe $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $OpenClawInstallMethod "exe"
  ${EndIf}

  ${NSD_GetState} $CheckboxSkip $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $OpenClawInstallMethod "skip"
  ${EndIf}

  ; Select the OpenClaw section if not skipping
  ${If} $OpenClawInstallMethod != "skip"
    !insertmacro SelectSection ${SecOpenClaw}
  ${Else}
    !insertmacro UnselectSection ${SecOpenClaw}
  ${EndIf}
FunctionEnd

; ── Uninstaller ──────────────────────────────────────────────────────────
Section "Uninstall"
  ; Stop AnyClaw if running
  nsExec::ExecToLog 'taskkill /F /IM AnyClaw.exe 2>nul'

  ; Remove auto-start
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}"

  ; Remove files
  Delete "$INSTDIR\AnyClaw.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  ; Remove Start Menu
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\卸载 ${APPNAME}.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"

  ; Remove registry
  DeleteRegKey HKLM "${UNINST_REG}"
  DeleteRegKey HKCU "${APP_REG}"

  ; Handle AnyClaw config data
  ${If} $UninstallAnyClaw == 1
    RMDir /r "$APPDATA\${APPNAME}"
  ${EndIf}

  ; Handle OpenClaw uninstall
  ${If} $UninstallOpenClaw == 1
    ; Stop gateway
    nsExec::ExecToLog 'openclaw gateway stop 2>nul'
    Sleep 1000

    ; Try npm uninstall first
    nsExec::ExecToStack 'npm --version'
    Pop $0
    ${If} $0 == 0
      DetailPrint "Uninstalling OpenClaw via npm..."
      nsExec::ExecToLog 'npm uninstall -g openclaw'
    ${Else}
      ; Try to find and run uninstaller
      nsExec::ExecToStack 'where openclaw 2>nul'
      Pop $0
      ${If} $0 == 0
        MessageBox MB_OK "无法自动卸载 OpenClaw。$\n请手动运行: npm uninstall -g openclaw"
      ${EndIf}
    ${EndIf}

    ; Optionally remove OpenClaw config
    MessageBox MB_YESNO "是否删除 OpenClaw 配置数据 (~/.openclaw)？" IDNO +2
    RMDir /r "$PROFILE\.openclaw"
  ${EndIf}
SectionEnd

; ── Uninstall Options Page (Custom) ──────────────────────────────────────
Function un.UninstallOptionsPage
  !insertmacro MUI_HEADER_TEXT "卸载选项" "选择要卸载的内容"

  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 20u "请选择要卸载的内容："
  Pop $0

  ${NSD_CreateCheckbox} 10u 30u 90% 15u "AnyClaw 应用及配置数据"
  Pop $UninstallAnyClaw
  ${NSD_Check} $UninstallAnyClaw

  ${NSD_CreateCheckbox} 10u 55u 90% 15u "OpenClaw 应用及配置数据"
  Pop $UninstallOpenClaw

  ${NSD_CreateLabel} 10u 80u 90% 40u "注意：$\n- 卸载 AnyClaw 不会自动卸载 OpenClaw（除非您勾选）$\n- OpenClaw 配置数据 (~/.openclaw) 默认保留"
  Pop $0

  nsDialogs::Show
FunctionEnd

Function un.UninstallOptionsPageLeave
  ${NSD_GetState} $UninstallAnyClaw $0
  StrCpy $UninstallAnyClaw $0

  ${NSD_GetState} $UninstallOpenClaw $0
  StrCpy $UninstallOpenClaw $0
FunctionEnd
