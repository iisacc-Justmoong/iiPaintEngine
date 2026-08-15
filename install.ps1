Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RootDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$BuildDir = Join-Path $RootDir "build"
$Prefix = if ($env:IIPAINTENGINE_PREFIX) { $env:IIPAINTENGINE_PREFIX } else { Join-Path $HOME ".local/iiPaintEngine" }
$QtRoot = if ($env:IIPAINTENGINE_QT_ROOT) { $env:IIPAINTENGINE_QT_ROOT } elseif (Test-Path -LiteralPath "C:\Qt\6.8.3" -PathType Container) { "C:\Qt\6.8.3" } else { Join-Path $HOME "Qt/6.8.3" }
$LvrsPrefix = if ($env:IIPAINTENGINE_LVRS_PREFIX) { $env:IIPAINTENGINE_LVRS_PREFIX } else { Join-Path $HOME ".local/LVRS" }

$WindowsQtPrefix = if ($env:IIPAINTENGINE_WINDOWS_QT_PREFIX) { $env:IIPAINTENGINE_WINDOWS_QT_PREFIX } else { Join-Path $QtRoot "mingw_64" }
$AndroidQtPrefix = if ($env:IIPAINTENGINE_ANDROID_QT_PREFIX) { $env:IIPAINTENGINE_ANDROID_QT_PREFIX } else { Join-Path $QtRoot "android_arm64_v8a" }
$WasmQtPrefix = if ($env:IIPAINTENGINE_WASM_QT_PREFIX) { $env:IIPAINTENGINE_WASM_QT_PREFIX } else { "" }

$WindowsBuildDir = $BuildDir
$AndroidBuildDir = Join-Path $BuildDir "platforms/android"
$WasmBuildDir = Join-Path $BuildDir "platforms/wasm"

$WindowsPrefix = $Prefix
$WindowsPlatformPrefix = Join-Path $Prefix "platforms/windows"
$AndroidPrefix = Join-Path $Prefix "platforms/android"
$WasmPrefix = Join-Path $Prefix "platforms/wasm"

