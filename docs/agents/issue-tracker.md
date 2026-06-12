# Issue tracker: GitHub

Issues 和 PRD 以 GitHub Issues 的形式存在。所有操作使用 `gh` CLI。

## 常用命令

- **创建 issue**：`gh issue create --title "..." --body "..."`。多行 body 使用 heredoc。
- **查看 issue**：`gh issue view <number> --comments`，通过 `jq` 过滤评论，同时获取 labels。
- **列出 issue**：`gh issue list --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'`，可按需加 `--label` 和 `--state` 过滤。
- **评论 issue**：`gh issue comment <number> --body "..."`
- **添加/移除标签**：`gh issue edit <number> --add-label "..."` / `--remove-label "..."`
- **关闭 issue**：`gh issue close <number> --comment "..."`

`gh` 从 `git remote -v` 自动推断仓库——在 clone 内运行时无需额外指定。

## 当 skill 说"发布到 issue tracker"

创建一个 GitHub Issue。

## 当 skill 说"获取相关工单"

运行 `gh issue view <number> --comments`。
