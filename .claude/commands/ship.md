---
allowed-tools: Bash(git *), Bash(gh *), Read, Glob, Grep
---

# /ship — Commit, push, PR, and auto-merge the current feature

You are shipping a completed feature. Follow these steps precisely.

## Step 1: Validate

Run `git branch --show-current` to get the current branch.

- **Must** be on a `feature/*` branch. If on `main`, stop and tell the user this command only works from a feature branch.
- Extract the feature name from the branch (e.g., `feature/ota-updates` -> `ota-updates`).

Run `git status` and `git log origin/main..HEAD --oneline` to verify there are either:
- Uncommitted changes to stage, OR
- Commits ahead of `origin/main` to push

If there's nothing to ship (no changes, no new commits), stop and tell the user.

## Step 2: Stage and Commit

If there are uncommitted changes:

1. Run `git diff` and `git diff --cached` and `git status` to review all changes
2. Stage relevant files with `git add <specific-files>` (avoid `git add -A` to prevent accidentally staging sensitive files)
3. Create a descriptive commit message summarizing all changes. Use this format:

```bash
git commit -m "$(cat <<'EOF'
Description of the change

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

If all changes are already committed, skip this step.

## Step 3: Push

Push the branch to the remote:

```bash
git push -u origin feature/{name}
```

## Step 4: Create PR

Look for a spec file in `.specs/` that matches the branch name. Read it for context about the feature.

Create the PR using the spec's summary and test plan:

```bash
gh pr create --title "Short descriptive title" --body "$(cat <<'EOF'
## Summary
- {Bullet points from spec summary and what was implemented}

## Test plan
- [ ] Desktop tests pass (`pio test -e native`)
- [ ] Firmware builds (`pio run -e wt32_sc01_plus`)
- [ ] {Any feature-specific test steps from the spec}

Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Keep the title under 70 characters. Use the body for details.

## Step 5: Auto-merge

Enable auto-merge so the PR merges automatically when CI passes:

```bash
gh pr merge --squash --auto --delete-branch
```

## Step 6: Report

Print a clear summary:

```
Shipped!

  PR:     {PR URL}
  Branch: feature/{name}
  Status: Auto-merge enabled (will merge when CI passes)

Monitor CI:
  gh pr checks --watch

After merge, clean up the worktree:
  cd {main-repo-path}
  git worktree remove ../bbq-{name}
```
