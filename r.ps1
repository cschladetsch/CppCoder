<#
.SYNOPSIS
    Single entry point for CppCoder: init submodules, configure, build,
    test, and optionally run a research question or open the web UI.

.PARAMETER Clean
    Remove the build/ directory first.

.PARAMETER SkipBuild
    Skip configure/build entirely (implies -SkipTests unless tests were
    already built). Useful with -Question or -OpenWeb only.

.PARAMETER SkipTests
    Skip running ctest after building.

.PARAMETER Jobs
    Parallel build jobs. Defaults to the processor count.

.PARAMETER Question
    If given, runs build/src/cppcoder against -Codebase with this question
    after building.

.PARAMETER Codebase
    Codebase root to research when -Question is given. Defaults to this
    repo's root.

.PARAMETER Model
    Ollama model tag passed to cppcoder. Defaults to qwen2.5-coder:7b.

.PARAMETER EventsFile
    Path to write JSON-Lines engine events when -Question is given.
    Defaults to build/last_run.jsonl.

.PARAMETER LogLevel
    Log level passed to cppcoder: trace|debug|info|warn|err|critical|off.

.PARAMETER OpenWeb
    Opens web/index.html in the default browser after everything else.

.EXAMPLE
    ./r.ps1
    Init submodules (if needed), configure, build, run all 88 tests.

.EXAMPLE
    ./r.ps1 -Question "How does the judge prune directions?" -Codebase .
    Build, then run cppcoder with that question against this repo,
    writing events to build/last_run.jsonl.

.EXAMPLE
    ./r.ps1 -SkipBuild -OpenWeb
    Just open the task-graph UI (e.g. to load a previously recorded
    events file by hand) without touching the build.

.EXAMPLE
    ./r.ps1 -Clean -Jobs 8
    Full rebuild from scratch with 8 parallel jobs.
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [int]$Jobs = 0,

    [string]$Question,
    [string]$Codebase = $PSScriptRoot,
    [string]$Model = 'qwen2.5-coder:7b',
    [string]$EventsFile,
    [ValidateSet('trace', 'debug', 'info', 'warn', 'err', 'critical', 'off')]
    [string]$LogLevel = 'info',

    [switch]$OpenWeb
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Test-CommandExists {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

# ---------------------------------------------------------------------------
# 1. Submodules (external/CppLmmModelStore, external/spdlog, external/googletest)
# ---------------------------------------------------------------------------
if (-not $SkipBuild -and (Test-Path (Join-Path $PSScriptRoot '.gitmodules'))) {
    $submodulePaths = @('external/CppLmmModelStore', 'external/spdlog', 'external/googletest')
    $needsInit = $false
    foreach ($dir in $submodulePaths) {
        $marker = Join-Path (Join-Path $PSScriptRoot $dir) 'CMakeLists.txt'
        if (-not (Test-Path $marker)) {
            $needsInit = $true
        }
    }

    if ($needsInit) {
        if (-not (Test-CommandExists 'git')) {
            throw "git not found on PATH; cannot initialize submodules."
        }
        Write-Step 'Initializing git submodules'
        git submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) { throw 'git submodule update failed' }
    }
}

# ---------------------------------------------------------------------------
# 2. Configure + build
# ---------------------------------------------------------------------------
if (-not $SkipBuild) {
    if ($Clean -and (Test-Path 'build')) {
        Write-Step 'Removing existing build/'
        Remove-Item -Recurse -Force 'build'
    }

    if (-not (Test-CommandExists 'cmake')) {
        throw "cmake not found on PATH. Install it first (e.g. 'sudo apt install cmake' " +
              "or 'winget install Kitware.CMake')."
    }

    Write-Step 'Configuring (cmake -B build -S .)'
    cmake -B build -S .
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

    if ($Jobs -le 0) {
        $Jobs = [Environment]::ProcessorCount
    }
    Write-Step "Building (-j $Jobs)"
    cmake --build build -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }
}

# ---------------------------------------------------------------------------
# 3. Test
# ---------------------------------------------------------------------------
if (-not $SkipBuild -and -not $SkipTests) {
    Write-Step 'Running tests (ctest)'
    Push-Location build
    try {
        ctest --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
    }
    finally {
        Pop-Location
    }
}

# ---------------------------------------------------------------------------
# 4. Optional: research a question
# ---------------------------------------------------------------------------
if ($Question) {
    $exeName = if ($IsWindows) { 'cppcoder.exe' } else { 'cppcoder' }
    $exe = Join-Path 'build' (Join-Path 'src' $exeName)
    if (-not (Test-Path $exe)) {
        throw "cppcoder executable not found at $exe -- build first (omit -SkipBuild)."
    }

    if (-not $EventsFile) {
        $EventsFile = Join-Path 'build' 'last_run.jsonl'
    }

    Write-Step "Researching: $Question"
    & $exe --question $Question --codebase $Codebase --model $Model `
        --events-file $EventsFile --log-level $LogLevel
    $exitCode = $LASTEXITCODE

    Write-Step "Events written to $EventsFile (load it in web/index.html to visualize)"
    if ($exitCode -ne 0) {
        Write-Warning "cppcoder exited with code $exitCode (no answer found -- see output above)"
    }
}

# ---------------------------------------------------------------------------
# 5. Optional: open the web UI
# ---------------------------------------------------------------------------
if ($OpenWeb) {
    $webPath = Join-Path $PSScriptRoot 'web/index.html'
    Write-Step "Opening $webPath"
    if ($IsWindows) {
        Start-Process $webPath
    }
    elseif ($IsMacOS) {
        & open $webPath
    }
    else {
        & xdg-open $webPath
    }
}

Write-Step 'Done'
