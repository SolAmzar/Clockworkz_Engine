#!/usr/bin/env python3
"""
repo-activity: Per-file contribution pivot table from git log.
Usage (interactive):  python main.py
Usage (flags):        python main.py --branch main --since "5 May" --until "10 July" --net
                      python main.py --comments   (count comment lines only)
"""

from __future__ import annotations

import re
import subprocess
import sys
from collections import defaultdict
from datetime import datetime, date
from typing import Optional

import typer
from tabulate import tabulate


# ---------------------------------------------------------------------------
# Comment detection helpers
# ---------------------------------------------------------------------------

# Map file extension -> set of line-comment prefixes (stripped)
_EXT_COMMENT_PREFIXES: dict[str, tuple[str, ...]] = {
    ".cpp": ("//", "/*", "*"),
    ".hpp": ("//", "/*", "*"),
    ".h":   ("//", "/*", "*"),
    ".c":   ("//", "/*", "*"),
    ".cc":  ("//", "/*", "*"),
    ".cs":  ("//", "/*", "*"),
    ".java":("//", "/*", "*"),
    ".js":  ("//", "/*", "*"),
    ".ts":  ("//", "/*", "*"),
    ".tsx": ("//", "/*", "*"),
    ".jsx": ("//", "/*", "*"),
    ".py":  ("#",),
    ".sh":  ("#",),
    ".rb":  ("#",),
    ".pl":  ("#",),
    ".lua": ("--",),
    ".sql": ("--",),
    ".html":("<!--",),
    ".xml": ("<!--",),
    ".css": ("/*", "*"),
    ".scss":("//", "/*", "*"),
}


def _is_comment_line(line: str, ext: str) -> bool:
    prefixes = _EXT_COMMENT_PREFIXES.get(ext.lower())
    if prefixes is None:
        return False
    stripped = line.strip()
    return any(stripped.startswith(p) for p in prefixes)

app = typer.Typer(add_completion=False)


def _current_branch() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "--abbrev-ref", "HEAD"],
        capture_output=True, text=True
    )
    return result.stdout.strip() or "main"

# ---------------------------------------------------------------------------
# Date parsing
# ---------------------------------------------------------------------------

_MONTH_NAMES = {
    "jan": 1, "feb": 2, "mar": 3, "apr": 4, "may": 5, "jun": 6,
    "jul": 7, "aug": 8, "sep": 9, "oct": 10, "nov": 11, "dec": 12,
}

_HUMAN_DATE_RE = re.compile(
    r"^(\d{1,2})\s+([A-Za-z]+)(?:\s+(\d{4}))?$"   # "5 May" or "5 May 2025"
    r"|^([A-Za-z]+)\s+(\d{1,2})(?:\s+(\d{4}))?$"   # "May 5" or "May 5 2025"
)


def _parse_date(raw: str) -> str:
    """
    Try to turn a human date into YYYY-MM-DD.
    Accepts: "5 May", "May 5", "5 May 2025", "2025-05-05", "today", git relative dates.
    Unknown formats are returned as-is (git accepts "1 month ago", "yesterday", etc.).
    """
    raw = raw.strip()
    if raw.lower() == "today":
        return date.today().isoformat()

    # Already ISO
    if re.match(r"^\d{4}-\d{2}-\d{2}$", raw):
        return raw

    m = _HUMAN_DATE_RE.match(raw)
    if m:
        if m.group(1):  # "5 May [year]"
            day, mon_str, yr = m.group(1), m.group(2), m.group(3)
        else:            # "May 5 [year]"
            mon_str, day, yr = m.group(4), m.group(5), m.group(6)

        mon = _MONTH_NAMES.get(mon_str[:3].lower())
        if mon:
            year = int(yr) if yr else datetime.now().year
            return f"{year:04d}-{mon:02d}-{int(day):02d}"

    # Fall through: let git interpret it (e.g. "1 month ago")
    return raw


# ---------------------------------------------------------------------------
# Git data collection
# ---------------------------------------------------------------------------

def _run_git(branch: str, since: str, until: str) -> str:
    cmd = [
        "git", "log", branch,
        f"--since={since}",
        f"--until={until}",
        "--pretty=format:COMMIT|%an",
        "--numstat",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        typer.echo(f"[git error] {result.stderr.strip()}", err=True)
        raise typer.Exit(1)
    return result.stdout


def _run_git_patch(branch: str, since: str, until: str) -> str:
    cmd = [
        "git", "log", branch,
        f"--since={since}",
        f"--until={until}",
        "--pretty=format:COMMIT|%an",
        "-p", "--diff-filter=ACMR",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        typer.echo(f"[git error] {result.stderr.strip()}", err=True)
        raise typer.Exit(1)
    return result.stdout


def _parse_patch_comments(output: str) -> dict[tuple[str, str], tuple[int, int]]:
    """
    Returns {(author, filename): (comment_lines_added, comment_lines_deleted)}.
    Only counts lines that match comment syntax for the file's extension.
    """
    stats: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0])
    current_author = "<unknown>"
    current_file = ""
    current_ext = ""

    for line in output.splitlines():
        if line.startswith("COMMIT|"):
            current_author = line[len("COMMIT|"):]
            continue
        # diff --git a/path b/path
        if line.startswith("diff --git "):
            m = re.search(r' b/(.+)$', line)
            if m:
                current_file = m.group(1)
                dot = current_file.rfind(".")
                current_ext = current_file[dot:] if dot != -1 else ""
            continue
        # Skip diff metadata lines
        if line.startswith("+++") or line.startswith("---") or line.startswith("@@"):
            continue
        if line.startswith("+") and not line.startswith("+++"):
            content = line[1:]
            if _is_comment_line(content, current_ext):
                stats[(current_author, current_file)][0] += 1
        elif line.startswith("-") and not line.startswith("---"):
            content = line[1:]
            if _is_comment_line(content, current_ext):
                stats[(current_author, current_file)][1] += 1

    return {k: (v[0], v[1]) for k, v in stats.items()}


