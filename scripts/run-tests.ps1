param(
    [ValidateSet('lexer', 'parser', 'ast', 'semantic', 'codegen', 'moon', 'all')]
    [string]$Step = 'all',

    [string]$TestsRoot,

    [string]$DriverPath,

    [string]$ReportPath,

    [int]$MaxTests = 0,

    [int]$PerTestTimeoutSec = 120,

    [bool]$KillStaleProcesses = $true
)

$ErrorActionPreference = 'Stop'

function Write-Rule {
    param(
        [char]$Char = '-',
        [int]$Width = 92
    )

    Write-Host ($Char.ToString() * $Width) -ForegroundColor DarkGray
}

function Write-Log {
    param(
        [ValidateSet('INFO', 'PASS', 'FAIL', 'WARN')]
        [string]$Level,
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    $ts = Get-Date -Format 'HH:mm:ss'
    $prefix = "[$ts] [$Level]"

    switch ($Level) {
        'INFO' { $color = 'Cyan' }
        'PASS' { $color = 'Green' }
        'FAIL' { $color = 'Red' }
        'WARN' { $color = 'Yellow' }
        default { $color = 'White' }
    }

    Write-Host "$prefix $Message" -ForegroundColor $color
}

function Format-Duration {
    param([int]$Milliseconds)

    if ($Milliseconds -ge 1000) {
        return ('{0:N2}s' -f ($Milliseconds / 1000.0))
    }

    return ("{0}ms" -f $Milliseconds)
}

function Get-RepoProcesses {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    $repoExeRoot = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot 'exe'))
    $matches = @()

    foreach ($name in $Names) {
        $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
        foreach ($proc in $procs) {
            $procPath = $null
            try {
                $procPath = $proc.Path
            } catch {
                $procPath = $null
            }

            if (-not $procPath) {
                continue
            }

            $fullProcPath = [System.IO.Path]::GetFullPath($procPath)
            if ($fullProcPath.StartsWith($repoExeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                $matches += $proc
            }
        }
    }

    return @($matches | Sort-Object Id -Unique)
}

function Stop-RepoProcesses {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    $stale = Get-RepoProcesses -RepoRoot $RepoRoot -Names $Names
    foreach ($proc in $stale) {
        try {
            Stop-Process -Id $proc.Id -Force -ErrorAction Stop
        } catch {
            # best effort cleanup
        }
    }

    return $stale.Count
}

function Invoke-DriverWithTimeout {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Driver,
        [Parameter(Mandatory = $true)]
        [string]$TestFile,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSec,
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [bool]$KillStale
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    $proc = $null

    try {
        if ($KillStale) {
            [void](Stop-RepoProcesses -RepoRoot $RepoRoot -Names @('moon', 'final_driver'))
        }

        $proc = Start-Process -FilePath $Driver -ArgumentList @($TestFile) -NoNewWindow -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

        $timedOut = $false
        if ($TimeoutSec -gt 0) {
            if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
                $timedOut = $true
                try {
                    Stop-Process -Id $proc.Id -Force -ErrorAction Stop
                } catch {
                    # process may have exited during timeout handling
                }

                if ($KillStale) {
                    [void](Stop-RepoProcesses -RepoRoot $RepoRoot -Names @('moon', 'final_driver'))
                }
            }
        } else {
            $proc.WaitForExit()
        }

        if (-not $timedOut -and -not $proc.HasExited) {
            $proc.WaitForExit()
        }

        $outLines = @()
        if (Test-Path -LiteralPath $stdoutPath) {
            $outLines += @(Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue)
        }
        if (Test-Path -LiteralPath $stderrPath) {
            $outLines += @(Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue)
        }

        return [ordered]@{
            exitCode = if ($timedOut) { 124 } else { $proc.ExitCode }
            timedOut = $timedOut
            output = @($outLines)
        }
    } finally {
        Remove-Item -LiteralPath $stdoutPath -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stderrPath -ErrorAction SilentlyContinue
    }
}

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function To-RelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $rootAbs = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Root).Path)

    if (Test-Path -LiteralPath $Path) {
        $pathAbs = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path)
    } else {
        $pathAbs = [System.IO.Path]::GetFullPath($Path)
    }

    if ($pathAbs.StartsWith($rootAbs, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $pathAbs.Substring($rootAbs.Length)
        $relative = $relative -replace '^[\\/]+', ''
        return ($relative -replace '\\', '/')
    }

    return ($pathAbs -replace '\\', '/')
}

