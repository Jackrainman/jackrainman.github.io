#!/usr/bin/env python3
"""给没有语言标注的代码块开栏补上语言。

只对能被特征判定的块动手；判不出来的报告位置，由人决定。
默认写回文件，--dry-run 只看结果。

用法: python .agents/scripts/fix_code_blocks.py [--dry-run]
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_docs import excluded_patterns, is_excluded, md_files  # noqa: E402

PATTERNS = {
    "c": [r"#include\s*<", r"\bint\s+main\s*\(", r"\buint\d+_t\b",
          r"\bprintf\s*\("],
    "python": [r"^\s*def\s+\w+\(", r"^\s*class\s+\w+", r"^\s*import\s+\w+",
               r"^\s*from\s+\w+\s+import\s"],
    "bash": [r"^\s*(\$|>)\s*(pip|git|mkdocs|npm|curl|cd|python)",
             r"^\s*(pip|git|mkdocs|npm|curl)\s"],
    "yaml": [r"^\s*[\w.-]+:\s*(\||[^\s#].*)$", r"^\s*-\s+\w+:\s"],
    "json": [r'^\s*\{?\s*"[^"]+"\s*:'],
}

FENCE = re.compile(r"^(\s*>?\s*)(`{3,})(\w*)\s*$")


def detect(code: str):
    """命中特征最多的语言；一个都不命中时返回 None，不猜。"""
    sample = "\n".join(code.splitlines()[:12])
    scores = {lang: sum(1 for p in pats if re.search(p, sample, re.M))
              for lang, pats in PATTERNS.items()}
    best = max(scores, key=scores.get)
    return best if scores[best] else None


def blocks(text: str):
    """按成对围栏 yield (开栏行下标, 缩进, 反引号串, 语言, 代码)。"""
    lines = text.splitlines(keepends=True)
    open_at = None
    for idx, line in enumerate(lines):
        m = FENCE.match(line.rstrip("\n"))
        if not m:
            continue
        indent, ticks, lang = m.group(1), m.group(2), m.group(3)
        if open_at is None:
            open_at = {"idx": idx, "indent": indent, "ticks": ticks, "lang": lang}
        elif ticks == open_at["ticks"]:
            yield open_at["idx"], open_at["indent"], ticks, open_at["lang"], \
                "".join(lines[open_at["idx"] + 1:idx])
            open_at = None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="只报告，不写回")
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[2]
    docs = root / "docs"
    pats = excluded_patterns(root / "mkdocs.yml")

    total = 0
    fixed_files, undecided = [], []
    for path in md_files(docs):
        rel = str(path.relative_to(docs)).replace("\\", "/")
        if is_excluded(rel, pats):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines(keepends=True)
        targets = []
        for idx, indent, ticks, lang, code in blocks(text):
            if lang or not code.strip():
                continue
            guess = detect(code)
            if guess is None:
                undecided.append(f"{rel}:{idx + 1}")
            else:
                targets.append((idx, f"{indent}{ticks}{guess}\n"))
        if not targets:
            continue
        for idx, new_line in targets:
            lines[idx] = new_line
        if not args.dry_run:
            path.write_text("".join(lines), encoding="utf-8")
        fixed_files.append(f"{rel} ({len(targets)} 处)")
        total += len(targets)

    print(f"补标注 {total} 处 / {len(fixed_files)} 个文件"
          f"{'（--dry-run，未写回）' if args.dry_run and total else ''}")
    for f in fixed_files:
        print(f"  {f}")
    if undecided:
        print(f"\n判不出语言，需人工确认 {len(undecided)} 处：")
        for u in undecided[:20]:
            print(f"  {u}")
        if len(undecided) > 20:
            print(f"  ... 另有 {len(undecided) - 20} 处")
    return 0


if __name__ == "__main__":
    sys.exit(main())