function Resolve-CMakeExecutable {
    $candidates = @()
    if ($env:IIPAINTENGINE_CMAKE_PATH) {
        $candidates += $env:IIPAINTENGINE_CMAKE_PATH
    }
    if ($env:LOCALAPPDATA) {
        $candidates += Join-Path $env:LOCALAPPDATA "Programs/CLion/bin/cmake/win/x64/bin/cmake.exe"
    }
    $pathCommand = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($pathCommand) {
        $candidates += $pathCommand.Source
    }

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (-not $candidate -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            continue
        }
        $versionOutput = (& $candidate --version 2>$null) | Out-String
        if ($LASTEXITCODE -ne 0) {
            continue
        }
        $versionMatch = [regex]::Match($versionOutput, 'cmake version\s+(?<version>\d+\.\d+(?:\.\d+)?)')
        if ($versionMatch.Success -and [version]$versionMatch.Groups["version"].Value -ge [version]"3.31") {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw "iiPaintEngine requires CMake 3.31 or newer. Set IIPAINTENGINE_CMAKE_PATH to a compatible cmake.exe."
}

function Resolve-WindowsNinjaExecutable {
    param([Parameter(Mandatory = $true)][string] $QtPrefix)

    $qtInstallRoot = Split-Path -Parent (Split-Path -Parent $QtPrefix)
    $candidates = @(
        $env:IIPAINTENGINE_NINJA_PATH,
        (Join-Path $qtInstallRoot "Tools/Ninja/ninja.exe")
    )
    $pathCommand = Get-Command "ninja.exe" -ErrorAction SilentlyContinue
    if ($pathCommand) {
        $candidates += $pathCommand.Source
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw "Ninja is required for the Windows Qt MinGW build. Set IIPAINTENGINE_NINJA_PATH to ninja.exe."
}

function Resolve-WindowsMinGwCompiler {
    param([Parameter(Mandatory = $true)][string] $QtPrefix)

    $qconfigPath = Join-Path $QtPrefix "mkspecs/qconfig.pri"
    if (-not (Test-Path -LiteralPath $qconfigPath -PathType Leaf)) {
        throw "Qt compiler metadata is missing: $qconfigPath"
    }
    $qconfig = Get-Content -LiteralPath $qconfigPath -Raw
    $majorMatch = [regex]::Match($qconfig, '(?m)^QT_GCC_MAJOR_VERSION\s*=\s*(?<value>\d+)\s*$')
    $minorMatch = [regex]::Match($qconfig, '(?m)^QT_GCC_MINOR_VERSION\s*=\s*(?<value>\d+)\s*$')
    $patchMatch = [regex]::Match($qconfig, '(?m)^QT_GCC_PATCH_VERSION\s*=\s*(?<value>\d+)\s*$')
    if (-not $majorMatch.Success -or -not $minorMatch.Success -or -not $patchMatch.Success) {
        throw "Qt MinGW version metadata is incomplete: $qconfigPath"
    }

    $qtInstallRoot = Split-Path -Parent (Split-Path -Parent $QtPrefix)
    $toolDirectoryName = "mingw$($majorMatch.Groups['value'].Value)$($minorMatch.Groups['value'].Value)$($patchMatch.Groups['value'].Value)_64"
    $candidates = @()
    if ($env:IIPAINTENGINE_MINGW_ROOT) {
        $configuredItem = Get-Item -LiteralPath $env:IIPAINTENGINE_MINGW_ROOT -ErrorAction SilentlyContinue
        $configuredCompiler = if ($configuredItem -and $configuredItem.PSIsContainer) {
            Join-Path $env:IIPAINTENGINE_MINGW_ROOT "bin/g++.exe"
        } else {
            $env:IIPAINTENGINE_MINGW_ROOT
        }
        $candidates += $configuredCompiler
    }
    $candidates += Join-Path $qtInstallRoot "Tools/$toolDirectoryName/bin/g++.exe"

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw "The MinGW compiler matching Qt ($toolDirectoryName) was not found. Set IIPAINTENGINE_MINGW_ROOT to the matching toolchain."
}

$CMakeExecutable = Resolve-CMakeExecutable
$CTestExecutable = Join-Path (Split-Path -Parent $CMakeExecutable) "ctest.exe"
if (-not (Test-Path -LiteralPath $CTestExecutable -PathType Leaf)) {
    throw "CTest was not found next to the selected CMake executable: $CTestExecutable"
}

function Get-DefaultInstallPlatforms {
    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
        return "windows,android,wasm"
    }

    return ([System.Runtime.InteropServices.RuntimeInformation]::OSDescription.ToLowerInvariant())
}

$AutoInstallPlatforms = $true
if ($env:IIPAINTENGINE_INSTALL_PLATFORMS) {
    $AutoInstallPlatforms = $false
    $InstallPlatforms = $env:IIPAINTENGINE_INSTALL_PLATFORMS
} else {
    $InstallPlatforms = Get-DefaultInstallPlatforms
}
$SkipTests = if ($env:IIPAINTENGINE_SKIP_TESTS) { $env:IIPAINTENGINE_SKIP_TESTS } else { "OFF" }

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)][string] $CacheFile,
        [Parameter(Mandatory = $true)][string] $Key
    )

    foreach ($line in Get-Content -Path $CacheFile) {
        if ($line -match "^$([regex]::Escape($Key))(:[^=]*)?=(.*)$") {
            return $Matches[2]
        }
    }

    return ""
}

function Remove-StaleBuildDir {
    param([Parameter(Mandatory = $true)][string] $Path)

    $cacheFile = Join-Path $Path "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
        return
    }

    $cachedSource = Get-CMakeCacheValue -CacheFile $cacheFile -Key "CMAKE_HOME_DIRECTORY"
    $cachedBuild = Get-CMakeCacheValue -CacheFile $cacheFile -Key "CMAKE_CACHEFILE_DIR"
    $expectedSource = [System.IO.Path]::GetFullPath($RootDir)
    $expectedBuild = [System.IO.Path]::GetFullPath($Path)
    $staleCache = $false

    if ($cachedSource -and ([System.IO.Path]::GetFullPath($cachedSource) -ne $expectedSource)) {
        $staleCache = $true
    }

    if ($cachedBuild -and ([System.IO.Path]::GetFullPath($cachedBuild) -ne $expectedBuild)) {
        $staleCache = $true
    }

    if ($staleCache) {
        Write-Host "Removing stale CMake build directory: $Path"
        Write-Host "  cached source: $(if ($cachedSource) { $cachedSource } else { 'unknown' })"
        Write-Host "  current source: $RootDir"
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Join-CMakePrefixPath {
    param([string[]] $Prefixes)
    return [string]::Join(";", ($Prefixes | Where-Object { $_ }))
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)][string] $FilePath,
        [string[]] $Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Skip-OrFail {
    param(
        [Parameter(Mandatory = $true)][string] $Platform,
        [Parameter(Mandatory = $true)][string] $Message
    )

    if ($AutoInstallPlatforms) {
        Write-Warning "Skipping iiPaintEngine $Platform package: $Message"
        return $false
    }

    throw "Cannot install iiPaintEngine $Platform package: $Message"
}

