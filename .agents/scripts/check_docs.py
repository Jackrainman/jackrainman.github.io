#!/usr/bin/env python3
"""文档站静态体检。mkdocs --strict 不检查图片存在性、HTML 死链、AI 残留、
站点地址与仓库是否匹配、评论绑定是否指向本仓库，本脚本补齐。

用法: python .agents/scripts/check_docs.py [--relaxed]
--relaxed 只报告，不返回非零退出码。
"""
import argparse
import fnmatch
import re
import subprocess
import sys
import urllib.parse
from pathlib import Path

FENCE = re.compile(r"^\s*>?\s*```")
LINK = re.compile(r"!?\[[^\]]*\]\(\s*<?([^)>\s]+?)>?(?:#[^)]*)?\)")
EXTERNAL = ("#", "http:", "https:", "mailto:", "data:", "javascript:", "tel:")

# 已发布的页面里不应出现的生成过程残留
RESIDUE = {
    "占位符": r"占位符|待补充|待生成|尚未补充",
    "TODO 标记": r"TODO|FIXME",
    # 写作提纲/施工单：Obsidian 草稿误入已发布路径的典型特征
    "配图需求清单": r"配图需求|待补图|总计[:：]\s*约?\d+\s*张配图",
}


def md_files(docs: Path):
    return sorted(docs.rglob("*.md"))


def nav_paths(mkdocs_yml: Path):
    """收集 nav: 块中引用的 md 路径（相对 docs_dir）。

    路径可能含空格（如 `FLASH 模拟 EEPROM.md`），因此按“冒号右侧整行”取值，
    而不是按空白切分。
    """
    text = mkdocs_yml.read_text(encoding="utf-8")
    start = text.find("\nnav:")
    if start == -1:
        return set()
    found = set()
    for line in text[start + len("\nnav:"):].splitlines():
        s = line.strip()
        if not s.endswith(".md") or ":" not in s:
            continue
        found.add(s.split(":", 1)[1].strip().strip("'\"").lstrip("./"))
    return found


def excluded_patterns(mkdocs_yml: Path):
    block = re.search(r"^exclude_docs: \|\n((?:  [^\n]+\n)+)",
                      mkdocs_yml.read_text(encoding="utf-8"), re.M)
    if not block:
        return []
    return [ln.strip() for ln in block.group(1).splitlines() if ln.strip()]


def is_excluded(rel: str, patterns):
    """exclude_docs 的 glob 以 docs/ 为根。fnmatch 的 * 可跨越 /，
    因此 `zh/**/Legacy/**` 这类模式能直接命中。"""
    return any(fnmatch.fnmatch(rel, p) for p in patterns)


def strip_fences(lines):
    """返回 (行号, 内容) 列表，跳过围栏代码块内部。块内链接不是真链接。"""
    out, inside = [], False
    for i, line in enumerate(lines, 1):
        if FENCE.match(line):
            inside = not inside
            continue
        if not inside:
            out.append((i, line))
    return out


def check_links(docs: Path, pats):
    """断链检查。exclude_docs 中的草稿不检查（图未画属正常）。"""
    errors = []
    for p in md_files(docs):
        rel = str(p.relative_to(docs)).replace("\\", "/")
        if is_excluded(rel, pats):
            continue
        for lineno, line in strip_fences(
                p.read_text(encoding="utf-8", errors="replace").splitlines()):
            for m in LINK.finditer(line):
                url = urllib.parse.unquote(m.group(1))
                if url.startswith(EXTERNAL):
                    continue
                target = (p.parent / url).resolve()
                if target.is_dir():
                    target /= "index.md"
                if not target.exists():
                    errors.append(f"{rel}:{lineno} -> {m.group(1)}")
    return errors


