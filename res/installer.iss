; Extra Inno Setup sections for the Windows installer. CPack's generated script
; #includes this file (CPACK_INNOSETUP_EXTRA_SCRIPTS in CMakeLists.txt).

[Tasks]
Name: "firewall"; Description: "Allow games I host through Windows Firewall"; GroupDescription: "Network:"

[Run]
; Delete first, so reinstalling doesn't leave duplicate rules
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""NetSpaceWar Server"""; Flags: runhidden; Tasks: firewall
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""NetSpaceWar Server"" dir=in action=allow program=""{app}\SpaceWarServer.exe"" enable=yes"; Flags: runhidden; Tasks: firewall; StatusMsg: "Adding a Windows Firewall rule for hosting games..."

[UninstallRun]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""NetSpaceWar Server"""; Flags: runhidden; RunOnceId: "RemoveFirewallRule"

[Messages]
; Installers can't pin to the taskbar (Windows doesn't allow it), so tell the player how
FinishedLabel=Setup has finished installing [name] on your computer. It can be started from the Start menu.%n%nTo pin it to the taskbar, start the game, right-click its taskbar button and choose "Pin to taskbar".
