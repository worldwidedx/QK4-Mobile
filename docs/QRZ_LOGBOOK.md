# QRZ Logbook

Open the shared Logbook and choose **Setup**. Enter the station callsign for
the target QRZ logbook and its **API key**, found at QRZ Logbook → Settings →
API Access. Enable **Auto-send new contacts to QRZ** to upload future FT8,
FT4, SSTV and manually added contacts when they are saved. Existing contacts
and ADIF imports are uploaded only through **Send to QRZ**.

QRZ requires a subscription for its INSERT API. The username/password XML
interface is for callsign data lookups; it does not insert QSOs. Each exact
station callsign, including a portable suffix, has its own QRZ logbook/key.
An existing contact whose My callsign differs from Setup is rejected locally.
An empty My callsign is filled from Setup when the operator sends the contact.

## Upload status

- An unchecked contact has not been confirmed as accepted by QRZ. Select it
  and press **Send to QRZ**; failures remain selectable for retry.
  Tap anywhere in its row, including the checkbox area, to select it. The
  selected callsign appears above the action button. Scrolling does not change
  the selection or disable a later tap of Send to QRZ.
- The checkbox is read-only. A successful QRZ INSERT response must contain
  `RESULT=OK`, `COUNT=1`, and a positive `LOGID` or `LOGIDS` before it is checked.
- A checked contact cannot be sent again. Local edits preserve the locked
  checkbox and mark the ADIF status as modified after upload. They do not
  overwrite the remote QSO.
- Uploads run asynchronously, one at a time. Queued records survive restart.
  An interrupted in-flight request is marked as uncertain and is not blindly
  retried: check QRZ before using the manual retry action. Duplicate records
  are not overwritten (`OPTION=REPLACE` is never sent).
- Practice contacts cannot upload. Removing the key disables auto-send and
  retains the log and its upload history.

ADIF export/import supports the standard `QRZCOM_QSO_UPLOAD_STATUS` (`Y` for
accepted, `M` for locally modified after upload) and `QRZCOM_QSO_UPLOAD_DATE`
(UTC `YYYYMMDD`). An absent upload status remains absent; `N` means "do not
upload" in ADIF, rather than simply "not uploaded yet". Remote log IDs use
`APP_QRZLOG_LOGID`. Private queue metadata is excluded from ADIF.

## Credential storage and transport

The Android helper encrypts the API key using AES-256-GCM with a fresh random
IV, authenticated version context and a non-exportable Android Keystore key.
Ciphertext is atomically written to the application's private no-backup
directory. Callsign and auto-send preference may be stored in QSettings; the
API key is never stored there, in the logbook, or in ADIF. Decryption/storage
failure has no plaintext fallback. Desktop transport tests use an injected,
in-memory credential provider.

Requests are form-encoded HTTPS POSTs to `https://logbook.qrz.com/api`, with an
identifiable QK4 user agent, normal TLS verification, no automatic redirects,
a bounded response and a request deadline. Credentials never appear in a URL.
Server error messages are plain text and redact the API key before display
or storage. The setup field starts masked and suppresses keyboard prediction.
**Show** reveals the typed key, or decrypts the saved key when the field is
empty; **Hide** masks it again. Leaving the app also masks the field. Revealing
or cancelling Setup does not replace the stored credential.

## Validation

Desktop tests cover accepted/rejected/malformed responses, manual retry,
interrupted requests, concurrent FT/SSTV logging, practice/mismatched-call
rejection, credential-provider failures, form encoding, locked checkboxes,
local edits and ADIF round trips. Live QRZ account upload acceptance requires
an operator-provided API key and has not yet been validated.

References checked September 10, 2026:

- [QRZ Logbook API](https://www.qrz.com/docs/logbook/QRZLogbookAPI.html)
- [QRZ XML callsign-data interface](https://www.qrz.com/page/current_spec.html)
- [ADIF standard](https://adif.org/adif/)
- [ADIF upload-status enumeration](https://adif.org/304/ADIF_304.htm#QSO_Upload_Status_Enumeration)