def check_residue(docs: Path, pats):
    """扫描生成过程残留。exclude_docs 中的文件属草稿区，不报错。"""
    errors = []
    for p in md_files(docs):
        rel = str(p.relative_to(docs)).replace("\\", "/")
        if is_excluded(rel, pats):
            continue
        for lineno, line in strip_fences(
                p.read_text(encoding="utf-8", errors="replace").splitlines()):
            for label, pat in RESIDUE.items():
                if re.search(pat, line):
                    errors.append(f"{rel}:{lineno} [{label}] {line.strip()[:60]}")
    return errors


HTML_REF = re.compile(r'(?:href|src)="([^"]+)"')


def check_html_refs(docs: Path, pats):
    """原生 HTML 的 href/src 不经 Markdown 链接解析，mkdocs --strict 不校验。

    首页 CTA 按钮就是这样带着 404 目标通过构建的。
    """
    errors = []
    for p in md_files(docs):
        rel = str(p.relative_to(docs)).replace("\\", "/")
        if is_excluded(rel, pats):
            continue
        for lineno, line in strip_fences(
                p.read_text(encoding="utf-8", errors="replace").splitlines()):
            for m in HTML_REF.finditer(line):
                url = urllib.parse.unquote(m.group(1))
                if url.startswith(("#", "//", "http:", "https:", "mailto:",
                                   "data:", "javascript:", "tel:")):
                    continue
                stem = url.rstrip("/")
                cands = [docs / stem, docs / stem / "index.md",
                         p.parent / url, p.parent / stem / "index.md"]
                if not any(c.exists() for c in cands):
                    errors.append(f"{rel}:{lineno} [HTML 引用] {m.group(1)}")
    return errors


def check_nav(docs: Path, mkdocs_yml: Path):
    refs = nav_paths(mkdocs_yml)
    pats = excluded_patterns(mkdocs_yml)
    # 站点根 index.md 与 zh 语言索引页由 i18n 生成，不进 nav 属正常
    builtin = {"index.md", "zh/index.md"}
    orphans, missing = [], []
    for p in md_files(docs):
        rel = str(p.relative_to(docs)).replace("\\", "/")
        if rel in builtin or is_excluded(rel, pats):
            continue
        if rel not in refs:
            orphans.append(rel)
    for r in sorted(refs):
        if is_excluded(r, pats):
            continue
        if not (docs / r).exists():
            missing.append(r)
    return orphans, missing


def check_fence_languages(docs: Path):
    """统计没有语言标注的开栏。``` 成对出现，开栏无标注即高亮缺失。"""
    total = 0
    for p in md_files(docs):
        inside = False
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.lstrip().lstrip(">").lstrip().startswith("```"):
                continue
            tag = line.lstrip().lstrip(">").lstrip()[3:].strip()
            if inside:
                inside = False
            else:
                inside = True
                if not tag:
                    total += 1
    return total


