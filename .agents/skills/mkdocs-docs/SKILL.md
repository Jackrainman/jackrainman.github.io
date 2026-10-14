---
name: mkdocs-docs
description: 在本仓库新增、移动、修订 MkDocs 文档页面时使用。覆盖 nav 注册、图片与链接规范、AI 残留清理、发布前校验。
---

# MkDocs 文档站工作流

站点：`Web_of_Jackrainman`，MkDocs Material + `mkdocs-static-i18n`（唯一语言 `zh`），
部署到 GitHub Pages（**公开仓库，历史不可撤回**）。

## 本仓库同时是 Obsidian 仓库

`docs/zh/` 就是你的 vault。风险在于：**在耦合布局里，"写进文件" 和 "公开" 只隔一次
`git push`**，而 `exclude_docs` 只挡构建、不挡仓库源码。所以草稿要同时过两道闸：

| 目的 | 机制 | 挡住什么 |
|---|---|---|
| 不上线 | `mkdocs.yml` 的 `exclude_docs` + 不写进 `nav` | 渲染出的站点 |
| 不公开 | 根 `.gitignore` + `docs/zh/.gitignore`（或 `git rm --cached`） | 仓库源码与历史 |

只做到第一行，文件仍能以 `/blob/main/...` 直接访问 —— 这是最常见的漏法。
默认新笔记进 `docs/zh/_Drafts/`（两处拦截都已配好），定稿后再 `git mv` 到主题目录。

## 新增或移动一个页面

1. 文件放在 `docs/zh/<主题>/` 下，图片放同级 `Pictures/<页面名>/`。
2. 在 `mkdocs.yml` 的 `nav:` 注册条目，否则页面游离于导航之外。
3. 若属私人/草稿/队内内容，**不要提交**，同时补进两处：
   - `mkdocs.yml` 的 `exclude_docs:`（不参与构建）
   - `docs/zh/.gitignore` 与根 `.gitignore`（不进版本库）
4. 重命名页面时同步三处：`nav:`、全站反向链接、`exclude_docs:` 里的旧路径。

## 链接与图片

- 用相对路径，不要用绝对 `/zh/...`；文件名含空格时写成 `%20` 或直接保留空格均可，
  MkDocs 两者都吃。
- 锚点链接写 `#标题` 即可，不指向其他文件。
- 图片必须真实存在。`mkdocs build --strict` **不会**检查图片缺失，只有
  `check_docs.py` 会。
- 引用形式：`![说明](Pictures/<页面名>/<文件>)`。

## 代码块

- 开栏必须带语言：` ```c `、` ```python `、` ```mermaid `。闭栏不带。
- 嵌入 C 代码用 `c`；公式走 MathJax：行内 `$...$`，独立 `$$...$$`。

## AI 残留（本站最大的发布事故源）

AI 生成的内容进入 `docs/` 前必须清除以下痕迹，它们会原样出现在公开页面上：

- `图片占位符`、`待补充`、`待生成`、`TODO`、`FIXME`
- 生成过程说明（"以下是本文即将介绍的内容"、分步配图脚本清单）
- `*_Prompt.md`、`chatgpt.md` 这类对话文件

配图未就绪时：**删掉整块占位符**，不要留骨架。宁可少一节，不要少图。

## 发布前校验

```bash
python .agents/scripts/check_docs.py          # 断链 / AI 残留 / nav 一致性
mkdocs build --strict                          # 官方检查
```

`check_docs.py` 返回非零即不得提交。历史教训：占位符已泄漏到线上
`zh/Chassis/底盘解算/`，strict 构建全程无警告。

## 不要做的事

- 不要提交 AI 技能脚本、提示词、临时调试文件到仓库根目录 —— 一律进 `.agents/`。
- 不要 `git push --force` 或改写历史来"删除敏感内容"；公开仓库的历史删不干净，
  只能靠不再提交。
- 不要把 `.venv/`、`site/`、`node_modules/`、`package-lock.json`（无 `package.json` 时）提交进来。
