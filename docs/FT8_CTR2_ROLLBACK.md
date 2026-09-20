# FT8 CTR2 Adjust/Set rollback marker

The **before-unified-tone** checkpoint in
`docs/generated/rollback-ft8-unified-tone-20260911/` preserves the corrected
wheel routing before unifying all tone selectors under preview-and-set.
It includes source/document copies, a hash manifest and `before-unified-tone.apk`
(SHA-256 `75DCDA61B79F8F0DAA46FF5FDC0C383C546B8D6EBC9EE2D9129CA49083904BF1`).
Mapping IDs are unchanged; this checkpoint restores immediate RX/TX selector
behavior if needed. Compare checkpoint copies and preserve later unrelated edits.

A second checkpoint, **before-ft8-tone-recovery**, preserves the installed
Adjust/Set design before the tone-step and RF-target recovery correction.
It is stored in `docs/generated/rollback-ft8-tone-recovery-20260910/` with
six source/test/document copies, `manifest.json`, and `before-tone-recovery.apk`
(SHA-256 `9462B755789D649DC8EED21AD2B2027B11BDDE35A9A8709C0C4FB63BAFAC9D3A`).
Use this checkpoint to roll back only the recovery correction while retaining
the assignable short-press Adjust and long-press Set actions. The correction
does not change mapping keywords or saved button assignments.

Marker: **before-ft8-ctr2-adjust-set**, created September 10, 2026 before
implementing the assignable RX/TX preview-and-commit workflow.

The local checkpoint is `docs/generated/rollback-ft8-adjust-set-20260910/`.
It contains byte-for-byte copies of the 15 affected source/document files,
their SHA-256 hashes in `manifest.json`, the Git HEAD and branch, and
`before-adjust-set.apk`. The archive is ignored by Git. It preserves the
existing dirty working tree; a tag on HEAD alone would not capture this state.

The previous signed APK hash is
`28A121A32CB9B85C191E736D4C8D1431160EF8F1A831546C7705290A29ADF8BA`.
It includes the CTR2 NoteOn correction and removal of waterfall callsign labels.
It predates the Adjust/Set workflow and was not installed on the test phone.

To revert only this work, compare the listed files with their checkpoint
copies and restore the relevant changes, preserving any subsequent unrelated
edits. Do not reset the repository to HEAD: the checkpoint includes preexisting
uncommitted FT8, SSTV, logbook and setup work. Rebuild with the documented
Android wrapper and the existing signing identity. A device-only rollback can
install the preserved signed APK in place without uninstalling or clearing data.

New mapping keywords are `adjust_ft8_rx_tx` and `set_ft8_frequency`.
Before installing the previous APK, replace any assignments using those new
keywords with the former `ft8_rx` and `ft8_tx` actions; the previous version
cannot load a mapping containing unknown action keywords. Other existing
button assignments are not migrated automatically by this feature.