function Require-DirectoryOrSkip {
    param(
        [Parameter(Mandatory = $true)][string] $Platform,
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Message
    )

    if (Test-Path -LiteralPath $Path -PathType Container) {
        return $true
    }

    return Skip-OrFail -Platform $Platform -Message "$Message`: $Path"
}

function Require-FileOrSkip {
    param(
        [Parameter(Mandatory = $true)][string] $Platform,
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Message
    )

    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        return $true
    }

    return Skip-OrFail -Platform $Platform -Message "$Message`: $Path"
}

function Require-WindowsHostOrSkip {
    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
        return $true
    }

    return Skip-OrFail -Platform "windows" -Message "windows package requires a Windows host"
}

function Get-LatestChildDirectory {
    param([Parameter(Mandatory = $true)][string] $Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return ""
    }

    $latest = Get-ChildItem -LiteralPath $Path -Directory | Sort-Object -Property FullName | Select-Object -Last 1
    if ($latest) {
        return $latest.FullName
    }

    return ""
}

function Resolve-WasmQtPrefix {
    if ($WasmQtPrefix -and (Test-Path -LiteralPath (Join-Path $WasmQtPrefix "lib/cmake/Qt6") -PathType Container)) {
        return $WasmQtPrefix
    }

    $candidates = @(
        (Join-Path $QtRoot "wasm_multithread"),
        (Join-Path $QtRoot "wasm_singlethread")
    )
    $candidates += @(Get-ChildItem -LiteralPath $QtRoot -Directory -Filter "wasm_*" -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate "lib/cmake/Qt6") -PathType Container)) {
            return $candidate
        }
    }

    return ""
}

function Resolve-AndroidSdkRoot {
    $candidates = @(
        $env:ANDROID_SDK_ROOT,
        $env:ANDROID_HOME,
        $(if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA "Android/Sdk" } else { "" }),
        $(Join-Path $HOME "AppData/Local/Android/Sdk")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return $candidate
        }
    }

    return ""
}

function Resolve-AndroidNdkRoot {
    param([Parameter(Mandatory = $true)][string] $SdkRoot)

    $sdkNdk = Get-LatestChildDirectory -Path (Join-Path $SdkRoot "ndk")
    $candidates = @($env:ANDROID_NDK_ROOT, $env:ANDROID_NDK_HOME, $env:CMAKE_ANDROID_NDK, $sdkNdk)
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return $candidate
        }
    }

    return ""
}

function Resolve-EmscriptenToolchainFile {
    $candidates = @($env:IIPAINTENGINE_EMSCRIPTEN_TOOLCHAIN_FILE, $env:QT_CHAINLOAD_TOOLCHAIN_FILE, $env:LVRS_BOOTSTRAP_EMSCRIPTEN_TOOLCHAIN_FILE)
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }

    $emsdkRoots = @($env:IIPAINTENGINE_EMSDK_ROOT, $env:EMSDK, (Join-Path $HOME "emsdk"), (Join-Path $HOME ".local/emsdk"))
    foreach ($emsdkRoot in $emsdkRoots) {
        if (-not $emsdkRoot) {
            continue
        }

        $candidate = Join-Path $emsdkRoot "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    return ""
}

