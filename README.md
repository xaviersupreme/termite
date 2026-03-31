# Termite

Windows 10/11 tray equalizer for apps routed through VB-CABLE.

## Users

Download and run `termite_setup.exe`. It includes Termite, the Microsoft Visual C++ runtime, and VB-CABLE. No build tools or developer runtimes are required. If VB-CABLE is missing, its VB-Audio setup window is shown during installation and Termite launches only after Windows exposes both `CABLE Input` and `CABLE Output`.

Restart Windows if the driver setup requests it. If the endpoints are still absent, use **Start Menu → Termite → Install or repair VB-CABLE**, then restart if prompted.

After installation, choose each app's output as `CABLE Input` once in **Windows Settings → System → Sound → Volume mixer**. This is a Windows setting, not a configuration file.

## Contributors

Run one elevated PowerShell command from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

It installs missing build tools with WinGet, builds the native Direct2D frontend, runs tests, stages the release dependencies, and creates `artifacts\termite_setup.exe`.

The first run needs internet access and administrator approval for Visual Studio Build Tools. The application itself uses only the Windows SDK.

For the DSP tests only, no Windows SDK is required:

```powershell
cmake -S . -B build-core -G Ninja -DTERMITE_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Use

1. Run `termite_setup.exe`; it installs VB-CABLE visibly when needed and verifies both endpoints. Reboot if prompted.
2. Start Termite. It captures `CABLE Output` and renders to the current Windows default speakers/headphones.
3. Select each app's output as `CABLE Input` in **Windows Settings → System → Sound → Volume mixer**.
4. Adjust the shared 20-band graphic EQ profile. Every app sent to `CABLE Input` receives it.

Termite saves its working profile, console state, window position, and selected-app reminders under `%LOCALAPPDATA%\Termite\settings.json`. Closing the console hides it to the notification area while processing continues; use **Quit** from the tray menu to stop it.

Use **Hardware** to inspect the active capture/render endpoints, formats, buffer fill, xruns, and recovery reason.

The default playback device must never be `CABLE Input`; that would create an audio feedback loop.

## Audio validation

After installing VB-CABLE, route a browser or media app to `CABLE Input` in Windows Volume Mixer and confirm that only that app is affected. Test a flat profile, a boost and cut, 44.1/48 kHz content, switching the default speakers/headphones while playing, and disabling/re-enabling the cable endpoint. Report the Hardware diagnostics if Termite does not recover.

## VB-CABLE

VB-CABLE is a separate VB-Audio donationware driver. Its origin and donation page are <https://vb-audio.com/Cable/>. The installer staging instructions and redistribution constraints are in [third_party/vb_cable/README.md](third_party/vb_cable/README.md).

## Licensing

Termite is released under the MIT License. See [LICENSE](LICENSE).