function Find-Driver {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [string]$Candidate
    )

    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }

    $candidates = @(
        (Join-Path $RepoRoot 'exe/final_driver.exe'),
        (Join-Path $RepoRoot 'exe/driver.exe')
    )

    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    throw "Could not find driver executable. Provide -DriverPath explicitly."
}

function Get-StepFolders {
    param([string]$SelectedStep)

    switch ($SelectedStep) {
        'lexer'   { return @('LEXER') }
        'parser'  { return @('Parser') }
        'ast'     { return @('AST') }
        'semantic'{ return @('Semantic') }
        'codegen' { return @('CodeGen') }
        'moon'    { return @('CodeGen') }
        'all'     { return @('LEXER', 'Parser', 'AST', 'Semantic', 'CodeGen') }
        default   { throw "Unsupported step: $SelectedStep" }
    }
}

function Get-TargetPhases {
    param([string]$SelectedStep)

    if ($SelectedStep -eq 'all') {
        return @('lexer', 'parser', 'ast', 'semantic', 'codegen', 'moon')
    }

    return @($SelectedStep)
}

function Get-NonEmptyLines {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return @()
    }

    $lines = Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue
    return @($lines | Where-Object { $_ -and $_.Trim().Length -gt 0 })
}

function Get-PhaseResult {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Phase,
        [Parameter(Mandatory = $true)]
        [string]$OutputBase,
        [Parameter(Mandatory = $true)]
        [string]$BaseName,
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    switch ($Phase) {
        'lexer' {
            $path = Join-Path $OutputBase ("Lexer/{0}.outlexerrors" -f $BaseName)
            if (-not (Test-Path -LiteralPath $path)) {
                return [ordered]@{
                    passed = $false
                    details = 'Missing lexer error artifact'
                    metrics = [ordered]@{ errorCount = -1 }
                    artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
                }
            }

            $errors = Get-NonEmptyLines -Path $path
            return [ordered]@{
                passed = ($errors.Count -eq 0)
                details = ("{0} lexical errors" -f $errors.Count)
                metrics = [ordered]@{ errorCount = $errors.Count }
                artifact = (To-RelativePath -Root $RepoRoot -Path $path)
            }
        }
        'parser' {
            $path = Join-Path $OutputBase ("Parser/{0}.outsyntaxerrors" -f $BaseName)
            if (-not (Test-Path -LiteralPath $path)) {
                return [ordered]@{
                    passed = $false
                    details = 'Missing parser error artifact'
                    metrics = [ordered]@{ errorCount = -1 }
                    artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
                }
            }

            $errors = Get-NonEmptyLines -Path $path
            return [ordered]@{
                passed = ($errors.Count -eq 0)
                details = ("{0} syntax errors" -f $errors.Count)
                metrics = [ordered]@{ errorCount = $errors.Count }
                artifact = (To-RelativePath -Root $RepoRoot -Path $path)
            }
        }
        'ast' {
            $astPath = Join-Path $OutputBase ("AST/{0}.outast" -f $BaseName)
            $dotPath = Join-Path $OutputBase ("AST/{0}.outast.dot" -f $BaseName)
            $pngPath = Join-Path $OutputBase ("AST/{0}.outast.png" -f $BaseName)

            $hasAst = Test-Path -LiteralPath $astPath
            $hasDot = Test-Path -LiteralPath $dotPath
            $hasPng = Test-Path -LiteralPath $pngPath

            $passed = $hasAst -and $hasDot
            return [ordered]@{
                passed = $passed
                details = ("ast={0}, dot={1}, png={2}" -f $hasAst, $hasDot, $hasPng)
                metrics = [ordered]@{
                    hasAst = $hasAst
                    hasDot = $hasDot
                    hasPng = $hasPng
                }
                artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
            }
        }
        'semantic' {
            $path = Join-Path $OutputBase ("Semantics/{0}.outsemanticerrors" -f $BaseName)
            if (-not (Test-Path -LiteralPath $path)) {
                return [ordered]@{
                    passed = $false
                    details = 'Missing semantic diagnostics artifact'
                    metrics = [ordered]@{ errorCount = -1; warningCount = -1 }
                    artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
                }
            }

            $lines = Get-NonEmptyLines -Path $path
            $errors = @($lines | Where-Object { $_ -match '\[ERROR\]' })
            $warnings = @($lines | Where-Object { $_ -match '\[WARN|WARNING\]' })

            return [ordered]@{
                passed = ($errors.Count -eq 0)
                details = ("{0} semantic errors, {1} warnings" -f $errors.Count, $warnings.Count)
                metrics = [ordered]@{
                    errorCount = $errors.Count
                    warningCount = $warnings.Count
                }
                artifact = (To-RelativePath -Root $RepoRoot -Path $path)
            }
        }
        'codegen' {
            $diagPath = Join-Path $OutputBase ("CodeGen/{0}.outcodegenerrors" -f $BaseName)
            $moonPath = Join-Path $OutputBase ("CodeGen/{0}.moon" -f $BaseName)

            if (-not (Test-Path -LiteralPath $diagPath)) {
                return [ordered]@{
                    passed = $false
                    details = 'Missing codegen diagnostics artifact'
                    metrics = [ordered]@{ errorCount = -1; hasMoon = (Test-Path -LiteralPath $moonPath) }
                    artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
                }
            }

            $lines = Get-NonEmptyLines -Path $diagPath
            $errors = @($lines | Where-Object { $_ -match '\[ERROR\]' })
            $hasMoon = Test-Path -LiteralPath $moonPath
            $passed = ($errors.Count -eq 0) -and $hasMoon

            return [ordered]@{
                passed = $passed
                details = ("{0} codegen errors, moon={1}" -f $errors.Count, $hasMoon)
                metrics = [ordered]@{
                    errorCount = $errors.Count
                    hasMoon = $hasMoon
                }
                artifact = (To-RelativePath -Root $RepoRoot -Path $diagPath)
            }
        }
        'moon' {
            $runPath = Join-Path $OutputBase ("CodeGen/{0}.outmoonrun" -f $BaseName)
            if (-not (Test-Path -LiteralPath $runPath)) {
                return [ordered]@{
                    passed = $false
                    details = 'Missing moon run log artifact'
                    metrics = [ordered]@{ errorCount = -1 }
                    artifact = (To-RelativePath -Root $RepoRoot -Path $OutputBase)
                }
            }

            $lines = Get-NonEmptyLines -Path $runPath
            $errors = @($lines | Where-Object { $_ -match '\[ERROR\]\[MOON\]' })
            return [ordered]@{
                passed = ($errors.Count -eq 0)
                details = ("{0} moon runtime errors" -f $errors.Count)
                metrics = [ordered]@{ errorCount = $errors.Count }
                artifact = (To-RelativePath -Root $RepoRoot -Path $runPath)
            }
        }
        default {
            throw "Unsupported phase: $Phase"
        }
    }
}