function Verify-DynamicLibrary {
    param(
        [Parameter(Mandatory = $true)][string] $Platform,
        [Parameter(Mandatory = $true)][string] $InstallPrefix
    )

    $expected = ""
    switch ($Platform) {
        "windows" {
            $found = Get-ChildItem -LiteralPath (Join-Path $InstallPrefix "bin") -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -eq "iiPaintEngine.dll" -or $_.Name -eq "libiiPaintEngine.dll" } |
                Select-Object -First 1
            if ($found) {
                $expected = $found.FullName
            }
        }
        "android" { $expected = Join-Path $InstallPrefix "lib/libiiPaintEngine.so" }
        "wasm" {
            $expected = Join-Path $InstallPrefix "lib/libiiPaintEngine.a"
        }
    }

    if ($expected -and (Test-Path -LiteralPath $expected -PathType Leaf)) {
        Write-Host "Verified iiPaintEngine $Platform library: $expected"
        return
    }

    throw "iiPaintEngine $Platform library was not found under $InstallPrefix"
}

$IiPaintEngineHostTestTargets = @(
    "iiPaintEngineTests",
    "iiPaintEngineCoreTests",
    "iiPaintEnginePipelineTests",
    "iiPaintEngineBitmapDocumentStructureTests",
    "iiPaintEngineDocumentSerializerContractTests",
    "iiPaintEngineAppDocumentApiContractTests",
    "iiPaintEngineInstallLayoutContractTests",
    "iiPaintEnginePublicUmbrellaHeaderContractTests",
    "iiPaintEnginePublicCxx17HeaderContractTests",
    "iiPaintEngineBitmapFileQmlApiTests",
    "iiPaintEngineBitmapFileApiContractTests",
    "iiPaintEngineBitmapFilePointerAlignmentTests",
    "iiPaintEngineBitmapFileTabletPressureContractTests",
    "iiPaintEngineBitmapFileLivePreviewRealtimeContractTests",
    "iiPaintEnginePointerStrokeFlowTests",
    "iiPaintEnginePressureInputContractTests",
    "iiPaintEngineTabletInputSurfaceTests",
    "iiPaintEngineRasterPaintingModelTests",
    "iiPaintEngineStrokePhysicalContractTests",
    "iiPaintEngineBrushDynamicsMappingTests",
    "iiPaintEngineBrushDynamicsResponseCurveContractTests",
    "iiPaintEngineBrushFeatureToggleContractTests",
    "iiPaintEngineBrushExpressionContractTests",
    "iiPaintEngineBrushTextureLayerContractTests",
    "iiPaintEngineWetBrushSimulationContractTests",
    "iiPaintEngineHistoryUndoRedoContractTests",
    "iiPaintEngineEditingToolPipelineContractTests",
    "iiPaintEngineStrokeCompositingTests",
    "iiPaintEngineLayerCompositingContractTests",
    "iiPaintEngineRendererProjectionContractTests",
    "iiPaintEngineRenderCacheBackendContractTests",
    "iiPaintEngineColorManagementContractTests",
    "iiPaintEngineBrushMaskSamplingTests",
    "iiPaintEngineCoordinateDirtyRegionTests",
    "iiPaintEngineBitmapFileEventLoopLoadContractTests",
    "iiPaintEngineBitmapOnlyArchitectureContractTests",
    "iiPaintEngineBitmapFileCompatibilityContractTests",
    "iiPaintEngineBitmapFileArchitectureContractTests"
)

function Build-HostTests {
    param([Parameter(Mandatory = $true)][string] $PlatformBuildDir)

    if ($SkipTests -in @("ON", "1", "true")) {
        Write-Host "Skipping iiPaintEngine host test build because IIPAINTENGINE_SKIP_TESTS=$SkipTests"
        return
    }

    Invoke-NativeCommand -FilePath $CMakeExecutable -Arguments (@("--build", $PlatformBuildDir, "--config", "Release", "--target") + $IiPaintEngineHostTestTargets)
}

function Run-HostTests {
    param([Parameter(Mandatory = $true)][string] $PlatformBuildDir)

    if ($SkipTests -in @("ON", "1", "true")) {
        Write-Host "Skipping iiPaintEngine host tests because IIPAINTENGINE_SKIP_TESTS=$SkipTests"
        return
    }

    Invoke-NativeCommand -FilePath $CTestExecutable -Arguments @("--test-dir", $PlatformBuildDir, "--output-on-failure", "-E", "iiPaintEngineExampleDemoContract")
}

