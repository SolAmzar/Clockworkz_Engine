#!/usr/bin/env python3
from __future__ import annotations

import argparse
import calendar as calendar_module
import subprocess
import sys
import threading
import traceback
from collections import defaultdict
from datetime import date, datetime
from pathlib import Path
from queue import Empty, Queue

import tkinter as tk
from tkinter import messagebox, ttk

from openpyxl import Workbook
from openpyxl.styles import Alignment, Font, PatternFill
from openpyxl.utils import get_column_letter
from tabulate import tabulate


DEFAULT_EXCEL_FILENAME = "Result of M6.xlsx"
ALL_AUTHORS_LABEL = "All authors"

_EXT_COMMENT_PREFIXES: dict[str, tuple[str, ...]] = {
    ".cpp": ("//", "/*", "*"),
    ".hpp": ("//", "/*", "*"),
    ".h": ("//", "/*", "*"),
    ".c": ("//", "/*", "*"),
    ".cc": ("//", "/*", "*"),
    ".cs": ("//", "/*", "*"),
    ".java": ("//", "/*", "*"),
    ".js": ("//", "/*", "*"),
    ".ts": ("//", "/*", "*"),
    ".tsx": ("//", "/*", "*"),
    ".jsx": ("//", "/*", "*"),
    ".py": ("#",),
    ".sh": ("#",),
    ".rb": ("#",),
    ".pl": ("#",),
    ".lua": ("--",),
    ".sql": ("--",),
    ".html": ("<!--",),
    ".xml": ("<!--",),
    ".css": ("/*", "*"),
    ".scss": ("//", "/*", "*"),
}


def _app_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def _repo_root() -> Path:
    candidates = [_app_dir(), Path.cwd()]
    for start in candidates:
        for path in (start, *start.parents):
            if (path / ".git").exists():
                return path
    raise RuntimeError("Could not find the git repository root from the repo-activity tool location.")


def _run_git_command(args: list[str], repo_root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        cwd=repo_root,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def _current_branch(repo_root: Path) -> str:
    result = _run_git_command(["git", "rev-parse", "--abbrev-ref", "HEAD"], repo_root)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "Failed to get current git branch.")
    return result.stdout.strip() or "main"


def _list_branches(repo_root: Path) -> list[str]:
    result = _run_git_command(["git", "branch", "--format=%(refname:short)"], repo_root)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "Failed to list git branches.")
    branches = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not branches:
        current = _current_branch(repo_root)
        return [current]
    return branches


def _list_authors(repo_root: Path) -> list[str]:
    result = _run_git_command(["git", "log", "--format=%an"], repo_root)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "Failed to list git authors.")
    authors = sorted({line.strip() for line in result.stdout.splitlines() if line.strip()})
    return [ALL_AUTHORS_LABEL, *authors] if authors else [ALL_AUTHORS_LABEL]


def _normalize_excel_name(raw_name: str) -> str:
    cleaned = Path(raw_name.strip() or DEFAULT_EXCEL_FILENAME).name
    if not cleaned.lower().endswith(".xlsx"):
        cleaned += ".xlsx"
    return cleaned


def _excel_output_path(raw_name: str) -> Path:
    return _app_dir() / _normalize_excel_name(raw_name)


def _should_exclude_file(path: str) -> bool:
    normalized = path.replace("\\", "/").lower().lstrip("./\"'")
    return normalized.endswith(".py") or normalized.startswith("tools/repo-activity/") or "/tools/repo-activity/" in normalized


def _format_display_date(value: date) -> str:
    return value.strftime("%d-%m-%Y")


def _parse_display_date(raw: str) -> date:
    cleaned = raw.strip()
    try:
        return datetime.strptime(cleaned, "%d-%m-%Y").date()
    except ValueError:
        return date.fromisoformat(cleaned)


def _mode_label(show_net: bool, comments_only: bool, show_percent: bool) -> str:
    label = "comments-only" if comments_only else ("net" if show_net else "added-only")
    if show_percent:
        label += " + percentage-share"
    return label


