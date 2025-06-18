# VB-CABLE installer staging

Do not commit VB-CABLE binaries here without confirming the redistribution terms for the release.

`scripts/package_release.ps1` downloads the pinned official VB-CABLE package, verifies its SHA-256 hash, and stages it under ignored `.tooling\release\vb_cable` before compiling `installer/termite.iss`.

- `VBCABLE_Setup_x64.exe`
- `VBCABLE_Setup.exe`

Termite's installer must retain the VB-Audio attribution and donationware notice. VB-CABLE A+B and C+D must not be bundled.
