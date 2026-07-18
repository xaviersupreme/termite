# Termite

Termite is a Windows equalizer for audio routed through VB-CABLE. It reads
`CABLE Output`, sends it to the current default playback device, and applies
one shared EQ profile.


What it does
------------

* 20-band graphic EQ and arbitrary EQ
* `.tsf` profile files
* Per-application CABLE routing while an application has an active audio
  stream
* VCL styles, including the user’s last selected theme
* Restores routes changed during a Termite run when the program closes


Before running it
-----------------

Install [VB-CABLE](https://vb-audio.com/Cable/) yourself. Windows must show
both `CABLE Input` and `CABLE Output`.

Do not make `CABLE Input` your default playback device. That makes a feedback
loop. Route an application to it instead, then Termite can pick up its audio
from `CABLE Output`.

The Route apps list only changes applications that Windows currently exposes
as audio clients. Start playback first if an application does not respond to a
route change.


Build
-----

The C++ host uses CMake. From the repository root:

```powershell
cmake -S . -B build
cmake --build build --target termite --parallel
```

The frontend is [ui/TermiteUI.dproj](ui/TermiteUI.dproj). Open it in Delphi,
select **Debug | Win64**, and build it. It writes
`build\Win64\Debug\TermiteUI.exe`.

Build the C++ host once more after Delphi. CMake copies `TermiteUI.exe` next
to `Termite.exe`. Delphi Community Edition is used here, so the VCL project is
built from the IDE, not the command line.

To make a release ZIP after both builds are finished, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\make_release.ps1
```

Upload `release\Termite-win64.zip` to GitHub. A person installing it extracts
the ZIP and runs `Install-Termite.ps1`. The script asks whether the app belongs
in `%LocalAppData%\Programs` or `%ProgramFiles%`. VB-CABLE itself is a driver,
so Windows will ask for administrator approval when it is first installed.

Run the tests with:

```powershell
cmake --build build --target termite_dsp_tests termite_audio_tests termite_eq_bridge_tests --parallel
ctest --test-dir build --output-on-failure
```


The files
---------

```
host/       process lifetime, named-pipe bridges, Windows glue
sound/      WASAPI, routing policy, equalizer model
ui/         Delphi VCL frontend
assets/     Termite-owned artwork
reference/  local reverse-engineering material; deliberately ignored by Git
tests/      small tests for the things that should not quietly drift
```

License
-------

Termite is MIT licensed; see [LICENSE](LICENSE). VB-CABLE is separate software
from VB-Audio and has its own terms.
