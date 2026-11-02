# Update signature probe

Runs the plugin's own signature check (`src/update-verify.cpp`) on a digest and signature. Build
`probe.cpp` with `src/update-verify.cpp` and `bcrypt.lib` (MSVC, x64), then:

    ROADIE_SIGNAL_SIGNING_KEY=<from the KB .secrets> node run.mjs <probe.exe>

It signs a test manifest with the release key and expects that to pass, and an altered manifest,
an altered signature and another key's signature to fail.
