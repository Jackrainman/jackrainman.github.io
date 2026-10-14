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

## 已知待办（勿在无关任务中顺手改）

- `docs/zh/Chassis/底盘解算.md` 是 Obsidian 在写的长稿：已 `exclude_docs` + 脱离 git 追踪，
  写完要上线时需 `git add -f` 并重新注册 nav
- `mkdocs.yml` 的 `giscus.category_id` 是占位值 `DIC_kwD...`，评论区不可用
- 66 个代码块缺语言标注
- git 历史仍含已删的队内内容（`培养/`、`运营/` 等），未被改写