$repoRoot = Resolve-RepoRoot

if (-not $TestsRoot) {
    $TestsRoot = Join-Path $repoRoot 'My-tests'
}

if (-not (Test-Path -LiteralPath $TestsRoot)) {
    throw "Tests root does not exist: $TestsRoot"
}

$driver = Find-Driver -RepoRoot $repoRoot -Candidate $DriverPath
$stepFolders = Get-StepFolders -SelectedStep $Step
$targetPhases = Get-TargetPhases -SelectedStep $Step

$testPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

foreach ($folder in $stepFolders) {
    $folderPath = Join-Path $TestsRoot $folder
    if (-not (Test-Path -LiteralPath $folderPath)) {
        continue
    }

    $files = Get-ChildItem -LiteralPath $folderPath -Filter '*.src' -File -Recurse
    foreach ($file in $files) {
        [void]$testPaths.Add((Resolve-Path -LiteralPath $file.FullName).Path)
    }
}

$testFiles = @($testPaths | Sort-Object)

if ($MaxTests -gt 0) {
    $testFiles = @($testFiles | Select-Object -First $MaxTests)
}

if ($testFiles.Count -eq 0) {
    throw "No test files found for step '$Step' under $TestsRoot"
}

if (-not $ReportPath) {
    $reportDir = Join-Path $repoRoot 'output/test-reports'
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ReportPath = Join-Path $reportDir ("test-stats-{0}-{1}.json" -f $Step, $timestamp)
}

