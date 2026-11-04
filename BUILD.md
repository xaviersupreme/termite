# Building Termite

## Host (C++)

```powershell
cmake -S . -B build
cmake --build build --target termite --parallel
```

Builds `Termite.exe` (the backend) into `build\`.

## UI (Delphi)

Open `ui/TermiteUI.dproj` in Delphi and build. That creates `build\TermiteUI.exe`.

## Tests
"Basic" checks.

```powershell
cmake --build build --target termite_dsp_tests termite_audio_tests termite_eq_bridge_tests --parallel
ctest --test-dir build -C Debug --output-on-failure
```

## Packaging

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\make_release.ps1
```

The `.zip` is written to `release\Termite-win64.zip`.