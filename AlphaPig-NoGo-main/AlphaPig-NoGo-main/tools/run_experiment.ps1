# ============================================================
# AlphaPig M1.0 Tree Reuse Full Experiment Runner
# ============================================================
param(
    [int]$Games = 500,
    [string]$WorkDir = "f:\AlphaPig-NoGo\AlphaPig-NoGo-main\AlphaPig-NoGo-main"
)

$ErrorActionPreference = "Stop"
$timestamp = Get-Date -Format "yyyy_MM_dd_HHmm"
$ResultDir = Join-Path $WorkDir "results\$timestamp"
New-Item -ItemType Directory -Force -Path $ResultDir | Out-Null

$LogFile = Join-Path $ResultDir "log.txt"
function Write-Log($msg) {
    $line = "$(Get-Date -Format 'HH:mm:ss')  $msg"
    Write-Host $line
    Add-Content $LogFile $line
}

# ---- Paths ----
$BuildDir  = Join-Path $WorkDir "gui\build\Release"
$Exe       = Join-Path $BuildDir "SelfPlayValidate.exe"
$Weights   = Join-Path $BuildDir "network.weights"
$ComparePy = Join-Path $WorkDir "tools\compare_results.py"

# ---- Check build ----
if (-not (Test-Path $Exe)) {
    Write-Log "ERROR: SelfPlayValidate.exe not found at $Exe"
    Write-Log "Build first: cd gui\build && cmake --build . --config Release"
    exit 1
}

# ---- Ensure weights ----
if (-not (Test-Path $Weights)) {
    $src = Join-Path $WorkDir "work\network.weights"
    if (Test-Path $src) {
        Copy-Item $src $Weights
        Write-Log "Copied network.weights"
    } else {
        Write-Log "ERROR: network.weights not found"
        exit 1
    }
}

# ---- Create temp configs ----
$BaselineCfg = Join-Path $ResultDir "baseline_cfg.txt"
$ReuseCfg    = Join-Path $ResultDir "tree_reuse_cfg.txt"
$BaselineCsv = Join-Path $ResultDir "baseline.csv"
$ReuseCsv    = Join-Path $ResultDir "tree_reuse.csv"
$Comparison  = Join-Path $ResultDir "comparison.md"

@"
mcts.tree_reuse = false
mcts.playouts = 200
mcts.c_puct = 2.5
test.num_games = $Games
test.seed = 42
test.output = $BaselineCsv
model.weight_file = network.weights
"@ | Out-File $BaselineCfg -Encoding ASCII

@"
mcts.tree_reuse = true
mcts.playouts = 200
mcts.c_puct = 2.5
test.num_games = $Games
test.seed = 42
test.output = $ReuseCsv
model.weight_file = network.weights
"@ | Out-File $ReuseCfg -Encoding ASCII

Write-Log "Experiment: $Games games each"
Write-Log "Result dir: $ResultDir"
Write-Log "=========================================="

# ---- Phase 1: Baseline ----
Write-Log "PHASE 1: Baseline (TreeReuse OFF)"
Write-Log "Running $Games games..."
$sw = [Diagnostics.Stopwatch]::StartNew()
Push-Location $BuildDir
try {
    & $Exe $BaselineCfg 2>&1 | ForEach-Object {
        if ($_ -match "Games:") { Write-Log $_ }
        elseif ($_ -match "Done in") { Write-Log $_ }
        elseif ($_ -match "P1 wins") { Write-Log $_ }
    }
} finally {
    Pop-Location
}
$sw.Stop()
Write-Log "Baseline completed in $([math]::Round($sw.Elapsed.TotalMinutes, 1)) min"

# ---- Phase 2: TreeReuse ----
Write-Log ""
Write-Log "PHASE 2: TreeReuse ON"
Write-Log "Running $Games games..."
$sw2 = [Diagnostics.Stopwatch]::StartNew()
Push-Location $BuildDir
try {
    & $Exe $ReuseCfg 2>&1 | ForEach-Object {
        if ($_ -match "Games:") { Write-Log $_ }
        elseif ($_ -match "Done in") { Write-Log $_ }
        elseif ($_ -match "P1 wins") { Write-Log $_ }
    }
} finally {
    Pop-Location
}
$sw2.Stop()
Write-Log "TreeReuse completed in $([math]::Round($sw2.Elapsed.TotalMinutes, 1)) min"

# ---- Phase 3: Compare ----
Write-Log ""
Write-Log "PHASE 3: Generating comparison report..."
python $ComparePy $BaselineCsv $ReuseCsv 2>&1 | Out-File $Comparison -Encoding UTF8
Write-Log "Comparison saved to $Comparison"

# ---- Summary ----
Write-Log ""
Write-Log "=========================================="
Write-Log "EXPERIMENT COMPLETE"
Write-Log "Results: $ResultDir"
Write-Log "  baseline.csv"
Write-Log "  tree_reuse.csv"
Write-Log "  comparison.md"
Write-Log "  log.txt"
Write-Log "=========================================="

# Show comparison
Get-Content $Comparison
