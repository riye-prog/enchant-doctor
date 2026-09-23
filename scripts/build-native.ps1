param(
    [ValidateSet("x64", "ARM64")]
    [string]$Architecture = $(if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "ARM64" } else { "x64" }),
    [string]$Generator = "Visual Studio 17 2022",
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ProjectDirectory = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SourceDirectory = Join-Path $ProjectDirectory "native"
$BuildDirectory = Join-Path $SourceDirectory "build"

if (-not $env:JAVA_HOME) {
    throw "JAVA_HOME must point to a Java 25 JDK."
}
if (-not (Test-Path (Join-Path $env:JAVA_HOME "include\jni.h"))) {
    throw "JAVA_HOME does not contain include\jni.h: $env:JAVA_HOME"
}
if (-not (Test-Path (Join-Path $env:JAVA_HOME "include\win32\jni_md.h"))) {
    throw "JAVA_HOME does not contain Windows JNI headers: $env:JAVA_HOME"
}
if (-not $env:VULKAN_SDK) {
    throw "VULKAN_SDK is not set. Install the LunarG Vulkan SDK and open a new terminal."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake is not available on PATH."
}

$ConfigureArguments = @(
    "-S", $SourceDirectory,
    "-B", $BuildDirectory,
    "-G", $Generator,
    "-A", $Architecture,
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DJAVA_INCLUDE_PATH=$env:JAVA_HOME\include",
    "-DJAVA_INCLUDE_PATH2=$env:JAVA_HOME\include\win32"
)

& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE." }

& cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Native build failed with exit code $LASTEXITCODE." }

$PackageArchitecture = if ($Architecture -eq "ARM64") { "arm64" } else { "x86_64" }
$PackageDirectory = Join-Path $BuildDirectory "package\windows-$PackageArchitecture"
$RequiredFiles = @("doctrine.dll", "ui.vert.spv", "ui.frag.spv")
foreach ($File in $RequiredFiles) {
    if (-not (Test-Path (Join-Path $PackageDirectory $File))) {
        throw "Native build completed but package file is missing: $File"
    }
}

Write-Host "Doctrine Windows native package created at $PackageDirectory"
Write-Host "Run .\gradlew.bat build to package it into the Minecraft jars."