function Add-UserPathEntries {
    param([Parameter(Mandatory = $true)][string[]] $Entries)

    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathParts = @()
    if ($userPath) {
        $pathParts = @($userPath -split ";" | Where-Object { $_ })
    }

    $changed = $false
    foreach ($entry in $Entries) {
        if (-not $entry -or -not (Test-Path -LiteralPath $entry -PathType Container)) {
            continue
        }

        $alreadyPresent = $false
        foreach ($part in $pathParts) {
            if ($part.TrimEnd("\") -ieq $entry.TrimEnd("\")) {
                $alreadyPresent = $true
                break
            }
        }

        if (-not $alreadyPresent) {
            $pathParts = @($entry) + $pathParts
            $changed = $true
        }
    }

    if ($changed) {
        [Environment]::SetEnvironmentVariable("Path", [string]::Join(";", $pathParts), "User")
        Write-Host "Updated user PATH for iiPaintEngine runtime DLL directories."
    }
}

function Add-UserCMakePrefixEntries {
    param([Parameter(Mandatory = $true)][string[]] $Entries)

    $userPrefixPath = [Environment]::GetEnvironmentVariable("CMAKE_PREFIX_PATH", "User")
    $prefixParts = @()
    if ($userPrefixPath) {
        $prefixParts = @($userPrefixPath -split ";" | Where-Object { $_ })
    }

    $changed = $false
    foreach ($entry in $Entries) {
        if (-not $entry -or -not (Test-Path -LiteralPath $entry -PathType Container)) {
            continue
        }

        $alreadyPresent = $false
        foreach ($part in $prefixParts) {
            if ($part.TrimEnd("\") -ieq $entry.TrimEnd("\")) {
                $alreadyPresent = $true
                break
            }
        }

        if (-not $alreadyPresent) {
            $prefixParts = @($entry) + $prefixParts
            $changed = $true
        }
    }

    if ($changed) {
        [Environment]::SetEnvironmentVariable("CMAKE_PREFIX_PATH", [string]::Join(";", $prefixParts), "User")
        Write-Host "Updated user CMAKE_PREFIX_PATH for iiPaintEngine consumers."
    }
}

function Configure-BuildInstall {
    param(
        [Parameter(Mandatory = $true)][string] $Platform,
        [Parameter(Mandatory = $true)][string] $PlatformBuildDir,
        [Parameter(Mandatory = $true)][string] $InstallPrefix,
        [Parameter(Mandatory = $true)][string] $QtPrefix,
        [Parameter(Mandatory = $true)][string] $LvrsPlatformPrefix,
        [string[]] $ExtraCMakeArgs = @()
    )

    $lvrsConfigDir = $LvrsPlatformPrefix
    $nestedLvrsConfigDir = Join-Path $LvrsPlatformPrefix "lib/cmake/LVRS"
    if (Test-Path -LiteralPath (Join-Path $nestedLvrsConfigDir "LVRSConfig.cmake") -PathType Leaf) {
        $lvrsConfigDir = $nestedLvrsConfigDir
    }

    Remove-StaleBuildDir -Path $PlatformBuildDir
    $cmakePrefixPath = Join-CMakePrefixPath -Prefixes @($QtPrefix, $LvrsPlatformPrefix, $LvrsPrefix)

    Write-Host "Configuring iiPaintEngine $Platform install prefix: $InstallPrefix"
    $cmakeArgs = @(
        "-S", $RootDir,
        "-B", $PlatformBuildDir,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=$InstallPrefix",
        "-DCMAKE_PREFIX_PATH=$cmakePrefixPath",
        "-DLVRS_DIR=$lvrsConfigDir",
        "-DIIPAINTENGINE_BUILD_SHARED=ON"
    )
    $cmakeArgs += $ExtraCMakeArgs

    Invoke-NativeCommand -FilePath $CMakeExecutable -Arguments (@("--fresh") + $cmakeArgs)

    Write-Host "Building iiPaintEngine $Platform library in $PlatformBuildDir"
    Invoke-NativeCommand -FilePath $CMakeExecutable -Arguments @("--build", $PlatformBuildDir, "--config", "Release", "--target", "iiPaintEngine")

    Write-Host "Installing iiPaintEngine $Platform package into $InstallPrefix"
    if ($Platform -eq "wasm") {
        Remove-Item -LiteralPath (Join-Path $InstallPrefix "lib/libiiPaintEngine.so") -Force -ErrorAction SilentlyContinue
    }
    Invoke-NativeCommand -FilePath $CMakeExecutable -Arguments @("--install", $PlatformBuildDir, "--prefix", $InstallPrefix, "--config", "Release")
    Verify-DynamicLibrary -Platform $Platform -InstallPrefix $InstallPrefix
}

function Install-WindowsPackage {
    if (-not (Require-WindowsHostOrSkip)) {
        return
    }

    if (-not (Require-DirectoryOrSkip -Platform "windows" -Path (Join-Path $WindowsQtPrefix "lib/cmake/Qt6") -Message "Qt Windows package is required")) {
        return
    }

    $lvrsWindowsPrefix = Join-Path $LvrsPrefix "platforms/windows"
    if (-not (Require-FileOrSkip -Platform "windows" -Path (Join-Path $lvrsWindowsPrefix "LVRSConfig.cmake") -Message "LVRS Windows package is required")) {
        return
    }

    $windowsCxxCompiler = Resolve-WindowsMinGwCompiler -QtPrefix $WindowsQtPrefix
    $windowsNinja = Resolve-WindowsNinjaExecutable -QtPrefix $WindowsQtPrefix
    $windowsMinGwBin = Split-Path -Parent $windowsCxxCompiler
    $env:Path = "$windowsMinGwBin;$env:Path"

    Configure-BuildInstall -Platform "windows" -PlatformBuildDir $WindowsBuildDir -InstallPrefix $WindowsPrefix -QtPrefix $WindowsQtPrefix -LvrsPlatformPrefix $lvrsWindowsPrefix -ExtraCMakeArgs @(
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=$windowsNinja",
        "-DCMAKE_CXX_COMPILER=$windowsCxxCompiler"
    )

    Write-Host "Installing iiPaintEngine windows platform mirror into $WindowsPlatformPrefix"
    Invoke-NativeCommand -FilePath $CMakeExecutable -Arguments @("--install", $WindowsBuildDir, "--prefix", $WindowsPlatformPrefix, "--config", "Release")
    Verify-DynamicLibrary -Platform "windows" -InstallPrefix $WindowsPlatformPrefix
    Copy-Item -Force (Join-Path $WindowsBuildDir "iiPaintEngineConfigVersionRoot.cmake") (Join-Path $WindowsPrefix "lib/cmake/iiPaintEngine/iiPaintEngineConfigVersion.cmake")

    Write-Host "Building iiPaintEngine windows tests in $WindowsBuildDir"
    Build-HostTests -PlatformBuildDir $WindowsBuildDir

    Write-Host "Running iiPaintEngine windows tests"
    Run-HostTests -PlatformBuildDir $WindowsBuildDir

    Add-UserPathEntries -Entries @(
        (Join-Path $WindowsPlatformPrefix "bin"),
        (Join-Path $WindowsPrefix "bin"),
        (Join-Path $WindowsQtPrefix "bin"),
        $windowsMinGwBin
    )
    Add-UserCMakePrefixEntries -Entries @(
        $WindowsPrefix,
        $WindowsPlatformPrefix,
        $WindowsQtPrefix,
        $LvrsPrefix,
        (Join-Path $LvrsPrefix "platforms/windows")
    )
    [Environment]::SetEnvironmentVariable("iiPaintEngine_DIR", (Join-Path $WindowsPlatformPrefix "lib/cmake/iiPaintEngine"), "User")
}

function Install-AndroidPackage {
    $androidToolchainFile = Join-Path $AndroidQtPrefix "lib/cmake/Qt6/qt.toolchain.cmake"
    $lvrsAndroidPrefix = Join-Path $LvrsPrefix "platforms/android"

    if (-not (Require-DirectoryOrSkip -Platform "android" -Path (Join-Path $AndroidQtPrefix "lib/cmake/Qt6") -Message "Qt Android package is required")) {
        return
    }
    if (-not (Require-FileOrSkip -Platform "android" -Path $androidToolchainFile -Message "Qt Android toolchain file is required")) {
        return
    }
    if (-not (Require-FileOrSkip -Platform "android" -Path (Join-Path $lvrsAndroidPrefix "LVRSConfig.cmake") -Message "LVRS Android package is required")) {
        return
    }

    $androidSdkRoot = Resolve-AndroidSdkRoot
    if (-not $androidSdkRoot) {
        [void](Skip-OrFail -Platform "android" -Message "Android SDK root was not found")
        return
    }

    $androidNdkRoot = Resolve-AndroidNdkRoot -SdkRoot $androidSdkRoot
    if (-not $androidNdkRoot) {
        [void](Skip-OrFail -Platform "android" -Message "Android NDK root was not found under $androidSdkRoot")
        return
    }

    Configure-BuildInstall -Platform "android" -PlatformBuildDir $AndroidBuildDir -InstallPrefix $AndroidPrefix -QtPrefix $AndroidQtPrefix -LvrsPlatformPrefix $lvrsAndroidPrefix -ExtraCMakeArgs @(
        "-DCMAKE_TOOLCHAIN_FILE=$androidToolchainFile",
        "-DCMAKE_SYSTEM_NAME=Android",
        "-DANDROID_ABI=arm64-v8a",
        "-DANDROID_PLATFORM=android-23",
        "-DANDROID_SDK_ROOT=$androidSdkRoot",
        "-DANDROID_NDK=$androidNdkRoot",
        "-DCMAKE_ANDROID_NDK=$androidNdkRoot"
    )
}

function Install-WasmPackage {
    $resolvedWasmQtPrefix = Resolve-WasmQtPrefix
    if (-not $resolvedWasmQtPrefix) {
        [void](Skip-OrFail -Platform "wasm" -Message "Qt WASM package was not found")
        return
    }

    $lvrsWasmPrefix = Join-Path $LvrsPrefix "platforms/wasm"
    if (-not (Require-DirectoryOrSkip -Platform "wasm" -Path (Join-Path $resolvedWasmQtPrefix "lib/cmake/Qt6") -Message "Qt WASM package is required")) {
        return
    }
    if (-not (Require-FileOrSkip -Platform "wasm" -Path (Join-Path $lvrsWasmPrefix "LVRSConfig.cmake") -Message "LVRS WASM package is required")) {
        return
    }

    $emscriptenToolchainFile = Resolve-EmscriptenToolchainFile
    if (-not $emscriptenToolchainFile) {
        [void](Skip-OrFail -Platform "wasm" -Message "Emscripten toolchain file was not found")
        return
    }

    $qtToolchainFile = Join-Path $resolvedWasmQtPrefix "lib/cmake/Qt6/qt.toolchain.cmake"
    Configure-BuildInstall -Platform "wasm" -PlatformBuildDir $WasmBuildDir -InstallPrefix $WasmPrefix -QtPrefix $resolvedWasmQtPrefix -LvrsPlatformPrefix $lvrsWasmPrefix -ExtraCMakeArgs @(
        "-DCMAKE_TOOLCHAIN_FILE=$qtToolchainFile",
        "-DQT_CHAINLOAD_TOOLCHAIN_FILE=$emscriptenToolchainFile",
        "-DCMAKE_SYSTEM_NAME=Emscripten"
    )
}

function Invoke-PlatformInstall {
    param([Parameter(Mandatory = $true)][string] $Platform)

    switch ($Platform) {
        "windows" { Install-WindowsPackage }
        "android" { Install-AndroidPackage }
        "wasm" { Install-WasmPackage }
        "" { return }
        default {
            throw "Unsupported iiPaintEngine install platform: $Platform. Supported platforms: windows, android, wasm"
        }
    }
}

Write-Host "Installing iiPaintEngine for platforms: $InstallPlatforms"
foreach ($platform in ($InstallPlatforms -split "[,;]")) {
    Invoke-PlatformInstall -Platform $platform.Trim().ToLowerInvariant()
}

Write-Host "iiPaintEngine installed for platforms: $InstallPlatforms."
