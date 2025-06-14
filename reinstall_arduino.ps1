# PowerShell script to reinstall Arduino IDE and required board packages

param(
    [string]$InstallDir = "C:\Program Files\Arduino",
    [string]$CliVersion = "latest",
    [string]$IdeVersion = "2.3.2"
)

function Stop-ArduinoProcesses {
    Get-Process arduino* -ErrorAction SilentlyContinue | Stop-Process -Force
}

function Remove-ArduinoInstall {
    if(Test-Path $InstallDir){
        Write-Host "Removing old installation at $InstallDir" -ForegroundColor Yellow
        Remove-Item $InstallDir -Recurse -Force
    }
}

function Install-ArduinoIDE {
    $ideUrl = "https://downloads.arduino.cc/arduino-ide_$IdeVersion_Windows_64bit.zip"
    $zipPath = "$env:TEMP\arduino_ide.zip"
    Write-Host "Downloading Arduino IDE $IdeVersion" -ForegroundColor Cyan
    Invoke-WebRequest $ideUrl -OutFile $zipPath
    Expand-Archive $zipPath -DestinationPath $InstallDir -Force
    Remove-Item $zipPath
}

function Install-ArduinoCLI {
    if($CliVersion -eq 'latest'){
        $cliInfo = Invoke-RestMethod https://api.github.com/repos/arduino/arduino-cli/releases/latest
        $CliVersion = $cliInfo.tag_name.TrimStart('v')
    }
    $cliUrl = "https://downloads.arduino.cc/arduino-cli/arduino-cli_${CliVersion}_Windows_64bit.zip"
    $zipPath = "$env:TEMP\arduino_cli.zip"
    Write-Host "Downloading Arduino CLI $CliVersion" -ForegroundColor Cyan
    Invoke-WebRequest $cliUrl -OutFile $zipPath
    Expand-Archive $zipPath -DestinationPath $InstallDir -Force
    Remove-Item $zipPath
    $env:PATH += ";$InstallDir"
}

function Setup-ArduinoPackages {
    & "$InstallDir\arduino-cli.exe" config init
    & "$InstallDir\arduino-cli.exe" core update-index
    & "$InstallDir\arduino-cli.exe" core install esp32:esp32
    & "$InstallDir\arduino-cli.exe" core install teensy:avr
    (Get-Content libraries.txt) | ForEach-Object {
        if($_){
            & "$InstallDir\arduino-cli.exe" lib install $_
        }
    }
}

Write-Host "Starting Arduino environment reinstallation..." -ForegroundColor Green
Stop-ArduinoProcesses
Remove-ArduinoInstall
Install-ArduinoIDE
Install-ArduinoCLI
Setup-ArduinoPackages
Write-Host "Arduino environment setup complete." -ForegroundColor Green
