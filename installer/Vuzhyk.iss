; Vuzhyk IDE — установщик Windows (Inno Setup 6)
; Источник файлов: Release-сборка Visual Studio (x64\Release\Vuzhyk)
;
; Сборка (в ВМ, где есть VS + Qt + Inno Setup 6):
;   installer\build-installer.bat
; Скрипт раскладывает Qt (windeployqt), проверяет содержимое сборки и вызывает ISCC.
;
; Версия продукта задаётся ТОЛЬКО здесь (ниже). Переопределить на один запуск:
;   installer\build-installer.bat 26.5.0
;
; Прямой вызов ISCC (без проверок и без windeployqt):
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\Vuzhyk.iss

#ifndef AppVersion
  #define AppVersion "26.4.1"
#endif

#ifndef BuildOutput
  #define BuildOutput "..\x64\Release\Vuzhyk"
#endif

#define AppName "Vuzhyk"
#define AppPublisher "Vuzhyk"
#define AppExeName "Vuzhyk.exe"
#define AppId "{{B8F4E2A1-3C5D-4E6F-9A0B-1C2D3E4F5A6B}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Лицензия GPLv3 — отдельная страница мастера
LicenseFile=..\LICENSE
OutputDir=output
OutputBaseFilename=VuzhykSetup-{#AppVersion}
SetupIconFile=..\icons\logo\vuzhyk.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=6.1sp1
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Ассоциации .py ставит сама IDE из настроек (реестр HKCU), а не установщик
ChangesAssociations=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Полная установка"
Name: "compact"; Description: "Компактная (без встроенного браузера)"
Name: "custom"; Description: "Выборочная установка"; Flags: iscustom

[Components]
Name: "core"; Description: "Vuzhyk IDE (основная программа, Python, инструменты)"; Types: full compact custom; Flags: fixed
Name: "browser"; Description: "Расширение «Встроенный браузер» (Qt WebEngine, HTML во встроенном браузере)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Создать значок на рабочем столе"; GroupDescription: "Дополнительно:"; Flags: unchecked

[Files]
; --- Основное приложение ---
Source: "{#BuildOutput}\{#AppExeName}"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "{#BuildOutput}\FilePicker.exe"; DestDir: "{app}"; Components: core; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildOutput}\VuzhykSyncServer.exe"; DestDir: "{app}"; Components: core; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildOutput}\vuzhyk.ico"; DestDir: "{app}"; Components: core; Flags: ignoreversion skipifsourcedoesntexist
; Qt, QScintilla, OpenSSL, MSVC runtime — рядом с exe
Source: "{#BuildOutput}\*.dll"; DestDir: "{app}"; Components: core; Flags: ignoreversion

; --- Qt-плагины и ресурсы (раскладывает windeployqt в build-installer.bat) ---
Source: "{#BuildOutput}\platforms\*"; DestDir: "{app}\platforms"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildOutput}\imageformats\*"; DestDir: "{app}\imageformats"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildOutput}\iconengines\*"; DestDir: "{app}\iconengines"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildOutput}\styles\*"; DestDir: "{app}\styles"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildOutput}\translations\*"; DestDir: "{app}\translations"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildOutput}\bearer\*"; DestDir: "{app}\bearer"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; --- Встроенный Python ---
Source: "{#BuildOutput}\Python38\*"; DestDir: "{app}\Python38"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Утилиты (7-Zip, SumatraPDF, учебники) ---
Source: "{#BuildOutput}\tools\*"; DestDir: "{app}\tools"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; --- Справка 6–7 класса: HelpWidget читает только файлы с диска (help\<key>.html).
;     Без этой строки 16 из 55 разделов (весь 6 класс, робот и черепаха 7 класса)
;     показывают заглушку «Раздел в разработке...». Флаг skipifsourcedoesntexist
;     намеренно НЕ стоит: пропажа help должна валить сборку установщика, а не
;     тихо ломать справку у школьника.
Source: "{#BuildOutput}\help\*"; DestDir: "{app}\help"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Скрипт-сессия задачника черепахи (в exe вшит через qrc: :/tasks/…;
;     копия на диске — для обновления без пересборки и для отладки) ---
Source: "{#BuildOutput}\tasks\*"; DestDir: "{app}\tasks"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; --- Опциональное расширение: встроенный браузер ---
Source: "{#BuildOutput}\extensions\EmbeddedBrowserExtension\*"; DestDir: "{app}\extensions\EmbeddedBrowserExtension"; Components: browser; Flags: ignoreversion recursesubdirs createallsubdirs

[Dirs]
; Папка для скачиваемых интерпретаторов (создаётся пустой)
Name: "{app}\interpreters"; Components: core; Permissions: users-modify

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Components: core
Name: "{group}\Удалить {#AppName}"; Filename: "{uninstallexe}"; Components: core
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; Components: core

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Запустить {#AppName}"; Flags: nowait postinstall skipifsilent unchecked; Components: core

[UninstallDelete]
; Файлы и папки, созданные программой после установки (не попадают в журнал установщика)
Type: filesandordirs; Name: "{app}\extensions"
Type: filesandordirs; Name: "{app}\interpreters"
Type: files; Name: "{app}\Vuzhyk_debug.log"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  { После удаления отслеживаемых файлов — убираем остатки и саму папку установки }
  if CurUninstallStep = usPostUninstall then
  begin
    if DirExists(ExpandConstant('{app}')) then
      DelTree(ExpandConstant('{app}'), True, True, True);
  end;
end;
