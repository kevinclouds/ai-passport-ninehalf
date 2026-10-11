<p align="right"><a href="ninehalf-provision-diagnostic.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Provisioning diagnosis — 2026-10-05

Observed: the companion mini program cannot find the device, and the in-device provisioning row remains idle after OK. No corresponding BLUFI error was captured; the radio failure's precise cause is still unknown.

Confirmed source defect: the worker discarded the provisioning start error and reported success. This version reports the actual result, shows a failure state, logs request dispatch, memory availability, callbacks, NimBLE, GATT, host-task, profile synchronization and advertising initiation. Initialization failures no longer leave a deleted network worker handle available for notification. Logs do not include SSIDs or passwords.

Build: PASS. Full validation gate, merged image layout and independent archive verification. Host tests: PASS, including native rendering of the provisioning failure state and text-width/memory checks. Device tests: flashed and data hash verified; user action reproduced NimBLE HCI initialization failure. Unverified: the actual failed initialization stage, advertising discoverability, mobile connection, security negotiation, Wi-Fi connection and record synchronization. This diagnostic does not claim that scanning has been fixed.

Write the verified 2,093,392-byte merged image at `0x0` only after confirmation. Working tree: `main`, uncommitted.

Full SHA-256: `9370e28c0082151faae2485524de4229e85a0375c914f1ba053c23abc6a450a0`

ELF SHA-256: `199d2c21e7970c59ce09c1e02fd00438e1afc3973a6ec0578e8019b46da6ada7`

Archive: `build/firmware/9370e28c0082151faae2485524de4229e85a0375c914f1ba053c23abc6a450a0`
