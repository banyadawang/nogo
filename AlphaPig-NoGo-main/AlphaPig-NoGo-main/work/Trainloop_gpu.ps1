# ============================================
# AlphaPig GPU Training Loop
# - GPU training (Train_torch.py)
# - CPU: all cores (full load)
# - 10000 games/round
# ============================================

# Full CPU (default OpenMP threads)

for (;;)
{
    Write-Host "=== SelfPlay (full CPU) ==="
    ./SelfPlay.exe

    Write-Host "=== GPU Training ==="
    python ./Train_torch.py
}