def remote_slug(root: Path):
    """origin 指向的 (owner, repo)。git 不可用或没有 origin 时返回 None。"""
    try:
        out = subprocess.run(["git", "-C", str(root), "remote", "get-url", "origin"],
                             capture_output=True, text=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return None
    if out.returncode:
        return None
    url = out.stdout.strip()
    tail = url.split("://", 1)[-1] if "://" in url else url.replace(":", "/")
    if tail.endswith(".git"):
        tail = tail[:-4]
    parts = [p for p in tail.split("/") if p]
    return (parts[-2], parts[-1]) if len(parts) >= 2 else None


def check_site_address(root: Path, mkdocs_yml: Path):
    """site_url 必须等于 Pages 的实际根路径。

    留空时 sitemap 每条 <loc> 变成 None 前缀（旧站带病跑了九个月，--strict 全程无警告）；
    仓库改名后残留旧名，则整站索引指向 404。
    """
    problems = []
    m = re.search(r"^site_url:\s*(\S*)\s*$",
                  mkdocs_yml.read_text(encoding="utf-8"), re.M)
    url = m.group(1).strip("'\"") if m else ""
    if not url:
        return ["site_url 为空 —— sitemap 的 <loc> 会全部变成 None 前缀"]
    slug = remote_slug(root)
    if not slug:
        return problems
    owner, repo = slug
    base = f"https://{owner.lower()}.github.io"
    expected = f"{base}/" if repo.lower() == f"{owner.lower()}.github.io" else f"{base}/{repo}/"
    if url.rstrip("/") + "/" != expected:
        problems.append(f"site_url={url}，仓库实际地址是 {expected}")
    return problems


def check_giscus_drift(root: Path, mkdocs_yml: Path):
    """评论区的生效源是 overrides/partials/comments.html，mkdocs.yml 的 extra 只是备忘。

    两处不一致会误导改配置的人，属阻塞；两处一致但仍是占位值，属已知待办，不阻塞。
    """
    tpl = root / "overrides/partials/comments.html"
    if not tpl.exists():
        return [], []
    problems, notes = [], []
    html = tpl.read_text(encoding="utf-8", errors="replace")
    got = dict(re.findall(r'data-(repo|repo-id|category|category-id)="([^"]*)"', html))
    slug = remote_slug(root)
    if slug and got.get("repo"):
        want = f"{slug[0]}/{slug[1]}"
        if got["repo"].lower() != want.lower():
            problems.append(
                f"comments.html 绑定仓库 {got['repo']}，本仓库是 {want}")
    cfg_block = re.search(r"^  comments:\n((?: {4,}[^\n]+\n)+)",
                          mkdocs_yml.read_text(encoding="utf-8"), re.M)
    yml = {k: v.split("#", 1)[0].strip().strip("'\"")
           for k, v in re.findall(
               r"^ +(repo|repo_id|category|category_id): *(.+?)\s*$",
               cfg_block.group(1), re.M)} if cfg_block else {}
    pairs = {"repo": "repo", "repo-id": "repo_id", "category": "category",
             "category-id": "category_id"}
    for html_key, yml_key in pairs.items():
        a, b = got.get(html_key, ""), yml.get(yml_key, "")
        if not (a and b):
            continue
        same = a.lower() == b.lower() if yml_key == "repo" else a == b
        if not same:
            problems.append(f"giscus {yml_key} 两处不一致：comments.html={a} mkdocs.yml={b}")
    for val in sorted({v for v in (*yml.values(), *got.values()) if "..." in v}):
        notes.append(f"giscus 占位值 {val} —— 评论区不可用")
    return problems, notes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--relaxed", action="store_true", help="只报告，不返回非零退出码")
    args = ap.parse_args()
    root = Path(__file__).resolve().parents[2]
    docs, cfg = root / "docs", root / "mkdocs.yml"

    pats = excluded_patterns(cfg)
    broken = check_links(docs, pats) + check_html_refs(docs, pats)
    residue = check_residue(docs, pats)
    orphans, missing = check_nav(docs, cfg)
    address = check_site_address(root, cfg)
    giscus, notes = check_giscus_drift(root, cfg)
    no_lang = check_fence_languages(docs)

    print(f"文档体检 {root.name}  (md: {len(list(md_files(docs)))}, "
          f"草稿区规则: {len(pats)})")
    for title, items in (
        ("断链（图片或页面不存在）", broken),
        ("AI 生成残留进入已发布页面", residue),
        ("nav 缺失（文件存在但未列入导航）", orphans),
        ("nav 引用不存在的文件", missing),
        ("站点地址与仓库不符", address),
        ("评论区配置两处不一致", giscus),
    ):
        print(f"\n[{title}] {len(items)}")
        for it in items[:15]:
            print(f"  {it}")
        if len(items) > 15:
            print(f"  ... 另有 {len(items) - 15} 条")

    print(f"\n[无语言标注的代码块] {no_lang}（影响高亮，非阻塞）")
    for note in notes:
        print(f"[待办] {note}（非阻塞）")

    hard = len(broken) + len(residue) + len(missing) + len(address) + len(giscus)
    if not hard:
        print("\nOK")
        return 0
    if args.relaxed:
        print(f"\nWARN: {hard} 项待修复（--relaxed）")
        return 0
    print(f"\nFAIL: {hard} 项需修复")
    return 1


if __name__ == "__main__":
    sys.exit(main())
