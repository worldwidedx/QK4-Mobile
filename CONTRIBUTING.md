# Contributing to QK4 Mobile

Development belongs in `worldwidedx/QK4-Mobile`. QK4-Android is the historical
archive. Read [AGENTS.md](AGENTS.md), [project status](docs/PROJECT_STATUS.md),
the [phone interaction contract](docs/PHONE_UX_CONTRACT.md), and the
[orientation policy](docs/ORIENTATION_POLICY.md) before changing mobile UI.

## Branch and review workflow

1. Start from current `main` in a clean checkout or isolated worktree. Preserve
   unrelated local work. Branch names describe a concern, not a permanent product
   variant; coding-agent branches use `codex/` by default.
2. Make one reviewable change. Separate tablet geometry, platform services,
   connection/audio changes, deliberate phone changes, and unrelated fixes.
3. Target `main`. Shared fixes land once. Android/iOS and phone/tablet use the
   same development history; do not maintain separate product branches.
4. State dependencies in the PR description. Prefer merging a prerequisite
   before opening its dependent PR. For an existing stack, retain the dependency
   list, merge prerequisites first, then update from `main` and review the
   remaining diff. Never merge the same implementation from multiple branches.
5. Preserve authorship when extracting contributions. Cherry-pick focused
   commits with `-x` when appropriate; for mixed commits extract only the required
   changes and record source repository, PR, SHA, and author. Do not import a
   whole platform branch merely to obtain one fix.
6. Complete the PR template and [validation matrix](docs/VALIDATION.md). Passing
   compilation does not establish phone UX or radio/device correctness.
7. Merge only after required checks and independent review pass, with current
   base changes incorporated. Merge commits are the default during migration
   so dependent branches retain recognizable ancestry. Do not force-push `main`.
8. Delete a feature branch only after its contribution is merged and no open
   PR depends on it. Do not delete another contributor's branch automatically.

`1.0.6` was created as a proposed integration branch but initially matched
`main`. It is not a second development destination. Existing contributions
are retargeted to `main`; retain the old reference until its owner retires it.

## Ownership and approval

- `worldwidedx` owns product scope, phone UX, activation of experimental support,
  and release acceptance. An intentional phone behavior change needs explicit
  approval recorded in its PR, including affected screens and device classes.
- `tcpreplay-dev` is the existing tablet/iOS contributor and a reviewer for
  platform integration. Reviewers must examine shared effects, not just their OS.
- Shared protocol, TLS, audio, state, or transmit changes require a reviewer to
  assess compatibility and the relevant K4 acceptance results.
- Every PR needs at least one approving reviewer other than its author. This
  technical review does not substitute for product approval when that is required.
  The maintainer's own PR needs independent review too.
- CODEOWNERS routes reviews; it does not prove hardware acceptance or product
  approval. Unavailable tests remain pending. Do not label a PR approved or a
  platform supported based solely on a branch name or contributor assertion.

See [code boundaries](docs/CODE_BOUNDARIES.md), the
[migration checklist](docs/MOBILE_MIGRATION.md), and
[release and recovery procedures](docs/RELEASE_PROCESS.md).

## Automation and repository settings

The `QK4 CI` workflow runs native regression tests and an Android debug build.
It also runs the unsigned iOS compile when iOS platform sources are present.
The aggregate `Required checks` status must succeed on the current PR merge
result. Jobs run on hosted runners with read-only repository access and no
release signing or operator credentials. Fork PRs may need GitHub workflow
approval; never bypass required checks to compensate for a pending approval.

Temporary `codex/ci-validation/<concern>` branches may combine proposed code
solely to rehearse CI before its prerequisite PRs land. Pushes there run the
same checks without signing or publishing. They are not merge candidates or
release branches; record results against their exact SHA and retire them after
inspection. A successful rehearsal is not approval of the included features.

`main` requires PR review, code-owner review, current-base checks, resolved
conversations, and rejection of stale approvals. Force pushes and deletion are
disabled, including for administrators. Apply equivalent protection to any
temporary release branch before using it. Keep action references pinned to
reviewed commits and update pins through PRs.

An emergency regression is handled by a reviewed revert PR. Do not disable
checks, reset `main`, or restore an obsolete platform branch as a shortcut.
