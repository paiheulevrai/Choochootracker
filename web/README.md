# ChooChooTracker Web

This is an additional WebAssembly build. It does not replace the Windows or
PortMaster targets.

## Local build

Use the [central build instructions](../tracker/README.md#web--vercel) to
generate `web/dist`. The checked-in Emscripten SDK lives in `.tmp/emsdk`; use
MSYS2 Bash rather than native Windows `make`.

To preview the generated bundle:

```bash
cd web/dist
python3 -m http.server 8080
```

Open `http://localhost:8080`. The tracker loads automatically; tap a tracker
control or press a key to activate browser audio, then use the normal mapping.

## Browser settings and recovery

The surrounding page follows the active tracker theme. Theme changes and saved
settings, including **Done** in Key Mapping, are flushed to persistent browser
storage promptly instead of relying only on the periodic save or page exit.

After returning from a backgrounded tab or screen lock, the browser build clears
stale held-input state and attempts to resume previously activated audio. Normal
startup autoplay suspension is not reported as an audio failure.

- **Restart audio** reopens the browser output while retaining the current
  project and instruments. Press Play again if needed.
- **Reload app** saves the current project to **Continue** (`autosave.cct`),
  saves settings, and waits for browser storage to finish before reloading.
  Saving failure or timeout cancels the reload. This does not create a new named
  song file or download a backup; **Download .cct** remains the portable backup.

Storage belongs to this browser profile and site origin. Clearing site data can
remove it, and a Home Screen installation may use different storage from a
regular Safari tab. A browser refresh or forced app termination cannot guarantee
the same awaited save sequence as Reload app. Recovery cannot guarantee that
iOS will expose a USB controller again or select a particular Bluetooth output.

## Optional regression checks

After regenerating the bundle, run `node --test tools/test_web_storage.cjs` from
the repository root. For the browser integration check, use an existing Python
environment with Playwright and its Chromium browser:

```bash
python3 tools/test_web_browser.py
```

Set `CHROME_EXECUTABLE` to use an installed Chrome instead, or `WEB_TEST_URL` to
check a served build. Tests use isolated browser profiles and cover theme and
mapping persistence, storage failures, audio interruptions, save-before-reload,
and synthetic gamepad wake/reconnect. They do not establish physical Safari,
iOS USB-controller, screen-lock or Bluetooth behavior. Playwright is optional
test tooling, not an application dependency.

### Physical iPhone checks before submission

Run these against the exact contribution build in Safari and a Home Screen
installation. Record the commit, iOS version, controller and audio output used;
automated Chromium results do not substitute for these checks.

- Change theme and mappings, confirm **Done**, reload and fully reopen the app.
  Check that both settings persist within each installation.
- Play and edit a song, lock/unlock repeatedly and switch apps. Confirm editing
  still works, audio recovers, and no key or touch control remains held.
- Test USB controller input at launch, after lock/unlock, and after unplugging
  and reconnecting. Check the custom mappings as well as keyboard/touch input.
- Test speaker and Bluetooth output before and after backgrounding; record
  routing failures separately from stopped playback. Check iPhone Silent Mode.
- Check that a fresh launch stays quiet until audio is activated. Exercise
  **Restart audio** and verify the song and instruments remain intact.
- Edit a disposable song, use **Reload app**, then **Continue** and verify the
  edits survived. Also download a `.cct` backup and reopen it.

## Vercel

Import the repository as a normal Vercel project (leave the **Root Directory**
at the repository root). The root `vercel.json` publishes the checked-in
`web/dist` bundle as a static site; Vercel does not need Emscripten installed.

After changing the tracker, regenerate and commit `web/dist`. The build command
and the other platform instructions are kept in
[`tracker/README.md`](../tracker/README.md).
