# Web_of_Jackrainman

MkDocs Material 中文文档站，GitHub Pages 部署。**公开仓库** —— 提交即永久。

## 命令

```bash
pip install -r requirements.txt
mkdocs serve                 # 本地预览
python .agents/scripts/check_docs.py   # 发布前校验（非零不得提交）
mkdocs build --strict        # 官方构建检查
```

PyPI 官方源为 `--index-url https://pypi.org/simple`（默认镜像在部分环境不可达）。

## 目录

- `docs/zh/<主题>/` 正文，同级 `Pictures/<页面名>/` 放图
- `mkdocs.yml` 主题、插件、`exclude_docs`、`nav`
- `.agents/` 所有 AI 相关产物（技能、脚本、提示词、笔记）
- `overrides/` 主题覆写，`docs/javascripts|stylesheets/` 前端资源

## 约定

1. **AI 相关内容一律写在 `.agents/` 下**，不放仓库根目录、不放 `docs/`。
   工具原生配置（`.claude/settings.json` 等）留在工具要求的路径，但只放权限开关。
2. 新增页面必须注册进 `mkdocs.yml` 的 `nav:`，否则游离于导航。
3. 私人/草稿/队内内容不提交：同时进 `exclude_docs` 和 `.gitignore`。
4. **不留 AI 生成痕迹**：占位符、TODO、配图脚本清单、对话文件一律清除后再提交。
5. 代码块开栏必须带语言标注。
6. 改文档先看 `.agents/skills/mkdocs-docs/SKILL.md`。

## 本仓库现状（新建，勿按旧站假设行事）

正文尚未开始，只有模板骨架。以下每一项都是有意为之，不是待修的疏漏：

- `mkdocs.yml` 的 `site_url` 与 `edit_uri` 指向 `web_next`；**仓库最终改名为
  `Jackrainman.github.io` 时这两行必须同步改**
- 未收录 17MB 的 `NotoSansSC-VariableFont_wght.ttf`，中文当前回退系统字体。
  要复刻旧站观感需补字体或做子集化，别直接整包塞进首次提交
- 首页 `landing-actions` 是空槽：内容页写出来后才有 CTA 可指
- `giscus.category_id` 仍是占位值 `DIC_kwD...`，评论区不可用（需你去 giscus.app 取）
- `.gitignore` 里的 `培养/`、`运营/` 等目录尚不存在，是给 Obsidian 草稿预留的拦截，
  不要当作冗余删掉

新增页面时的顺序：写文件 → 进 `nav:` → 跑 `check_docs.py` → 跑 `mkdocs build --strict`。
`--strict` 查不出图片缺失和 HTML `href` 死链，只有 `check_docs.py` 会查。
- git 历史仍含已删的队内内容（`培养/`、`运营/` 等），未被改写
