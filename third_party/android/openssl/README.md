# Android OpenSSL runtime

These two ARM64 shared libraries provide the OpenSSL 3 runtime required by
Qt's Android TLS backend. They are bundled into every Android APK as
`libcrypto_3.so` and `libssl_3.so`; the application selects the `_3` suffix
before creating a `QSslSocket`.

They were sourced unchanged from the Qt-documented
[`KDAB/android_openssl`](https://github.com/KDAB/android_openssl) helper,
commit `b71f1470962019bd89534a2919f5925f93bc5779`, directory
`ssl_3/arm64-v8a`.

| File | SHA-256 |
| --- | --- |
| `libcrypto_3.so` | `1e6c12ae0c2dadfe9d178d7f80f0ab248a1877a234066098bf5594e5205e5740` |
| `libssl_3.so` | `01d2bd0baac626efd3309f35f99c4b826dd9b885a7e4d14d5b12b3603d3a407f` |

The helper repository's Apache-2.0 license is retained in `LICENSE.txt`.
OpenSSL is independently licensed under the Apache License 2.0.
