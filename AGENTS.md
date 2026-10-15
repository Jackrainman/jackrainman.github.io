# Web_of_Jackrainman

MkDocs Material 中文文档站，GitHub Pages 用户站 `https://jackrainman.github.io/`。
**公开仓库，提交即永久；`docs/zh/` 同时是 Obsidian vault，写了就可能被推上去。**

## 命令

```bash
pip install -r requirements.txt --index-url https://pypi.org/simple
python .agents/scripts/check_docs.py   # 体检，非零不得提交
mkdocs build --strict                  # MkDocs 官方检查
mkdocs serve                           # 本地预览
```

默认 pip 镜像在部分环境不可达，需显式指定官方源。

## 动手前先读对应技能

| 做什么 | 读哪个 |
|---|---|
| 写 / 移动 / 修订页面 | `.agents/skills/mkdocs-docs/SKILL.md` |
| 改 `mkdocs.yml`、workflow、发布验收 | `.agents/skills/pages-deploy/SKILL.md` |
| 提交、草稿拦截、仓库迁移、泄露排查 | `.agents/skills/repo-hygiene/SKILL.md` |

## 硬规则

1. AI 产物（技能、脚本、提示词、笔记）只进 `.agents/`，不落 `docs/` 也不落仓库根。
2. **不上线** = `exclude_docs` + 不进 `nav`；**不公开** = 根 `.gitignore` **且** `docs/zh/.gitignore`。
   两道闸都要过——`exclude_docs` 挡不住 `blob/main/...` 直读。
3. 已发布页面不留 AI 残留（占位符 / TODO / 配图脚本清单）；代码块开栏必须带语言。
4. 不可逆动作（force push、改写历史、删文件、动凭据）先问用户。公开仓库的历史删不干净。

## 现状

正文尚未开始，以下都是有意为之，不是待修：

- `nav` 只有 Home；首页 hero 的 stats/chips 沿用旧站数字，与当前内容不符，`landing-actions` 是空槽。
- `docs/index.md` 与 `docs/zh/index.md` 内容相同且都映射到 `/`，后者胜出，前者不产出页面。
- 未收录 17MB 的 `NotoSansSC-VariableFont_wght.ttf`，中文回退系统字体；要补先做子集化。
- giscus `category_id` 是占位值，仓库未开 Discussions，评论区不可用。
- `.gitignore` 里指向 `培养/`、`运营/` 等不存在目录的规则是给本地草稿预留的拦截，别当冗余删。
- 队内材料与旧正文在私有归档库 `Jackrainman/Jackrainman.github.io_archive`；
  本仓库历史里没有它们，回捞内容逐页人工过一遍，不要整仓拷贝。