def _is_comment_line(line: str, ext: str) -> bool:
    prefixes = _EXT_COMMENT_PREFIXES.get(ext.lower())
    if prefixes is None:
        return False
    stripped = line.strip()
    return any(stripped.startswith(prefix) for prefix in prefixes)


def _run_git_numstat(branch: str, since: str, until: str, repo_root: Path) -> str:
    result = _run_git_command(
        [
            "git",
            "log",
            branch,
            f"--since={since}",
            f"--until={until}",
            "--pretty=format:COMMIT|%an",
            "--numstat",
        ],
        repo_root,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git log failed.")
    return result.stdout


def _run_git_patch(branch: str, since: str, until: str, repo_root: Path) -> str:
    result = _run_git_command(
        [
            "git",
            "log",
            branch,
            f"--since={since}",
            f"--until={until}",
            "--pretty=format:COMMIT|%an",
            "-p",
            "--diff-filter=ACMR",
        ],
        repo_root,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git log -p failed.")
    return result.stdout


def _parse_patch_comments(output: str) -> dict[tuple[str, str], tuple[int, int]]:
    stats: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0])
    current_author = "<unknown>"
    current_file = ""
    current_ext = ""

    for line in output.splitlines():
        if line.startswith("COMMIT|"):
            current_author = line[len("COMMIT|") :]
            continue
        if line.startswith("diff --git "):
            marker = " b/"
            idx = line.rfind(marker)
            if idx != -1:
                current_file = line[idx + len(marker) :]
                dot = current_file.rfind(".")
                current_ext = current_file[dot:] if dot != -1 else ""
            continue
        if line.startswith("+++") or line.startswith("---") or line.startswith("@@"):
            continue
        if line.startswith("+") and not line.startswith("+++"):
            if _is_comment_line(line[1:], current_ext):
                stats[(current_author, current_file)][0] += 1
        elif line.startswith("-") and not line.startswith("---"):
            if _is_comment_line(line[1:], current_ext):
                stats[(current_author, current_file)][1] += 1

    return {key: (value[0], value[1]) for key, value in stats.items()}


def _parse_numstat(output: str) -> dict[tuple[str, str], tuple[int, int]]:
    stats: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0])
    current_author = "<unknown>"

    for line in output.splitlines():
        if line.startswith("COMMIT|"):
            current_author = line[len("COMMIT|") :]
            continue
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        added_raw, deleted_raw, filename = parts
        if added_raw == "-" or deleted_raw == "-":
            continue
        try:
            added = int(added_raw)
            deleted = int(deleted_raw)
        except ValueError:
            continue
        stats[(current_author, filename)][0] += added
        stats[(current_author, filename)][1] += deleted

    return {key: (value[0], value[1]) for key, value in stats.items()}


def _format_signed(val: int) -> str:
    if val == 0:
        return "0"
    return f"+{val}" if val > 0 else str(val)


def _format_percent(val: int, total: int) -> str:
    if total <= 0:
        return "0.0%"
    return f"{(val / total) * 100:.1f}%"


def _build_pivot(
    stats: dict[tuple[str, str], tuple[int, int]],
    show_net: bool,
    show_percent: bool,
) -> tuple[list[str], list[list[str]], list[list[object]]]:
    if not stats:
        return [], [], []

    authors = sorted({author for author, _ in stats})
    files = {filename for _, filename in stats}

    file_totals: dict[str, int] = defaultdict(int)
    for (author, filename), (added, deleted) in stats.items():
        value = (added - deleted) if show_net else added
        file_totals[filename] += value

    sorted_files = sorted(
        (filename for filename in files if file_totals[filename] != 0),
        key=lambda filename: abs(file_totals[filename]),
    )

    rows: list[list[str]] = []
    excel_rows: list[list[object]] = []
    column_totals: dict[str, int] = defaultdict(int)

    for filename in sorted_files:
        values: list[int] = []
        row = [filename]
        excel_row: list[object] = [filename]
        for author in authors:
            added, deleted = stats.get((author, filename), (0, 0))
            value = (added - deleted) if show_net else added
            column_totals[author] += value
            values.append(value)
        row_total = sum(values)
        for value in values:
            if show_percent:
                numeric_value = (value / row_total) if row_total > 0 else 0.0
                row.append(_format_percent(value, row_total))
                excel_row.append(numeric_value)
            else:
                row.append(_format_signed(value))
                excel_row.append(value)
        rows.append(row)
        excel_rows.append(excel_row)

    grand_total = sum(column_totals.values())
    total_row = ["TOTAL"]
    total_excel_row: list[object] = ["TOTAL"]
    for author in authors:
        value = column_totals[author]
        if show_percent:
            numeric_value = (value / grand_total) if grand_total > 0 else 0.0
            total_row.append(_format_percent(value, grand_total))
            total_excel_row.append(numeric_value)
        else:
            total_row.append(_format_signed(value))
            total_excel_row.append(value)
    rows.append(total_row)
    excel_rows.append(total_excel_row)

    return ["Filename", *authors], rows, excel_rows


