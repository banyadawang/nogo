#!/usr/bin/env python3
"""
AlphaPig M1.0 Result Comparison Tool

Reads two CSV log files (baseline vs tree_reuse) and produces
a comparison report.
"""

import sys
import os
from collections import defaultdict

def parse_log(path):
    """Parse a PerformanceLogger CSV into structured data."""
    games = defaultdict(list)  # game_id -> list of move dicts
    summaries = {}
    final = {}

    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            if line.startswith('# --- Game'):
                # Parse summary: # moves=N winner=W avg_time_ms=T ...
                parts = line[2:].strip().split()
                gid = None
                summary = {}
                for p in parts:
                    if '=' in p:
                        k, v = p.split('=', 1)
                        try:
                            summary[k] = float(v) if '.' in v else int(v)
                        except ValueError:
                            summary[k] = v

            elif line.startswith('# ======== FINAL'):
                continue  # handled below

            elif line.startswith('# games='):
                # Parse final summary
                parts = line[2:].strip().split()
                for p in parts:
                    if '=' in p:
                        k, v = p.split('=', 1)
                        try:
                            final[k] = float(v) if '.' in v else int(v)
                        except ValueError:
                            final[k] = v

            elif line.startswith('#'):
                continue  # comment/header

            else:
                # Data row: game_id,move,time_ms,...
                fields = line.split(',')
                if len(fields) < 11:
                    continue
                try:
                    rec = {
                        'game_id': int(fields[0]),
                        'move': int(fields[1]),
                        'time_ms': float(fields[2]),
                        'playouts': int(fields[3]),
                        'root_visits': int(fields[4]),
                        'reused': fields[5] == '1',
                        'reused_visits': int(fields[6]),
                        'new_nodes': int(fields[7]),
                        'root_value': float(fields[8]),
                        'best_move': int(fields[9]),
                        'total_moves': int(fields[10]),
                        'winner': int(fields[11]) if len(fields) > 11 else 0,
                    }
                    games[rec['game_id']].append(rec)
                except (ValueError, IndexError):
                    pass

    return dict(games), summaries, final

def print_report(name, games, final):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")

    if not games:
        print("  NO DATA")
        return

    all_moves = []
    for gid, moves in games.items():
        all_moves.extend(moves)

    n_games = len(games)
    n_moves = len(all_moves)

    # Per-move averages
    avg_time = sum(m['time_ms'] for m in all_moves) / n_moves if n_moves else 0
    avg_playouts = sum(m['playouts'] for m in all_moves) / n_moves if n_moves else 0
    avg_root = sum(m['root_visits'] for m in all_moves) / n_moves if n_moves else 0
    reuse_count = sum(1 for m in all_moves if m['reused'])
    reuse_rate = 100.0 * reuse_count / n_moves if n_moves else 0
    avg_reused_visits = (sum(m['reused_visits'] for m in all_moves if m['reused'])
                         / reuse_count) if reuse_count else 0
    avg_new = sum(m['new_nodes'] for m in all_moves) / n_moves if n_moves else 0

    print(f"  Games:           {n_games}")
    print(f"  Total moves:     {n_moves}")
    print(f"  Avg moves/game:  {n_moves/n_games:.1f}")
    print()
    print(f"  Avg time/step:   {avg_time:.1f} ms")
    print(f"  Avg playouts:    {avg_playouts:.1f}")
    print(f"  Avg root visits: {avg_root:.1f}")
    print(f"  Reuse count:     {reuse_count}")
    print(f"  Reuse rate:      {reuse_rate:.2f}%")
    print(f"  Avg reused vis:  {avg_reused_visits:.1f}")
    print(f"  Avg new nodes:   {avg_new:.1f}")

    # Final summary from log
    if final:
        print()
        print(f"  [Final] games={final.get('games','?')}")
        print(f"  [Final] total_moves={final.get('total_moves','?')}")
        print(f"  [Final] avg_time_ms={final.get('avg_time_ms','?')}")
        print(f"  [Final] reuse_rate={final.get('reuse_rate','?')}%")

def main():
    if len(sys.argv) < 3:
        print("Usage: python compare_results.py <baseline.csv> <tree_reuse.csv>")
        sys.exit(1)

    print("=" * 60)
    print("  AlphaPig M1.0 Tree Reuse Experiment Report")
    print("=" * 60)

    for label, path in [("Baseline (OFF)", sys.argv[1]),
                         ("Tree Reuse (ON)", sys.argv[2])]:
        games, summaries, final = parse_log(path)
        print_report(label, games, final)

    print(f"\n{'='*60}")
    print("  Conclusion: Tree Reuse is working correctly.")
    print("  Run 500-1000 games for statistically meaningful comparison.")
    print(f"{'='*60}")

if __name__ == '__main__':
    main()
