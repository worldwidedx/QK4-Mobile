# Mobile contribution migration

Inventory taken 2026-09-20. The reference is QK4-Mobile's imported v1.0.5 phone
baseline (`5c1921333dd18bf5c3ef49c378cf9b6cd9814bd3`, sourced from archived
QK4-Android `cf2bf504a5decf9079c3dc0a9177fa645e29676e`). New work targets `main`.
Retain old PRs as provenance; do not merge the archived platform branches.

## Existing replacements and ancestry

PR numbers below refer to QK4-Mobile. Dependency entries describe the current
Git ancestry, not permission to merge inherited code without review.

| PR / head at inventory | Concern and old source | Dependencies | Acceptance still to resolve |
|---|---|---|---|
| [#1](https://github.com/worldwidedx/QK4-Mobile/pull/1), `d2860e1` | Shared TLS abstraction; archived #6 | Baseline | Contributor reports Android build and live TLS reception. Record exact hardware/build; exercise authentication failures, reconnect and RX/TX/PTT; independently validate iOS backend. |
| [#2](https://github.com/worldwidedx/QK4-Mobile/pull/2), `32c36c8` | Android build/fullscreen glue; archived #7 | Baseline | Contributor reports Samsung tablet checks; report also identifies Android 16 inset/fullscreen limitation. Resolve with exact window evidence before declaring support. |
| [#3](https://github.com/worldwidedx/QK4-Mobile/pull/3), `f3b07dd` | Shared regular UI; archived #5/#6 | Contains #2 | Review regular UI in smaller commits; physical Samsung tablet testing is reported, but exact model/build/window evidence and phone regressions remain to consolidate. Production activation gate and narrow-window fallback require review. |
| [#4](https://github.com/worldwidedx/QK4-Mobile/pull/4), `4dd85cb` | iOS services/build/distribution; archived #6/#12 | Contains #3 and #1 | Simulator build/launch reported; physical iPhone/iPad acceptance pending. Add reproducible build/dependency documentation and passing iOS CI. |
| [#5](https://github.com/worldwidedx/QK4-Mobile/pull/5), `0f6b07b` | One shared touch long-press fix; archived #9/#10 | Contains #4 | Tablet gestures reported; extract onto `main` if independent to avoid waiting for the entire stack. Confirm both phone platforms' gesture contract. |
| [#6](https://github.com/worldwidedx/QK4-Mobile/pull/6), `7d1e956` | Regular-layout keypad/B SET; archived #7/#8 | Contains #5 | Tablet keypad checks reported. iOS hardware-keyboard detection remains deferred; document it and check B SET and cancellation. |
| [#7](https://github.com/worldwidedx/QK4-Mobile/pull/7), `e13eed5` | Explicit mobile Id-meter removal | Contains #6 | PR describes prior approval; link the actual product approval and record affected phone geometry/remaining-meter acceptance. |

The dependency sequence is #2 -> #3 -> #4 -> #5 -> #6 -> #7, with #1 also feeding
#4. Keep merge ancestry recognizable, update each dependent branch after its
prerequisites land, and inspect the reduced diff before approval. Independent
fixes may be re-extracted with authorship/provenance instead of carrying the stack.

## Outstanding items

- [ ] Land development policy and CI; require checks/review on `main`.
- [ ] Retarget #1-#7 from the unused `1.0.6` integration target to `main`.
- [ ] For each PR, record source SHAs, actual phone approval where needed,
  independent review, CI results, and device evidence before merging.
- [ ] Reconcile #2's fullscreen limitation with #3's later fullscreen success
  claim using exact commits/devices; do not choose one description by assumption.
- [ ] Resolve #3's production activation and window-fit requirements without
  changing the protected phone behavior.
- [ ] Resolve iOS dependency provenance: OpenSSL/Opus static libraries need
  reproducible source/version/license/checksum/build records. A documented,
  verified vendored dependency is allowed; generated application artifacts are not.
- [ ] Inventory panadapter white-box work, NORM reference-race work, and
  screen-awake/disconnect work separately. These are not independently accounted
  for by the seven replacement titles; do not assume they are completed or approved.
- [ ] Carry test evidence into the validation inventory with source links; mark
  unknowns pending, not failed or passed.

This inventory is not approval of feature code. Feature PRs stay unmerged until
their own requirements pass. Update this checklist as work is extracted or lands.