def _render_table(headers: list[str], rows: list[list[str]]) -> str:
    if not headers or not rows:
        return ""
    table = tabulate(
        rows,
        headers=headers,
        tablefmt="simple",
        colalign=("left",) + ("right",) * (len(headers) - 1),
    )
    lines = table.splitlines()
    if len(lines) > 2:
        lines.insert(-1, lines[1])
    return "\n".join(lines)


def _export_excel(
    headers: list[str],
    rows: list[list[object]],
    path: Path,
    branch: str,
    since: str,
    until: str,
    mode_label: str,
    show_percent: bool,
) -> None:
    workbook = Workbook()
    sheet = workbook.active
    sheet.title = "repo-activity"

    sheet.append([f"Branch: {branch}"])
    sheet.append([f"Since: {since}   Until: {until}   Mode: {mode_label}"])
    sheet.append([])

    header_fill = PatternFill("solid", fgColor="1F4E79")
    header_font = Font(bold=True, color="FFFFFF")
    total_fill = PatternFill("solid", fgColor="D9E1F2")
    total_font = Font(bold=True)

    sheet.append(headers)
    header_row_idx = sheet.max_row
    for col_idx in range(1, len(headers) + 1):
        cell = sheet.cell(row=header_row_idx, column=col_idx)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(horizontal="center")

    data_rows = rows[:-1]
    total_row = rows[-1]

    for row in data_rows:
        sheet.append(row)

    sheet.append([])
    sheet.append(total_row)
    total_row_idx = sheet.max_row
    for col_idx in range(1, len(total_row) + 1):
        cell = sheet.cell(row=total_row_idx, column=col_idx)
        cell.fill = total_fill
        cell.font = total_font

    for row_idx in range(header_row_idx + 1, total_row_idx + 1):
        if row_idx == total_row_idx - 1:
            continue
        for col_idx in range(2, len(headers) + 1):
            cell = sheet.cell(row=row_idx, column=col_idx)
            cell.alignment = Alignment(horizontal="right")
            cell.number_format = "0.0%" if show_percent else "0"

    for col_idx, col_cells in enumerate(sheet.columns, start=1):
        max_len = max((len(str(cell.value or "")) for cell in col_cells), default=10)
        sheet.column_dimensions[get_column_letter(col_idx)].width = min(max_len + 2, 60)

    workbook.save(path)


