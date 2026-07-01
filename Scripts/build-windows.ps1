param (
    [Parameter(Mandatory=$true)][string]$version
 )

$curDir = Get-Location

Write-Output "Setting up development environment with Visuals Studio 2022"
Import-Module "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\2022\Community\"

$Env:RELEASE_VERSION = "$version"
$Env:PATH_TO_JUCE = "$curDir\JUCE"

Write-Output "Changing location to $curDir"
Set-Location $curDir

$buildLocation = "Builds\VisualStudio2017\x64"

Write-Output "Deleting previous build from $buildLocation"
Remove-Item -LiteralPath $buildLocation -Force -Recurse

Write-Output "Building project"
MSBuild.exe .\Builds\VisualStudio2017\routemidi.sln /p:Configuration=Release /p:PreferredToolArchitecture=x64 /p:Platform=x64 /clp:ErrorsOnly

# Package the unsigned binary the way the release is distributed: a zip containing
# a versioned folder with the executable, the readme and the license.
$package = "routemidi-windows-$version"
$stageDir = "$buildLocation\$package"
Write-Output "Packaging $package.zip"
Remove-Item -LiteralPath $stageDir -Force -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stageDir | Out-Null
Copy-Item -Path "$buildLocation\Release\ConsoleApp\routemidi.exe", "README.md", "COPYING.md" -Destination $stageDir

$zip = "$package.zip"
Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path $stageDir -DestinationPath $zip

# Publish a SHA-256 checksum of the zip so users can verify the unsigned download.
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLower()
Set-Content -Path "$zip.sha256" -Value "$hash  $zip" -Encoding ascii
Write-Output "$hash  $zip"
