## Change

Describe the problem and resulting behavior. Identify any intentional phone
behavior change and its maintainer approval; a platform port or tablet layout
does not implicitly approve one.

## Scope

- Affected screens and entry/exit paths:
- Affected device classes (Android phone, Android tablet, iPhone, iPad):
- Orientation, layout selection, or window-support changes (or none):

## Validation

List checks actually run and their results. For device checks, include model,
OS version, build/commit, orientations, and window configuration. Mark checks
not run as pending, with the reason; do not mark an unavailable platform passed.

For UI/layout/orientation changes, complete the following or explain why an
item is not applicable. See the [orientation policy](https://github.com/worldwidedx/QK4-Mobile/blob/main/docs/ORIENTATION_POLICY.md).

- [ ] The change follows the policy; any proposed exception is explicitly identified and approved.
- [ ] Android-phone and iPhone behavior is preserved; affected phone regression results or pending checks are listed.
- [ ] Device classification is shared, and narrow windows use a usable compact fallback without changing phone orientation rules.
- [ ] Required controls remain reachable; tap, long press, and scroll cancellation retain their documented behavior.
- [ ] Affected module/logbook entry, rotation, return, and background/resume paths were checked.
- [ ] Keyboard, safe-area/system insets, and every claimed resize or multitasking configuration were checked.
- [ ] Physical tablet acceptance supports any regular-layout or tablet-orientation activation, separately for Android tablet and iPad.
- [ ] Evidence and limitations are recorded in the [device-validation table](https://github.com/worldwidedx/QK4-Mobile/blob/main/docs/PROJECT_STATUS.md#device-and-orientation-validation) or linked report; requirements are not presented as verified support.
