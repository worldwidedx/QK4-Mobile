# QK4 Mobile Security Policy

QK4 Mobile is a touch-first client for Elecraft K4 transceivers.
It connects to a K4 over TLS-PSK, stores connection credentials on-device, and
parses control, display, and audio data received from the radio and the
network. We take the security of that path seriously and welcome reports from
the community.

## Supported Versions

Security fixes are applied to the **latest released version** only. Please
update to the current release before reporting an issue, and expect fixes to
ship in a new release rather than as a patch to an older one.

| Version              | Supported          |
| -------------------- | ------------------ |
| Latest release       | :white_check_mark: |
| All earlier releases | :x:                |

## Reporting a Vulnerability

**Please do not open a public issue, pull request, or discussion for a
security vulnerability.** Report it privately using either channel:

1. **GitHub private vulnerability reporting (preferred).** Go to this
   repository's **Security** tab → **Report a vulnerability**. This keeps the
   report private and tracked until a fix is released.
2. **Email:** `tcpreplay.dev@gmail.com`. Use this if GitHub private reporting
   is unavailable to you. You may encrypt sensitive details; ask in an initial
   message and we will arrange a key.

Please include, as far as you can:

- The affected version(s) and platform (Android and/or iOS) and device.
- A description of the vulnerability and its impact.
- Steps to reproduce, a proof of concept, or relevant logs/configuration.
- Any suggested remediation.

**What to expect:**

- **Acknowledgement** within 5 business days.
- An initial **assessment** (severity, affected versions) within 10 business
  days.
- Regular updates as we work on a fix, and credit in the release notes and any
  published advisory if you would like it.

## Disclosure Policy

We follow **coordinated disclosure**. We ask that you give us up to **90 days**
from acknowledgement to release a fix before any public disclosure. We will
work with you on the timing and are happy to disclose sooner once a fix is
available. If a vulnerability is being actively exploited, we may expedite a
release and disclosure.

## Safe Harbor

We support good-faith security research. If you make a good-faith effort to
comply with this policy while researching and reporting a vulnerability, we
will consider your research authorized, will work with you to understand and
resolve the issue quickly, and will not pursue or support legal action against
you. Good faith means, among other things:

- Only testing against **your own devices, your own K4, and your own network** —
  never another operator's station, account, or equipment.
- Not accessing, modifying, or destroying data that is not yours, and not
  degrading service for other users.
- Giving us a reasonable time to remediate before public disclosure, per the
  policy above.

## Scope

**In scope** (this repository's code):

- The TLS-PSK network client and connection/credential handling.
- On-device storage of K4 connection profiles and passwords.
- QRZ API credentials protected by Android Keystore or iOS secure storage.
- Parsing and handling of CAT/control, display, panadapter, and audio data
  received from the radio or network.
- The MIDI/keyer, logbook/QRZ, and file import/export paths.

**Out of scope:**

- The Elecraft K4/K4D radio firmware and hardware (report to Elecraft).
- Third-party dependencies — report to their maintainers; tell us if a version
  bump is needed here.
- The upstream QK4 desktop application (separate project).
- Findings that require physical access to an unlocked device, social
  engineering, or attacks over the amateur-radio RF path itself.
- Volumetric denial-of-service and automated scanner output without a
  demonstrated, specific impact.

Thank you for helping keep QK4 Mobile and its operators safe.
