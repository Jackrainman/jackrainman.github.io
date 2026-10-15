---
name: repo-hygiene
description: 提交、迁移仓库、处理草稿与队内内容、排查历史泄露时使用。覆盖 Obsidian 与站点同库的两道闸、公开历史不可撤回的处置顺序、新仓库种子拷贝规则、AI 产物归置。
---

# 仓库卫生与发布边界

本仓库**同时是 Obsidian vault**（`docs/zh/` 就是笔记本）。这决定了所有风险的形状：
默认行为是"写了就可能被推上去"，属于失效开放（fall-open）布局。

## 两道闸，缺一不可

| 目的 | 机制 | 挡住什么 |
|---|---|---|
| 不上线 | `mkdocs.yml` 的 `exclude_docs` + 不写进 `nav` | 渲染出的站点 |
| 不公开 | 根 `.gitignore` **且** `docs/zh/.gitignore` | 仓库源码与 git 历史 |

只做第一行，文件照样能以 `https://github.com/<owner>/<repo>/blob/main/<path>` 直接读到，
即使它从没出现在站点上。**这是本站历史上队内内容外泄的唯一原因。**

嵌套 `.gitignore` 的规则**相对该文件所在目录**解释：写在 `docs/zh/.gitignore` 里的
应该是 `Chassis/底盘解算.md`，而不是 `docs/zh/Chassis/底盘解算.md`。写错会静默不生效。

`.gitignore` 里指向不存在目录的规则（`培养/`、`运营/` 等）是给本地 Obsidian 草稿预留的
拦截，不是冗余，不要删。

## 已提交内容的处置顺序

公开仓库的历史删不干净：fork、clone、`web.archive.org`、GitHub 自身的缓存都可能已经留存。
按代价从低到高：

1. **确认暴露面**：`gh api repos/<owner>/<repo> --jq '.forks_count,.stargazers_count,.subscribers_count'`。
   fork 数为 0 时，改名 + 转私有足以止血，不必动历史。
2. **改名转私有**，用新仓库从零起历史（见下节）。改名不影响已有 clone 的读权限，但会让
   根路径站点失效，所以新仓库名要占住 `jackrainman.github.io`。
3. **`git filter-repo` + force push** 只在确实需要抹除、且确认无协作者在建物时使用。
   它改写所有人本地历史的基线，是不可逆动作，需用户明确授权。

不要为了"删掉敏感内容"顺手 rebase 或 force push。

## 判断是否真的泄露了凭证

对"已删除的私有 blob"逐个扫，而不是只看当前工作树：

```bash
git log --all --diff-filter=D --name-only --pretty=format: \
  | sort -u | grep -viE '^$' > /tmp/deleted.txt
# 再在这些路径的历史版本里找凭据特征
git grep -nEI '(ghp_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|BEGIN (RSA|OPENSSH) PRIVATE KEY|-----BEGIN)' $(git rev-list --all) -- <路径> | head
```

区分两类结论：**凭据/PII 泄露**（必须轮换）与**队内内容外泄**（下架即可，无需轮换）。
不要把两者混为一谈汇报给用户。凭据不要贴进对话，建议改本地凭据存储。

## 种子新仓库：只拷必要文件

从旧仓库起一个新仓库时，**不要整仓拷贝**，那会把历史与队内内容一起带走。
本站认定的必要清单：`mkdocs.yml`、`requirements.txt`、`docs/index.md`、
`docs/zh/index.md`、`docs/img/`、`docs/javascripts/`、`docs/stylesheets/`、
`overrides/`、`.github/workflows/`、`.agents/`、两层 `.gitignore`。

新历史用一次干净的首版提交建立（在新目录里 `git init`，或 `git checkout --orphan`）。
从私有归档库回捞内容时，逐页人工过一遍再放，不批量 `git mv`。

## AI 产物归置

- 一律在 `.agents/` 下：`skills/<名>/SKILL.md`、`scripts/*.py`、提示词与笔记。
  不放仓库根目录，不放 `docs/`（会被构建进站点）。
- 工具原生配置（`.claude/settings.json`、`.opencode/` 等）留在工具要求的路径，
  但只放权限开关，且本地状态文件进 `.gitignore`。
- `docs/**/AGENTS.md`、`docs/**/*_Prompt.md`、`docs/**/chatgpt.md` 已三重拦截；
  新种类的 AI 文件要同时补 `exclude_docs` 与 `.gitignore`。
- 提交前清干净：占位符、TODO、配图脚本清单、"以下是本文即将介绍的内容"这类生成说明。

## 提交纪律

- 不用 `git add -A`；按名字加文件，加完 `git status` 复核有没有混进意外内容。
- `git diff --check` 查空白问题；提交前跑 `.agents/scripts/check_docs.py`。
- 不合并无关改动：harness、内容、配置、CI 分开提交。
