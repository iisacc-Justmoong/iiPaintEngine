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
            $found = Get-ChildItem -LiteralPath $InstallPrefix -Recurse -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "libiiPaintEngine.*" -or $_.Name -eq "iiPaintEngine.wasm" } |
                Select-Object -First 1
            if ($found) {
                $expected = $found.FullName
            }
        }
    }

    if ($expected -and (Test-Path -LiteralPath $expected -PathType Leaf)) {
        Write-Host "Verified iiPaintEngine $Platform dynamic library: $expected"
        return
    }

    throw "iiPaintEngine $Platform dynamic library was not found under $InstallPrefix"
}

$IiPaintEngineHostTestTargets = @(
    "iiPaintEngineTests",
    "iiPaintEngineCoreTests",
    "iiPaintEnginePipelineTests",
    "iiPaintEngineCanvasDocumentStructureTests",
    "iiPaintEngineDocumentSerializerContractTests",
    "iiPaintEngineAppDocumentApiContractTests",
    "iiPaintEngineInstallLayoutContractTests",
    "iiPaintEnginePublicUmbrellaHeaderContractTests",
    "iiPaintEnginePublicCxx17HeaderContractTests",
    "iiPaintEngineCanvasQmlApiTests",
    "iiPaintEngineCanvasAdapterContractTests",
    "iiPaintEngineCanvasPointerAlignmentTests",
    "iiPaintEngineCanvasTabletPressureContractTests",
    "iiPaintEngineCanvasLivePreviewRealtimeContractTests",
    "iiPaintEnginePointerStrokeFlowTests",
    "iiPaintEnginePressureInputContractTests",
    "iiPaintEngineTabletInputSurfaceTests",
    "iiPaintEngineHybridPaintingModelTests",
    "iiPaintEngineStrokePhysicalContractTests",
    "iiPaintEngineStrokeGeometryReportContractTests",
    "iiPaintEngineStabilizerAdvancedContractTests",
    "iiPaintEngineLiveStrokeRenderingTests",
    "iiPaintEngineBrushDynamicsMappingTests",
    "iiPaintEngineBrushDynamicsResponseCurveContractTests",
    "iiPaintEngineBrushFeatureToggleContractTests",
    "iiPaintEngineBrushExpressionContractTests",
    "iiPaintEngineBrushTextureLayerContractTests",
    "iiPaintEngineWetBrushSimulationContractTests",
    "iiPaintEngineHistoryUndoRedoContractTests",
    "iiPaintEngineEditingToolPipelineContractTests",
    "iiPaintEngineStrokeResamplerTests",
    "iiPaintEngineStrokeCompositingTests",
    "iiPaintEngineLayerCompositingContractTests",
    "iiPaintEngineRendererProjectionContractTests",
    "iiPaintEngineRenderCacheBackendContractTests",
    "iiPaintEngineColorManagementContractTests",
    "iiPaintEngineBrushMaskSamplingTests",
    "iiPaintEngineCoordinateDirtyRegionTests",
    "iiPaintEngineCanvasEventThreadingTests",
    "iiPaintEngineCanvasEventLoopLoadContractTests",
    "iiPaintEngineCanvasInputBatchingContractTests",
    "iiPaintEngineCanvasStrokePipelineSeparationContractTests"
)

function Build-HostTests {
    param([Parameter(Mandatory = $true)][string] $PlatformBuildDir)

    if ($SkipTests -in @("ON", "1", "true")) {
        Write-Host "Skipping iiPaintEngine host test build because IIPAINTENGINE_SKIP_TESTS=$SkipTests"
        return
    }

    Invoke-NativeCommand -FilePath "cmake" -Arguments (@("--build", $PlatformBuildDir, "--config", "Release", "--target") + $IiPaintEngineHostTestTargets)
}

function Run-HostTests {
    param([Parameter(Mandatory = $true)][string] $PlatformBuildDir)

    if ($SkipTests -in @("ON", "1", "true")) {
        Write-Host "Skipping iiPaintEngine host tests because IIPAINTENGINE_SKIP_TESTS=$SkipTests"
        return
    }

    Invoke-NativeCommand -FilePath "ctest" -Arguments @("--test-dir", $PlatformBuildDir, "--output-on-failure", "-E", "iiPaintEngineExampleDemoContract")
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
        "-DCMAKE_INSTALL_PREFIX=$InstallPrefix",
        "-DCMAKE_PREFIX_PATH=$cmakePrefixPath",
        "-DLVRS_DIR=$lvrsConfigDir",
        "-DIIPAINTENGINE_BUILD_SHARED=ON"
    )
    $cmakeArgs += $ExtraCMakeArgs

    Invoke-NativeCommand -FilePath "cmake" -Arguments (@("--fresh") + $cmakeArgs)

    Write-Host "Building iiPaintEngine $Platform dynamic library in $PlatformBuildDir"
    Invoke-NativeCommand -FilePath "cmake" -Arguments @("--build", $PlatformBuildDir, "--config", "Release", "--target", "iiPaintEngine")

    Write-Host "Installing iiPaintEngine $Platform package into $InstallPrefix"
    Invoke-NativeCommand -FilePath "cmake" -Arguments @("--install", $PlatformBuildDir, "--prefix", $InstallPrefix, "--config", "Release")
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

    Configure-BuildInstall -Platform "windows" -PlatformBuildDir $WindowsBuildDir -InstallPrefix $WindowsPrefix -QtPrefix $WindowsQtPrefix -LvrsPlatformPrefix $lvrsWindowsPrefix

    Write-Host "Installing iiPaintEngine windows platform mirror into $WindowsPlatformPrefix"
    Invoke-NativeCommand -FilePath "cmake" -Arguments @("--install", $WindowsBuildDir, "--prefix", $WindowsPlatformPrefix, "--config", "Release")
    Verify-DynamicLibrary -Platform "windows" -InstallPrefix $WindowsPlatformPrefix

    Write-Host "Building iiPaintEngine windows tests in $WindowsBuildDir"
    Build-HostTests -PlatformBuildDir $WindowsBuildDir

    Write-Host "Running iiPaintEngine windows tests"
    Run-HostTests -PlatformBuildDir $WindowsBuildDir
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
