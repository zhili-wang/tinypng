#ifndef AppVersion
  #define AppVersion "1.2.2"
#endif
#define StageDir GetEnv("TINYPNG_STAGE")
#define InstallerDir GetEnv("TINYPNG_INSTALLER_DIR")

[Setup]
AppId={{45535D70-2A7F-4EE9-B0DE-3F63218A6BFC}
AppName=TinyPNG
AppVersion={#AppVersion}
AppPublisher=zhili-wang
AppPublisherURL=https://github.com/zhili-wang/tinypng
DefaultDirName={localappdata}\Programs\TinyPNG
DefaultGroupName=TinyPNG
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#InstallerDir}
OutputBaseFilename=TinyPNG-Windows-x64-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\TinyPNG.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\TinyPNG"; Filename: "{app}\TinyPNG.exe"
Name: "{autodesktop}\TinyPNG"; Filename: "{app}\TinyPNG.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TinyPNG.exe"; Description: "{cm:LaunchProgram,TinyPNG}"; Flags: nowait postinstall skipifsilent