$reportDirParent = Split-Path -Path $ReportPath -Parent
if ($reportDirParent) {
    New-Item -ItemType Directory -Path $reportDirParent -Force | Out-Null
}

$totalTests = $testFiles.Count
$progressActivity = ("Running compiler tests [{0}]" -f $Step.ToUpperInvariant())
$runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host ''
Write-Rule -Char '='
Write-Host 'Compiler Test Runner' -ForegroundColor Cyan
Write-Rule -Char '='
Write-Log -Level INFO -Message ("Step          : {0}" -f $Step)
Write-Log -Level INFO -Message ("Target phases : {0}" -f (($targetPhases -join ', ')))
Write-Log -Level INFO -Message ("Tests root    : {0}" -f (To-RelativePath -Root $repoRoot -Path $TestsRoot))
Write-Log -Level INFO -Message ("Driver        : {0}" -f (To-RelativePath -Root $repoRoot -Path $driver))
Write-Log -Level INFO -Message ("Discovered    : {0} test files" -f $totalTests)
Write-Log -Level INFO -Message ("Timeout       : {0}s per test (0 disables timeout)" -f $PerTestTimeoutSec)

if ($KillStaleProcesses) {
    $killedAtStart = Stop-RepoProcesses -RepoRoot $repoRoot -Names @('moon', 'final_driver')
    if ($killedAtStart -gt 0) {
        Write-Log -Level WARN -Message ("Cleaned up {0} stale compiler processes before run" -f $killedAtStart)
    }
}

Write-Rule

$results = @()
$oldNoColor = $env:NO_COLOR
$env:NO_COLOR = '1'

try {
    $index = 0
    foreach ($testFile in $testFiles) {
        $index++
        $testRelativePath = To-RelativePath -Root $repoRoot -Path $testFile
        $percentStart = [int](($index - 1) * 100.0 / [Math]::Max($totalTests, 1))

        Write-Progress -Id 1 `
            -Activity $progressActivity `
            -Status ("[{0}/{1}] Running {2}" -f $index, $totalTests, $testRelativePath) `
            -PercentComplete $percentStart

        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($testFile)
        $outputBase = Join-Path $repoRoot ("output/{0}" -f $baseName)
        $start = Get-Date

        try {
            $runResult = Invoke-DriverWithTimeout -Driver $driver -TestFile $testFile -TimeoutSec $PerTestTimeoutSec -RepoRoot $repoRoot -KillStale $KillStaleProcesses
            $consoleOutput = @($runResult.output)
            $exitCode = $runResult.exitCode
            $elapsedMs = [int][Math]::Round(((Get-Date) - $start).TotalMilliseconds)

            if ($runResult.timedOut) {
                throw "Driver timed out after $PerTestTimeoutSec seconds"
            }

            $phaseMap = [ordered]@{}
            foreach ($phase in $targetPhases) {
                $phaseMap[$phase] = Get-PhaseResult -Phase $phase -OutputBase $outputBase -BaseName $baseName -RepoRoot $repoRoot
            }

            $allTargetedPassed = $true
            foreach ($phase in $targetPhases) {
                if (-not $phaseMap[$phase].passed) {
                    $allTargetedPassed = $false
                    break
                }
            }

            $phaseBadges = @(
                foreach ($phase in $targetPhases) {
                    if ($phaseMap[$phase].passed) {
                        "{0}=OK" -f $phase
                    } else {
                        "{0}=FAIL" -f $phase
                    }
                }
            ) -join ', '

            if ($allTargetedPassed) {
                Write-Log -Level PASS -Message ("[{0}/{1}] {2} ({3}) :: {4}" -f $index, $totalTests, $testRelativePath, (Format-Duration -Milliseconds $elapsedMs), $phaseBadges)
            } else {
                Write-Log -Level FAIL -Message ("[{0}/{1}] {2} ({3}) :: {4}" -f $index, $totalTests, $testRelativePath, (Format-Duration -Milliseconds $elapsedMs), $phaseBadges)
            }

            $results += [ordered]@{
                testFile = $testRelativePath
                exitCode = $exitCode
                durationMs = $elapsedMs
                allTargetedPhasesPassed = $allTargetedPassed
                phases = $phaseMap
                consoleTail = @($consoleOutput | Select-Object -Last 5)
            }
        } catch {
            $elapsedMs = [int][Math]::Round(((Get-Date) - $start).TotalMilliseconds)
            $err = $_.Exception.Message
            Write-Log -Level FAIL -Message ("[{0}/{1}] {2} ({3}) :: runner error: {4}" -f $index, $totalTests, $testRelativePath, (Format-Duration -Milliseconds $elapsedMs), $err)

            $phaseMap = [ordered]@{}
            foreach ($phase in $targetPhases) {
                $phaseMap[$phase] = [ordered]@{
                    passed = $false
                    details = ("Runner error: {0}" -f $err)
                    metrics = [ordered]@{ errorCount = -1 }
                    artifact = (To-RelativePath -Root $repoRoot -Path $outputBase)
                }
            }

            $results += [ordered]@{
                testFile = $testRelativePath
                exitCode = 1
                durationMs = $elapsedMs
                allTargetedPhasesPassed = $false
                phases = $phaseMap
                consoleTail = @($err)
            }
        }

        $percentDone = [int]($index * 100.0 / [Math]::Max($totalTests, 1))
        Write-Progress -Id 1 `
            -Activity $progressActivity `
            -Status ("[{0}/{1}] Completed {2}" -f $index, $totalTests, $testRelativePath) `
            -PercentComplete $percentDone
    }
} finally {
    Write-Progress -Id 1 -Activity $progressActivity -Completed

    if ($null -eq $oldNoColor) {
        Remove-Item Env:NO_COLOR -ErrorAction SilentlyContinue
    } else {
        $env:NO_COLOR = $oldNoColor
    }
}

