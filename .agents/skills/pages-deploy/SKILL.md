---
name: pages-deploy
description: 改动 mkdocs.yml、GitHub Actions 工作流、站点地址或发布后验证时使用。覆盖用户站命名约束、site_url 与 sitemap、deploy-pages 404 的两个真因、权限最小化与线上验收清单。
---

# GitHub Pages 部署

站点：用户站 `https://jackrainman.github.io/`，由 `.github/workflows/build.yml`
用 `mkdocs build` + `actions/deploy-pages@v4` 发布到 main 分支。

## 根路径的前提：仓库名必须等于登录名

只有名为 `Jackrainman/jackrainman.github.io` 的仓库才能把站点挂在根路径。
改名成别的名字（例如 `web_next`）会**立刻**失去根路径，Pages 不跟随改名做重定向。
仓库名在 GitHub 上**大小写不敏感**：`Jackrainman.github.io` 与
`jackrainman.github.io` 是同一个名字。

由此产生两个迁移陷阱：

- 改名释放出的旧名，会被任何仍指向它的 clone 静默解析到**新仓库**。
  迁移完成后必须逐个检查旧 clone 的 `git remote -v`。
- 站点地址一旦变动，`site_url` / `edit_uri` / giscus `data-repo` 三处要同步改。

## site_url 错了不会有警告

`site_url` 留空时 `mkdocs build --strict` 全程无警告，但 `sitemap.xml` 每条
`<loc>` 都会变成 `None/...`，等于整站不可被索引——旧站带着这个毛病跑了九个月。
仓库改名后 `site_url` 残留旧名，则是 sitemap 全量指向 404 地址。

线上自查（比构建日志可靠）：

```bash
curl -s https://jackrainman.github.io/sitemap.xml | grep -c None    # 必须为 0
curl -s https://jackrainman.github.io/sitemap.xml | grep -o '<loc>[^<]*' | head -3
```

## deploy-pages 的 404 有两个真因

报错都是同一句 `Failed to create deployment (status: 404) ... Ensure GitHub Pages has been enabled`，
但成因不同，先分清再改：

1. **仓库设置**：Settings → Pages → Build and deployment → Source 必须是
   *GitHub Actions*。默认的 "Deploy from a branch" 不吃 `deploy-pages@v4`。
2. **workflow 缺 environment**：部署 job 必须声明

   ```yaml
   environment:
     name: github-pages
     url: ${{ steps.deployment.outputs.page_url }}
   ```

   `deploy-pages` 通过这个 environment 绑定 OIDC 身份。缺了它，无论 Pages 开没开都报 404。
   这条最容易漏，因为报错把嫌疑指向了设置页。

`GET /repos/<owner>/<repo>/pages` 未认证时返回 404，不能据此判断 Pages 没开。

## 权限与触发

- `pages: write` 与 `id-token: write` 写在 **job 级** `permissions`，不要写 workflow 级：
  PR 构建会继承 workflow 级授权，等于给任意 PR 发放站点写权限。
  workflow 级只留 `contents: read`。
- 部署步骤用 `if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'`
  收口，PR 只构建不发布。
- `workflow_dispatch` 作为手动重发入口；`concurrency` 组名 `pages` 且
  `cancel-in-progress: false`（半部署会留下只传了一半的站点）。

## 发布后验收（逐项做，别只看绿灯）

```bash
gh run list --limit 3                                    # 或 API：最新 run 的 conclusion
curl -s -o /dev/null -w "%{http_code}\n" https://jackrainman.github.io/
curl -s https://jackrainman.github.io/sitemap.xml | grep -c None
```

再抽查：旧路径应 404、`assets/stylesheets/*.css` 与 `img/` 资源 200、
首页 HTML 体积与本地构建同量级。构建绿 ≠ 发布成功，只有线上 200 + sitemap 正确才算完。

## 不要做的事

- 不要为了"重新发一次"而改写历史或 force push；重跑 workflow 即可。
- 不要把 Pages / OIDC 权限提到 workflow 级图省事。
- 不要在 `mkdocs.yml` 里设置 `repo_url`，除非确认要重新引入主题里的仓库图标
  （本站刻意移除了它，`edit_uri` 因此写成绝对地址）。
