# FT8 / FT4 baseline coverage

This is a feature-family comparison for the handheld module, reviewed on
2026-09-09. The reference versions are the K4-Control iOS manual dated
2026-06-24 and the WSJT-X 2.7 user guide. It is not a claim of parity with
every setting or subsequent WSJT-X release.

K4-Control is the phone workflow reference; WSJT-X is the exchange and
interoperability reference. Public documentation is not a substitute for
operating either application on hardware. No proprietary K4-Control source
or artwork was copied.

| Capability | K4-Control iOS | WSJT-X | Handheld implementation decision |
| --- | --- | --- | --- |
| Native FT8/FT4 reception | Built in | Built in | Present; field sensitivity still needs measurement |
| Standard CQ/reply/report/acknowledgment | Supported | Supported | State machine and practice present; RF TX follows calibration |
| Automatic next message | Auto | Auto Seq | Present in practice; visible next message stays on main screen |
| Manual message selection | Supported | Supported | Standard messages present; custom free text later |
| CQ destination | Supported | Supported | Optional destination in Options |
| First responder handling | Automatic exchange | CQ responder choices | First-responder option present; ranked queues later |
| Retry control / stop | Retry limit, TX | Enable/Halt TX | Bounded practice retries and persistent Halt |
| Odd/even selection | Supported | Supported | Present; opposite period selected from a decode |
| Independent RX/TX offsets | Supported | Hold TX | Present; long press sets and holds TX independently of RX and RF split |
| Decoded-row selection | Tap/double-tap | Double-click | Tap selects; double-tap calls in practice |
| Waterfall station selection | CQ labels | Signal selection | Short tap selects by signal frequency; overlapping traces open a chooser. Callsigns are shown in the traffic list. |
| Waterfall viewport | Waterfall controls | Wide Graph controls | Actual QK4 main renderer at 0–3000 Hz; pinch, pan, +/−, Fit; zoom never tunes |
| Activity and QSO focus | RX/TX lists | Two decode panes | One list with All/CQ/My QSO filters and persistent partner card |
| Busy-band readability | Phone-oriented decode list | Resizable desktop panes | Waterfall toggle expands list; Comfortable/Compact/Dense rows; batched arrivals preserve reading position with a new-message jump |
| Activity colors | CQ/own-call/TX/worked colors | Configurable decode highlighting | Green CQ, red own-call, yellow TX, gray worked, white other; selection preserves backgrounds |
| Worked status | Configurable | ADIF highlighting | Present for callsign/band/mode; entity/grid awards later |
| Frequency presets/manual | Regional/manual | Editable working list | HF/6 m shortcuts and custom MHz; regional/favorite editor later |
| Radio preparation | Automatic RX setup | CAT/audio setup | Existing connection reused; deliberate mode/filter setup still needed |
| Logging/review | Integrated | Integrated | Present; manual review or completion auto-save |
| ADIF exchange | Import/export | ADIF log | Present; FT4 normalization and duplicate preview |
| Log search/edit/extra fields | Supported | Logger integration | Search/edit present; unknown fields retained; richer editor later |
| External logging | UDP formats | UDP messages | Later; define delivery, retry and duplicate handling |
| Propagation reporting | PSK Reporter/map | PSK Reporter | Later, after reliable reception and reporting calibration |
| Callsign lookup/device sync | Integrated services | External tools | Later; keep the core operating screen uncluttered |
| Recording/redecode/depth/AP | Not fully documented | WAV, depth, AP | Later; measure CPU, heat and latency before expanding decoder effort |
| Portable/hashed calls | Requires validation | Supported | Codec can decode some forms; complete selection/exchange coverage pending |
| Contest exchanges | Not supported | Special activities | Separate later operating profiles; no parity in preview |
| DXpedition/Hound | Limited instructions | Dedicated modes | Defer until ordinary RF QSOs and special message formats are validated |
| SuperFox / multiple simultaneous QSOs | Not documented | Dedicated support | Outside initial scope; materially different decoder/operating requirements |
| Continuing outside module | Tool can remain running | Desktop monitoring | This preview stops capture when hidden or inactive; Android service design later |

Baseline columns summarize the [K4-Control iOS manual](https://documents.roskosch.de/ham-control-elecraft-ios/)
and [WSJT-X 2.7 user guide](https://wsjt.sourceforge.io/wsjtx-doc/wsjtx-main-2.7.0.pdf).
The K4-Control manual says special contest/DXpedition modes are unsupported
while separately describing limited Hound operation; that should not be read
as full Fox/Hound or SuperFox parity.

## UX acceptance scenarios

1. Open with a short tap; hold the same control for SSTV, with no FT8 action
   on release. Return to Radio in one tap and restore landscape.
2. Set a band, then a custom frequency. Switch mode without losing the custom
   frequency. Show confirmed radio frequency, rather than pretending a rejected
   tune succeeded.
3. Locate a weak trace, zoom and pan, and select its decoded callsign without
   moving a held TX. Hold an open spot to set TX, then tap a different RX
   station and retain TX. Hold release, drag and pinch must not trigger an
   unintended selection. A trace without a matching decode cannot identify a station.
4. Scroll a busy activity list without arming a call or moving the touched row.
   Select a CQ, inspect the partner and next message, then call deliberately.
   Hide the waterfall and adjust Rows; preserve colors and reading position.
   Receive 40+ decodes together while browsing or touching a row, then jump
   to the latest messages deliberately.
5. Follow a reply and a CQ exchange through grid, reports and acknowledgments.
   Exercise RRR/RR73, missing replies, manual recovery, timeout and Halt.
6. Keep mode, partner, next message, timing and Halt readable at compact sizes.
   Put infrequent settings in a scrolling sheet with reachable Save/Back.
7. Log once after completion; correct a contact, import it elsewhere, and
   re-import without duplication or losing unfamiliar ADIF fields.
8. Interrupt audio, change frequency, disconnect, background the app and return.
   Discard stale decoder work and require deliberate rearming.

Automated checks cover portions of these scenarios. Physical touch, timing,
thermal, Android file-picker and radio behavior remain device acceptance work.