$phaseSummary = [ordered]@{}
foreach ($phase in $targetPhases) {
    $passedCount = @($results | Where-Object { $_.phases[$phase].passed }).Count
    $failedCount = $results.Count - $passedCount
    $phaseSummary[$phase] = [ordered]@{
        total = $results.Count
        passed = $passedCount
        failed = $failedCount
        passRatePercent = if ($results.Count -eq 0) { 0 } else { [Math]::Round(($passedCount * 100.0) / $results.Count, 2) }
    }
}

$overallPassed = @($results | Where-Object { $_.allTargetedPhasesPassed }).Count
$overallFailed = $results.Count - $overallPassed

$report = [ordered]@{
    generatedAt = (Get-Date).ToString('s')
    step = $Step
    targetedPhases = @($targetPhases)
    testsRoot = (To-RelativePath -Root $repoRoot -Path $TestsRoot)
    driverPath = (To-RelativePath -Root $repoRoot -Path $driver)
    totalTests = $results.Count
    overall = [ordered]@{
        passed = $overallPassed
        failed = $overallFailed
        passRatePercent = if ($results.Count -eq 0) { 0 } else { [Math]::Round(($overallPassed * 100.0) / $results.Count, 2) }
    }
    phaseSummary = $phaseSummary
    tests = @($results)
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8

$runStopwatch.Stop()
$totalElapsedText = Format-Duration -Milliseconds ([int]$runStopwatch.ElapsedMilliseconds)
$passRate = if ($results.Count -eq 0) { 0 } else { [Math]::Round(($overallPassed * 100.0) / $results.Count, 2) }

$failedExamples = @($results | Where-Object { -not $_.allTargetedPhasesPassed } | Select-Object -First 5)

Write-Rule
Write-Host 'Execution Summary' -ForegroundColor Cyan
Write-Rule
Write-Log -Level INFO -Message ("Total tests    : {0}" -f $results.Count)
Write-Log -Level INFO -Message ("Passed / Failed: {0} / {1}" -f $overallPassed, $overallFailed)
Write-Log -Level INFO -Message ("Pass rate      : {0}%" -f $passRate)
Write-Log -Level INFO -Message ("Duration       : {0}" -f $totalElapsedText)
Write-Log -Level INFO -Message ("Report         : {0}" -f (To-RelativePath -Root $repoRoot -Path $ReportPath))

if ($failedExamples.Count -gt 0) {
    Write-Log -Level WARN -Message 'Sample failures:'
    foreach ($failed in $failedExamples) {
        Write-Host ("  - {0}" -f $failed.testFile) -ForegroundColor Yellow
    }
}

Write-Rule -Char '='
