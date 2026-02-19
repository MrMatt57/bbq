---
allowed-tools: Bash(git *), Bash(gh *), Read, Glob, Grep
---

# /ship — Commit, push, PR, auto-merge, wait, and clean up

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

The test plan checklist items should be **checked off** (`[x]`) — the implementation and verification was already done before `/ship` was called. Only leave items unchecked if they genuinely weren't verified.

Create the PR using the spec's summary and test plan:

```bash
gh pr create --title "Short descriptive title" --body "$(cat <<'EOF'
## Summary
- {Bullet points from spec summary and what was implemented}

## Test plan
- [x] Desktop tests pass (`pio test -e native`)
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
- [x] {Any feature-specific test steps from the spec — checked off}

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

## Step 6: Wait for Merge

Tell the user the PR is created and you're waiting for CI, then watch for checks to complete:

```bash
gh pr checks --watch
```

Once checks finish, poll until the PR is merged (auto-merge may take a moment after checks pass):

```bash
# Poll every 10 seconds, up to 2 minutes
for i in $(seq 1 12); do
  state=$(gh pr view --json state --jq '.state')
  if [ "$state" = "MERGED" ]; then break; fi
  sleep 10
done
```

If the PR doesn't merge after polling, inform the user and provide the PR URL.

## Step 7: Clean Up

After the PR is merged:

1. Switch to the main repo directory (`C:\dev\bbq\bbq`)
2. Pull latest main
3. Remove the worktree

```bash
cd {main-repo-path}
git pull origin main
git worktree remove ../bbq-{name}
```

If the worktree removal fails (e.g., dirty files), inform the user.

## Step 8: Report

Print a clear summary:

```
Shipped and merged!

  PR:     {PR URL}
  Branch: feature/{name} (merged and deleted)
  Worktree: cleaned up
```

If the PR hasn't merged yet (timeout), instead print:

```
PR created and auto-merge enabled:

  PR:     {PR URL}
  Branch: feature/{name}
  Status: Waiting for CI — will auto-merge when checks pass

Worktree still exists at ../bbq-{name} (clean up manually after merge)
```