def _parse_numstat(output: str) -> dict[tuple[str, str], tuple[int, int]]:
    """
    Returns {(author, filename): (added, deleted)}.
    Skips binary files (reported as '-').
    """
    stats: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0])
    current_author = "<unknown>"

    for line in output.splitlines():
        if line.startswith("COMMIT|"):
            current_author = line[len("COMMIT|"):]
            continue
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        added_raw, deleted_raw, filename = parts
        if added_raw == "-" or deleted_raw == "-":
            continue  # binary file
        try:
            added, deleted = int(added_raw), int(deleted_raw)
        except ValueError:
            continue
        stats[(current_author, filename)][0] += added
        stats[(current_author, filename)][1] += deleted

    return {k: (v[0], v[1]) for k, v in stats.items()}


# ---------------------------------------------------------------------------
# Pivot table builder
# ---------------------------------------------------------------------------

def _build_pivot(
    stats: dict[tuple[str, str], tuple[int, int]],
    show_net: bool,
) -> tuple[list[str], list[list]]:
    """
    Returns (headers, rows) ready for tabulate.
    headers = ["Filename", author1, author2, ...]
    rows    = [[filename, val, val, ...], ..., ["TOTAL", ...]]
    Values are net (added-deleted) or added only depending on show_net.
    """
    if not stats:
        return [], []

    authors: list[str] = sorted({author for author, _ in stats})
    files: set[str] = {fname for _, fname in stats}

    # Per-file totals for sorting
    file_total: dict[str, int] = defaultdict(int)
    for (author, fname), (added, deleted) in stats.items():
        val = (added - deleted) if show_net else added
        file_total[fname] += val

    sorted_files = sorted(
        (f for f in files if file_total[f] != 0),
        key=lambda f: abs(file_total[f]),
        reverse=True,
    )

    def fmt(val: int) -> str:
        if val == 0:
            return "0"
        return f"+{val}" if val > 0 else str(val)

    rows: list[list] = []
    col_totals: dict[str, int] = defaultdict(int)

    for fname in sorted_files:
        row = [fname]
        for author in authors:
            added, deleted = stats.get((author, fname), (0, 0))
            val = (added - deleted) if show_net else added
            col_totals[author] += val
            row.append(fmt(val))
        rows.append(row)

    # Totals row
    total_row = ["TOTAL"] + [fmt(col_totals[a]) for a in authors]
    rows.append(total_row)

    headers = ["Filename"] + authors
    return headers, rows


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

@app.command()
def main(
    branch:   Optional[str]  = typer.Option(None, "--branch", "-b", help="Git branch"),
    since:    Optional[str]  = typer.Option(None, "--since", "-s", help="Start date"),
    until:    Optional[str]  = typer.Option(None, "--until", "-u", help="End date"),
    net:      Optional[bool] = typer.Option(None, "--net/--no-net", "-n", help="Show net lines (added - deleted)"),
    comments: Optional[bool] = typer.Option(None, "--comments/--no-comments", "-c", help="Count comment lines only"),
) -> None:
    # --- Interactive prompts for any missing flags ---
    if branch is None:
        branch = typer.prompt("Enter branch", default=_current_branch())
    if since is None:
        since = typer.prompt("Enter start date", default="1 month ago")
    if until is None:
        until = typer.prompt("Enter end date", default="today")
    if net is None:
        ans = typer.prompt("Show net lines (added - deleted)? [y/n]", default="y")
        net = ans.strip().lower() in ("y", "yes", "1", "true")
    if comments is None:
        ans = typer.prompt("Count comment lines only? [y/n]", default="n")
        comments = ans.strip().lower() in ("y", "yes", "1", "true")

    since_parsed = _parse_date(since)
    until_parsed = _parse_date(until)

    mode_label = "comments-only" if comments else ("net" if net else "added-only")
    typer.echo(
        f"\nQuerying: git log {branch}  since={since_parsed}  until={until_parsed}  {mode_label}\n"
    )

    if comments:
        raw = _run_git_patch(branch, since_parsed, until_parsed)
        stats = _parse_patch_comments(raw)
    else:
        raw = _run_git(branch, since_parsed, until_parsed)
        stats = _parse_numstat(raw)

    stats = {k: v for k, v in stats.items() if not k[1].endswith(".json")}

    if not stats:
        typer.echo("No file-level changes found for that range.")
        raise typer.Exit(0)

    headers, rows = _build_pivot(stats, show_net=net)

    # Separate the TOTAL row for styling
    data_rows = rows[:-1]
    total_row = rows[-1]

    table = tabulate(
        data_rows + [total_row],
        headers=headers,
        tablefmt="simple",
        colalign=("left",) + ("right",) * (len(headers) - 1),
    )

    # Insert a separator line before the TOTAL row
    lines = table.splitlines()
    # Find the last data row index — insert separator before it
    # tabulate "simple" format: line 0 = header, line 1 = dashes, lines 2..N = data
    sep_line = lines[1]  # reuse the dash separator from tabulate
    lines.insert(-1, sep_line)

    typer.echo("\n" + "\n".join(lines) + "\n")
    input("Press Enter to close...")


if __name__ == "__main__":
    app()
