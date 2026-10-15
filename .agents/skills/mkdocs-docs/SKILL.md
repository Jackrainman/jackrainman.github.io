---
name: mkdocs-docs
description: 在本仓库新增、移动、修订 MkDocs 文档页面时使用。覆盖 i18n 的真实路径映射、nav 注册、图片与链接规范、AI 残留清理、发布前校验。
---

# MkDocs 文档站工作流

站点：`Web_of_Jackrainman` → `https://jackrainman.github.io/`，
MkDocs 1.6.1 + Material 9.7.7 + `mkdocs-static-i18n`（唯一语言 `zh`），GitHub Pages 部署。
**公开仓库，提交即永久。**

## i18n 的真实路径映射（实测，别按直觉推断）

`docs_structure: folder` 且 `zh` 是 `default: true`，结果是：

| 源文件 | 站点 URL |
|---|---|
| `docs/zh/<主题>/<页面>.md` | `/<主题>/<页面>/` |
| `docs/zh/index.md` | `/` |
| `docs/index.md` | 被 `docs/zh/index.md` 覆盖，**不产出任何页面** |

- 站点里**不存在 `/zh/` 前缀**。写 `/zh/xxx` 的绝对链接必然是死链。
- `docs/index.md` 与 `docs/zh/index.md` 当前内容相同且都映射到 `/`，后者胜出。
  改首页只改 `docs/zh/index.md`；`docs/index.md` 是模板带来的冗余副本，删掉不影响产出。
- `nav:` 只写 `Home: index.md`。再加一条 `中文首页: zh/index.md` 不会多出一个页面，
  只会让导航重复。

## 本仓库同时是 Obsidian 仓库

`docs/zh/` 就是你的 vault。风险形状：**"写进文件" 与 "公开" 只隔一次 `git push`**，
而 `exclude_docs` 只挡构建、不挡仓库源码。草稿要同时过两道闸（详见 `repo-hygiene` 技能）：

| 目的 | 机制 | 挡住什么 |
|---|---|---|
| 不上线 | `exclude_docs` + 不写进 `nav` | 渲染出的站点 |
| 不公开 | 根 `.gitignore` **且** `docs/zh/.gitignore` | 仓库源码与 git 历史 |

默认新笔记进 `docs/zh/_Drafts/`（两处拦截都已配好），定稿后再 `git mv` 到主题目录。
`docs/zh/.gitignore` 的规则**相对 `docs/zh/`** 解释：写 `Chassis/底盘解算.md`，
不写 `docs/zh/Chassis/底盘解算.md`。

## 新增或移动一个页面

1. 文件放 `docs/zh/<主题>/`，图片放同级 `Pictures/<页面名>/`。
   `docs/img/` 是主题资产（logo / favicon），不放正文配图。
2. 在 `mkdocs.yml` 的 `nav:` 注册，否则页面游离于导航之外。
3. 私人/草稿/队内内容不提交，同时补三处：`exclude_docs` + 根 `.gitignore` + `docs/zh/.gitignore`。
4. 重命名页面时同步三处：`nav:`、全站反向链接、`exclude_docs` 里的旧路径。

## 链接与图片

- 用相对路径（相对当前 md 文件），不写绝对 `/...`。
- 文件名含空格时 `%20` 或直接保留空格都可以，MkDocs 两者都吃。
- 图片必须真实存在：`mkdocs build --strict` **不检查**图片缺失，只有 `check_docs.py` 查。
- 原生 HTML 的 `href` / `src` 同样绕过 MkDocs 的链接解析，`--strict` 也不查。
  首页 CTA 就是这样带着 404 目标通过构建并上线的。
- 图片缩放走 `extra.css` 的既有类：`img-scale-50` / `img-w-640` 等，不手写内联宽度。

## 代码块与公式

- 开栏必须带语言：` ```c `、` ```python `、` ```bash `、` ```mermaid `；闭栏不带。
- 公式走 MathJax：行内 `$...$`，独立 `$$...$$`。

## AI 残留（本站最大的发布事故源）

进入 `docs/` 的内容必须清除以下痕迹，它们会原样出现在公开页面上：

- `图片占位符`、`待补充`、`待生成`、`TODO`、`FIXME`
- 生成过程说明（"以下是本文即将介绍的内容"、分步配图脚本清单）
- `*_Prompt.md`、`chatgpt.md` 这类对话文件

配图未就绪时**删掉整块占位符**，不要留骨架。宁可少一节，不要少图。
旧站曾把占位符发到线上且 `--strict` 全程无警告；本仓库的历史里没有那次事故，
但机制上的漏洞还在，靠 `check_docs.py` 挡。

## 发布前校验

```bash
python .agents/scripts/check_docs.py      # 断链/残留/nav 一致/site_url/giscus 一致
mkdocs build --strict                     # 官方检查
```

两者都要跑，非零不得提交。`--strict` 只查 MkDocs 自己认识的引用；
`check_docs.py` 补的是图片存在性、HTML 死链、AI 残留、站点地址与仓库是否匹配。

## 首页是模板带来的，数字会过期

`docs/zh/index.md` 的 `landing-stats`（"40+ 专题文档"等）和 `landing-chips`
沿用旧站，正文尚未开始，与站点实际内容不符；`landing-actions` 是空槽。
首批内容上线时：要么改数字，要么补页面，并在有真实 CTA 目标后再填 `landing-actions`
（填了就必须是活链接，否则 `check_html_refs` 会拦）。

## 不要做的事

- AI 脚本、提示词、笔记一律进 `.agents/`，不放仓库根、不放 `docs/`。
- 不 `git push --force` 或改写历史来"删除敏感内容"。
- 不提交 `.venv/`、`site/`、`node_modules/`、`__pycache__/`。
- 不为了让某页出现在 `/zh/` 而加语言：只有 `zh` 一种语言，路径前缀不存在。