def generate_report(
    repo_root: Path,
    branch: str,
    since: str,
    until: str,
    author: str,
    show_net: bool,
    comments_only: bool,
    show_percent: bool,
    excel_name: str,
) -> tuple[str, Path]:
    if show_percent and show_net:
        raise ValueError("Percentage mode only works with added-only or comments-only output.")

    since_date = _parse_display_date(since)
    until_date = _parse_display_date(until)
    if since_date > until_date:
        raise ValueError("Start date must be on or before end date.")
    since_iso = since_date.isoformat()
    until_iso = until_date.isoformat()
    since_display = _format_display_date(since_date)
    until_display = _format_display_date(until_date)

    if comments_only:
        raw = _run_git_patch(branch, since_iso, until_iso, repo_root)
        stats = _parse_patch_comments(raw)
    else:
        raw = _run_git_numstat(branch, since_iso, until_iso, repo_root)
        stats = _parse_numstat(raw)

    stats = {
        key: value
        for key, value in stats.items()
        if not key[1].lower().endswith(".json") and not _should_exclude_file(key[1])
    }
    if author and author != ALL_AUTHORS_LABEL:
        stats = {key: value for key, value in stats.items() if key[0] == author}
    if not stats:
        raise ValueError("No file-level changes found for that range.")

    headers, rows, excel_rows = _build_pivot(stats, show_net=show_net, show_percent=show_percent)
    if not headers or not rows:
        raise ValueError("No non-zero file activity found for that range.")

    mode_label = _mode_label(show_net, comments_only, show_percent)
    output_path = _excel_output_path(excel_name)
    _export_excel(headers, excel_rows, output_path, branch, since_display, until_display, mode_label, show_percent)

    scope_label = author if author and author != ALL_AUTHORS_LABEL else "all authors"

    text = (
        f"Querying: git log {branch}  since={since_display}  until={until_display}  {mode_label}  author={scope_label}\n\n"
        f"{_render_table(headers, rows)}\n\n"
        f"Exported to: {output_path}"
    )
    return text, output_path


class CalendarDialog(tk.Toplevel):
    def __init__(self, parent: tk.Misc, initial_date: date, callback) -> None:
        super().__init__(parent)
        self.title("Select Date")
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()
        self.callback = callback
        self.current_year = initial_date.year
        self.current_month = initial_date.month
        self.selected_date = initial_date

        self.header_var = tk.StringVar()
        self.body = ttk.Frame(self, padding=12)
        self.body.grid(sticky="nsew")

        nav = ttk.Frame(self.body)
        nav.grid(row=0, column=0, columnspan=7, sticky="ew", pady=(0, 8))
        ttk.Button(nav, text="<", width=3, command=self._prev_month).grid(row=0, column=0)
        ttk.Label(nav, textvariable=self.header_var, width=18, anchor="center").grid(row=0, column=1, padx=8)
        ttk.Button(nav, text=">", width=3, command=self._next_month).grid(row=0, column=2)

        for idx, weekday in enumerate(("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")):
            ttk.Label(self.body, text=weekday, anchor="center", width=5).grid(row=1, column=idx, pady=(0, 4))

        self.day_buttons: list[ttk.Button] = []
        for row in range(6):
            for col in range(7):
                button = ttk.Button(self.body, width=4)
                button.grid(row=row + 2, column=col, padx=1, pady=1)
                self.day_buttons.append(button)

        self.protocol("WM_DELETE_WINDOW", self.destroy)
        self._render()
        self.wait_visibility()
        self.focus_set()

    def _prev_month(self) -> None:
        if self.current_month == 1:
            self.current_month = 12
            self.current_year -= 1
        else:
            self.current_month -= 1
        self._render()

    def _next_month(self) -> None:
        if self.current_month == 12:
            self.current_month = 1
            self.current_year += 1
        else:
            self.current_month += 1
        self._render()

    def _select_day(self, day: int) -> None:
        selected = date(self.current_year, self.current_month, day)
        self.callback(_format_display_date(selected))
        self.destroy()

    def _render(self) -> None:
        self.header_var.set(f"{calendar_module.month_name[self.current_month]} {self.current_year}")
        month_days = calendar_module.Calendar(firstweekday=0).monthdayscalendar(self.current_year, self.current_month)
        while len(month_days) < 6:
            month_days.append([0] * 7)

        for button, day in zip(self.day_buttons, [d for week in month_days for d in week]):
            if day == 0:
                button.configure(text="", state="disabled", command=lambda: None)
            else:
                button.configure(text=str(day), state="normal", command=lambda d=day: self._select_day(d))


