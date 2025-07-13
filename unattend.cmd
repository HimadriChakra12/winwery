@echo off
(
echo ^<?xml version="1.0" encoding="utf-8"?^>
echo ^<unattend xmlns="urn:schemas-microsoft-com:unattend"^>

rem ===== Offline servicing pass: add drivers =====
echo   ^<settings pass="offlineServicing"^>
echo     ^<component name="Microsoft-Windows-PnpSysprepDriver" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"^>
echo       ^<DriverPaths^>
echo         ^<PathAndCredentials wcm:action="add"^>
echo           ^<Path^>F:\Drivers^</Path^>
echo         ^</PathAndCredentials^>
echo       ^</DriverPaths^>
echo     ^</component^>
echo   ^</settings^>

rem ===== Offline servicing pass: add updates =====
echo   ^<settings pass="offlineServicing"^>
echo     ^<component name="Microsoft-Windows-Foundation-Package" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"^>
echo       ^<PackagePaths^>
echo         ^<PackagePath^>F:\Updates\update1.cab^</PackagePath^>
echo         ^<PackagePath^>F:\Updates\update2.cab^</PackagePath^>
echo       ^</PackagePaths^>
echo     ^</component^>
echo   ^</settings^>

rem ===== oobeSystem pass: user setup and OOBE =====
echo   ^<settings pass="oobeSystem"^>
echo     ^<component name="Microsoft-Windows-Shell-Setup" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"^>
echo       ^<AutoLogon^>
echo         ^<Username^>AdminUser^</Username^>
echo         ^<Password^>
echo           ^<Value^>MySecurePassword^</Value^>
echo           ^<PlainText^>true^</PlainText^>
echo         ^</Password^>
echo         ^<Enabled^>true^</Enabled^>
echo         ^<LogonCount^>1^</LogonCount^>
echo       ^</AutoLogon^>
echo       ^<UserAccounts^>
echo         ^<LocalAccounts^>
echo           ^<LocalAccount^>
echo             ^<Name^>AdminUser^</Name^>
echo             ^<Group^>Administrators^</Group^>
echo             ^<Password^>
echo               ^<Value^>MySecurePassword^</Value^>
echo               ^<PlainText^>true^</PlainText^>
echo             ^</Password^>
echo           ^</LocalAccount^>
echo         ^</LocalAccounts^>
echo       ^</UserAccounts^>
echo       ^<RegisteredOrganization^>MyOrg^</RegisteredOrganization^>
echo       ^<RegisteredOwner^>MyName^</RegisteredOwner^>
echo       ^<TimeZone^>Bangladesh Standard Time^</TimeZone^>
echo       ^<OOBE^>
echo         ^<HideEULAPage^>true^</HideEULAPage^>
echo         ^<NetworkLocation^>Work^</NetworkLocation^>
echo         ^<ProtectYourPC^>1^</ProtectYourPC^>
echo       ^</OOBE^>
echo     ^</component^>
echo   ^</settings^>

echo ^</unattend^>
) > C:\Windows\System32\Sysprep\unattend.xml
