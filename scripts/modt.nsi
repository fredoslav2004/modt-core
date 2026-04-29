# Variables are passed from package.py via /D flags
# !define APPNAME "..."
# !define NAME "..."
# !define DESCRIPTION "..."
# !define VERSION "..."

!include "LogicLib.nsh"
!include "WinMessages.nsh"

# EnvVarUpdate function
!macro EnvVarUpdate VarName Action Scope Value
  # Action: A (Add), R (Remove)
  # Scope: HKLM (System), HKCU (User)
  DetailPrint "Updating PATH: $2 ($3) - $5"
  
  ReadRegStr $R0 ${Scope} "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "${VarName}"
  ${If} ${Action} == "A"
    # Check if exists
    StrCpy $R1 "$R0"
    # Simplified check: search for $INSTDIR in PATH
    Push "$R1"
    Push "${Value}"
    Call StrStr
    Pop $R2
    ${If} $R2 == ""
       # Add to end
       ${If} $R1 == ""
         WriteRegExpandStr ${Scope} "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "${VarName}" "${Value}"
       ${Else}
         WriteRegExpandStr ${Scope} "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "${VarName}" "$R1;${Value}"
       ${EndIf}
    ${EndIf}
  ${ElseIf} ${Action} == "R"
     # Removal is more complex, but for now we focus on Addition as requested
  ${EndIf}
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
!macroend

Function StrStr
  Exch $R1 ; string to find
  Exch
  Exch $R0 ; string to search in
  Push $0
  Push $1
  Push $2
  Push $3
  Push $4

  StrLen $1 $R0
  StrLen $2 $R1
  StrCpy $3 0

  StrStr_loop:
    StrCpy $4 $R0 $2 $3
    StrCmp $4 $R1 StrStr_done
    IntOp $3 $3 + 1
    IntCmp $3 $1 +1 StrStr_loop StrStr_done
    StrCpy $3 ""
  StrStr_done:
  StrCpy $R0 $3
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
  Pop $R1
  Exch $R0
FunctionEnd

# Set compression
SetCompressor lzma

# Output file
OutFile "${OUTFILE}"

# Default installation folder
InstallDir "$PROGRAMFILES64\${APPNAME}"

# Request admin privileges
RequestExecutionLevel admin

Page directory
Page instfiles

Section "MainSection" SEC01
    SetOutPath "$INSTDIR"
    File "..\dist\windows\${NAME}.exe"
    File "..\dist\windows\Documentation.pdf"
    File "..\dist\windows\Documentation.html"
    File "..\dist\windows\Documentation.md"
    File "..\README.md"
    File "..\LICENSE"
    
    # Write the uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    # Update PATH for all users (system)
    !insertmacro EnvVarUpdate "PATH" "A" "HKLM" "$INSTDIR"
SectionEnd

Section "Uninstall"
    # Note: Removal logic for PATH in uninstall can be added here
    # !insertmacro EnvVarUpdate "PATH" "R" "HKLM" "$INSTDIR"

    Delete "$INSTDIR\${NAME}.exe"
    Delete "$INSTDIR\Documentation.pdf"
    Delete "$INSTDIR\Documentation.html"
    Delete "$INSTDIR\Documentation.md"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
SectionEnd