class RepoActivityGUI(tk.Tk):
    def __init__(self, repo_root: Path) -> None:
        super().__init__()
        self.repo_root = repo_root
        self.results_queue: Queue[tuple[str, object]] = Queue()
        self.title("Repo Activity")
        self.geometry("1080x760")
        self.minsize(900, 640)

        branches = _list_branches(repo_root)
        authors = _list_authors(repo_root)
        today = _format_display_date(date.today())
        month_start = _format_display_date(date.today().replace(day=1))

        self.branch_var = tk.StringVar(value=_current_branch(repo_root))
        self.author_var = tk.StringVar(value=ALL_AUTHORS_LABEL)
        self.since_var = tk.StringVar(value=month_start)
        self.until_var = tk.StringVar(value=today)
        self.net_var = tk.BooleanVar(value=True)
        self.comments_var = tk.BooleanVar(value=False)
        self.percent_var = tk.BooleanVar(value=False)
        self.excel_var = tk.StringVar(value=DEFAULT_EXCEL_FILENAME)
        self.status_var = tk.StringVar(value=f"Output folder: {_app_dir()}")

        container = ttk.Frame(self, padding=16)
        container.grid(sticky="nsew")
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        container.columnconfigure(1, weight=1)
        container.rowconfigure(7, weight=1)

        ttk.Label(container, text="Repo Activity", font=("Segoe UI", 16, "bold")).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 12)
        )

        ttk.Label(container, text="Branch").grid(row=1, column=0, sticky="w", pady=4)
        self.branch_box = ttk.Combobox(container, textvariable=self.branch_var, values=branches, state="readonly")
        self.branch_box.grid(row=1, column=1, columnspan=2, sticky="ew", pady=4)

        ttk.Label(container, text="Author").grid(row=2, column=0, sticky="w", pady=4)
        self.author_box = ttk.Combobox(container, textvariable=self.author_var, values=authors, state="readonly")
        self.author_box.grid(row=2, column=1, columnspan=2, sticky="ew", pady=4)

        self._build_date_row(container, row=3, label="Since", variable=self.since_var)
        self._build_date_row(container, row=4, label="Until", variable=self.until_var)

        options = ttk.LabelFrame(container, text="Options", padding=12)
        options.grid(row=5, column=0, columnspan=3, sticky="ew", pady=(12, 8))
        ttk.Checkbutton(options, text="Net lines (added - deleted)", variable=self.net_var).grid(row=0, column=0, sticky="w")
        ttk.Checkbutton(options, text="Comments only", variable=self.comments_var).grid(row=0, column=1, sticky="w", padx=(18, 0))
        ttk.Checkbutton(options, text="Percentage share", variable=self.percent_var).grid(row=0, column=2, sticky="w", padx=(18, 0))

        ttk.Label(container, text="Excel filename").grid(row=6, column=0, sticky="w", pady=(8, 4))
        excel_entry = ttk.Entry(container, textvariable=self.excel_var)
        excel_entry.grid(row=6, column=1, sticky="ew", pady=(8, 4))
        excel_entry.bind("<KeyRelease>", lambda _event: self._refresh_status())
        ttk.Button(container, text="Generate Excel", command=self._start_run).grid(row=6, column=2, sticky="e", pady=(8, 4))

        output_frame = ttk.LabelFrame(container, text="Output", padding=8)
        output_frame.grid(row=7, column=0, columnspan=3, sticky="nsew", pady=(12, 8))
        output_frame.columnconfigure(0, weight=1)
        output_frame.rowconfigure(0, weight=1)

        self.output_text = tk.Text(output_frame, wrap="none", font=("Consolas", 10))
        self.output_text.grid(row=0, column=0, sticky="nsew")
        yscroll = ttk.Scrollbar(output_frame, orient="vertical", command=self.output_text.yview)
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll = ttk.Scrollbar(output_frame, orient="horizontal", command=self.output_text.xview)
        xscroll.grid(row=1, column=0, sticky="ew")
        self.output_text.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)

        ttk.Label(container, textvariable=self.status_var).grid(row=8, column=0, columnspan=3, sticky="w")

        self._refresh_status()
        self.after(150, self._poll_results)

    def _build_date_row(self, parent: ttk.Frame, row: int, label: str, variable: tk.StringVar) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=4)
        entry = ttk.Entry(parent, textvariable=variable, state="readonly")
        entry.grid(row=row, column=1, sticky="ew", pady=4)
        ttk.Button(parent, text="Select Date", command=lambda v=variable: self._open_calendar(v)).grid(
            row=row, column=2, sticky="e", pady=4
        )

    def _open_calendar(self, variable: tk.StringVar) -> None:
        try:
            initial = _parse_display_date(variable.get())
        except ValueError:
            initial = date.today()
        CalendarDialog(self, initial, variable.set)

    def _refresh_status(self) -> None:
        target = _excel_output_path(self.excel_var.get())
        self.status_var.set(f"Excel will be saved to: {target}")

    def _set_running(self, running: bool) -> None:
        self.branch_box.configure(state="disabled" if running else "readonly")
        self.author_box.configure(state="disabled" if running else "readonly")

    def _start_run(self) -> None:
        self._refresh_status()
        self._set_running(True)
        self.status_var.set("Generating report...")

        config = {
            "repo_root": self.repo_root,
            "branch": self.branch_var.get(),
            "author": self.author_var.get(),
            "since": self.since_var.get(),
            "until": self.until_var.get(),
            "show_net": self.net_var.get(),
            "comments_only": self.comments_var.get(),
            "show_percent": self.percent_var.get(),
            "excel_name": self.excel_var.get(),
        }
        worker = threading.Thread(target=self._run_worker, args=(config,), daemon=True)
        worker.start()

    def _run_worker(self, config: dict[str, object]) -> None:
        try:
            text, output_path = generate_report(**config)
            self.results_queue.put(("success", (text, output_path)))
        except Exception as exc:  # pragma: no cover - GUI error path
            details = "".join(traceback.format_exception_only(type(exc), exc)).strip()
            self.results_queue.put(("error", details))

    def _poll_results(self) -> None:
        try:
            kind, payload = self.results_queue.get_nowait()
        except Empty:
            self.after(150, self._poll_results)
            return

        self._set_running(False)
        if kind == "success":
            text, output_path = payload
            self.output_text.delete("1.0", "end")
            self.output_text.insert("1.0", str(text))
            self.status_var.set(f"Exported to: {output_path}")
            messagebox.showinfo("Repo Activity", f"Excel created:\n{output_path}")
        else:
            self.status_var.set("Report generation failed.")
            messagebox.showerror("Repo Activity", str(payload))

        self.after(150, self._poll_results)


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="repo-activity")
    parser.add_argument("--branch", "-b")
    parser.add_argument("--author", "-a", default=ALL_AUTHORS_LABEL)
    parser.add_argument("--since", "-s")
    parser.add_argument("--until", "-u")
    parser.add_argument("--net", dest="net", action="store_true")
    parser.add_argument("--no-net", dest="net", action="store_false")
    parser.add_argument("--comments", dest="comments", action="store_true")
    parser.add_argument("--no-comments", dest="comments", action="store_false")
    parser.add_argument("--percent", dest="percent", action="store_true")
    parser.add_argument("--no-percent", dest="percent", action="store_false")
    parser.add_argument("--excel", "-e", default=DEFAULT_EXCEL_FILENAME)
    parser.set_defaults(net=True, comments=False, percent=False)
    return parser


def _run_cli(args: argparse.Namespace, repo_root: Path) -> int:
    branch = args.branch or _current_branch(repo_root)
    since = args.since or _format_display_date(date.today().replace(day=1))
    until = args.until or _format_display_date(date.today())

    try:
        text, _output_path = generate_report(
            repo_root=repo_root,
            branch=branch,
            author=args.author,
            since=since,
            until=until,
            show_net=args.net,
            comments_only=args.comments,
            show_percent=args.percent,
            excel_name=args.excel,
        )
    except Exception as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 1

    print(text)
    return 0


def main() -> int:
    repo_root = _repo_root()
    parser = _build_arg_parser()

    if len(sys.argv) > 1:
        return _run_cli(parser.parse_args(), repo_root)

    app = RepoActivityGUI(repo_root)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
