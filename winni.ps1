param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("init", "apply")]
    [string]$Command,

    [string]$ConfigPath = "C:/config.json"
)

$ErrorActionPreference = "Stop"

function Ask-YesNo($message) {
    $resp = Read-Host "$message (y/n)"
    return $resp -match '^(y|yes)$'
}

function Init-Config {
    Write-Host "🆕 Initializing or updating config: $ConfigPath"

    if (Test-Path $ConfigPath) {
        Write-Host "📄 Existing config found. Loading..."
        $config = Get-Content $ConfigPath | ConvertFrom-Json
    } else {
        Write-Host "No config found. Creating new one..."
        $config = [pscustomobject]@{
            registry = @()
            startup  = @()
        }
    }

    ### 🧠 REGISTRY ENTRIES
    if (Ask-YesNo "`n🧠 Add registry modifications?") {
        while ($true) {
            $regPath = Read-Host "Registry path (e.g., HKCU\\Software\\...)" 
            if ([string]::IsNullOrWhiteSpace($regPath)) { break }

            $regName = Read-Host "Value name"
            $regType = Read-Host "Value type (e.g., DWORD, String)"
            $regVal  = Read-Host "Value data"

            $entry = [pscustomobject]@{
                path  = $regPath
                name  = $regName
                type  = $regType
                value = $regVal
            }

            $config.registry += $entry
            Write-Host "✔ Added registry entry: $($regPath)\$($regName)"
            if (-not (Ask-YesNo "Add another registry entry?")) { break }
        }
    }

    ### 🌄 WALLPAPER
    if (Ask-YesNo "`n🖼 Set custom wallpaper?") {
        $wallpaper = Read-Host "Enter path to wallpaper (e.g., files/wallpaper.jpg)"
        $config.wallpaper = $wallpaper
    }

    ### 🚀 STARTUP SCRIPTS
    if (Ask-YesNo "`n🚀 Add startup PowerShell scripts?") {
        while ($true) {
            $scriptPath = Read-Host "Path to startup script (e.g., files/setup.ps1)"
            if ([string]::IsNullOrWhiteSpace($scriptPath)) { break }
            $config.startup += $scriptPath
            if (-not (Ask-YesNo "Add another startup script?")) { break }
        }
    }

    $config | ConvertTo-Json -Depth 10 | Set-Content $ConfigPath -Encoding UTF8
    Write-Host "`n✅ Saved configuration to $ConfigPath"
}

function Apply-Config {
    if (-not (Test-Path $ConfigPath)) {
        Write-Error "Config file $ConfigPath not found."
        exit 1
    }

    Write-Host "Applying settings from $ConfigPath ..."

    $config = Get-Content $ConfigPath | ConvertFrom-Json

    # Registry
    if ($config.registry) {
        Write-Host "`n🧠 Applying registry entries..."
        foreach ($entry in $config.registry) {
            try {
                $path = $entry.path
                $name = $entry.name
                $type = $entry.type.ToLower()
                $value = $entry.value

                if (-not (Test-Path "Registry::$path")) {
                    New-Item -Path "Registry::$path" -Force | Out-Null
                    Write-Host "Created registry path: $path"
                }

                switch ($type) {
                    "dword" { $typeVal = [Microsoft.Win32.RegistryValueKind]::DWord }
                    "qword" { $typeVal = [Microsoft.Win32.RegistryValueKind]::QWord }
                    "string" { $typeVal = [Microsoft.Win32.RegistryValueKind]::String }
                    "expandstring" { $typeVal = [Microsoft.Win32.RegistryValueKind]::ExpandString }
                    "binary" { $typeVal = [Microsoft.Win32.RegistryValueKind]::Binary }
                    default {
                        Write-Warning "Unknown registry type '$type' for $path\$name. Skipping."
                        continue
                    }
                }

                Set-ItemProperty -Path "Registry::$path" -Name $name -Value $value -Type $typeVal -Force
                Write-Host "Set $path\$name = $value ($type)"
            }
            catch {
                Write-Warning "Failed to set $($entry.path)\$($entry.name): $_"
            }
        }
    }

    # Wallpaper
    if ($config.wallpaper) {
        Write-Host "`n🖼 Setting wallpaper to $($config.wallpaper) ..."
        try {
            Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;

public class Wallpaper {
    [DllImport("user32.dll",SetLastError=true)]
    public static extern bool SystemParametersInfo(int uAction, int uParam, string lpvParam, int fuWinIni);
}
"@

            $SPI_SETDESKWALLPAPER = 20
            $SPIF_UPDATEINIFILE = 1
            $SPIF_SENDWININICHANGE = 2

            [Wallpaper]::SystemParametersInfo($SPI_SETDESKWALLPAPER, 0, $config.wallpaper, $SPIF_UPDATEINIFILE -bor $SPIF_SENDWININICHANGE) | Out-Null
            Write-Host "Wallpaper set successfully."
        }
        catch {
            Write-Warning "Failed to set wallpaper: $_"
        }
    }

    # Startup scripts
    if ($config.startup) {
        Write-Host "`n🚀 Adding startup scripts..."
        $startupFolder = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"

        foreach ($scriptPath in $config.startup) {
            if (-not (Test-Path $scriptPath)) {
                Write-Warning "Startup script not found: $scriptPath"
                continue
            }
            $dest = Join-Path $startupFolder (Split-Path $scriptPath -Leaf)
            try {
                Copy-Item -Path $scriptPath -Destination $dest -Force
                Write-Host "Copied $scriptPath to Startup folder."
            }
            catch {
                Write-Warning "Failed to copy $scriptPath: $_"
            }
        }
    }

    Write-Host "`n✅ All done! Restart your PC if needed."
}

switch ($Command) {
    "init"  { Init-Config }
    "apply" { Apply-Config }
}

