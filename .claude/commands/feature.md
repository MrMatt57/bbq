---
allowed-tools: Bash(git *), Bash(ls *), Bash(mkdir *), Bash(cp *), Bash(pio *), Read, Write, Edit, Glob, Grep, AskUserQuestion
---

# /feature — Create a new feature branch with spec, implement, and offer to ship

You are starting a new feature lifecycle. Follow these steps precisely.

## Step 1: Git State Check

Run `git status` and `git branch --show-current` to verify:
- Working tree is clean (no uncommitted changes)
- Currently on `main` branch

If working tree is dirty, warn the user and ask if they want to stash or commit first.
If not on `main`, ask the user if you should switch to `main` and pull latest.

Once on `main`, run `git pull origin main` to ensure you're up to date.

## Step 2: Feature Conversation

Use `$ARGUMENTS` as the starting point for the feature description. Your job is to understand the feature well enough to write a clear spec that another Claude session can implement from scratch.

- Explore the codebase (read relevant files, search for related code)
- Ask clarifying questions about scope, approach, and constraints
- Identify which files will need to be created or modified
- Consider the test plan

Keep the conversation focused. Once you have a clear understanding, move to the next step.

## Step 3: Branch Name

Determine a short, descriptive kebab-case name for the branch (e.g., `ota-updates`, `pid-tuning`, `alarm-pushover`).

Confirm the name with the user:
> Branch: `feature/{name}` — sound good?

## Step 4: Spec Numbering

Scan `.specs/*.md` for existing spec files. Find the highest `NNN` prefix number and increment by 1. If no specs exist, start at `001`.

The spec filename format is: `NNN-{branch-name}.md` (e.g., `001-ota-updates.md`).

## Step 5: Write Spec

Create the spec file at `.specs/NNN-{branch-name}.md` with this structure:

```markdown
# NNN: Feature Title

**Branch**: `feature/{name}`
**Created**: {YYYY-MM-DD}

## Summary

{2-3 sentence description of what this feature does and why}

## Requirements

- {Bulleted list of specific requirements}

## Design

{How the feature will be implemented — architecture decisions, approach, key details}

## Files to Modify

| File | Change |
|------|--------|
| `path/to/file` | Description of change |

## Test Plan

- [ ] {Specific test steps}
- [ ] Desktop tests pass (`pio test -e native`)
- [ ] Firmware builds (`pio run -e wt32_sc01_plus`)
```

Make the spec detailed enough that a fresh Claude session with no prior context can implement the feature by reading only this spec and the codebase.

## Step 6: Create Worktree

Run these commands to create a git worktree for the feature:

```bash
git worktree add ../bbq-{name} -b feature/{name} origin/main
```

This creates:
- A new directory `../bbq-{name}` (sibling to the main repo)
- A new branch `feature/{name}` tracking `origin/main`

## Step 7: Copy Spec to Worktree

Ensure the `.specs/` directory exists in the worktree and copy the spec:

```bash
mkdir -p ../bbq-{name}/.specs
cp .specs/NNN-{branch-name}.md ../bbq-{name}/.specs/
```

## Step 8: Implement

Now implement the feature directly in the worktree. Read the spec you just wrote and make all the changes described in it. Work in the worktree directory (`../bbq-{name}`), not the main repo.

After implementing, verify the test plan items from the spec:
- Run any applicable build commands or tests listed in the spec's test plan
- Confirm the changes match the requirements

## Step 9: Report and Offer to Ship

Print a summary of what was implemented, then ask:

> Done! Want me to `/ship` it?

If the user says yes, proceed with the `/ship` workflow (commit, push, PR, auto-merge, wait for merge, clean up worktree).
