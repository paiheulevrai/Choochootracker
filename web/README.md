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

Open `http://localhost:8080`. Click **Start tracker** once to unlock browser
audio, then use the normal keyboard mapping.

## Vercel

Import the repository as a normal Vercel project (leave the **Root Directory**
at the repository root). The root `vercel.json` publishes the checked-in
`web/dist` bundle as a static site; Vercel does not need Emscripten installed.

After changing the tracker, regenerate and commit `web/dist`. The build command
and the other platform instructions are kept in
[`tracker/README.md`](../tracker/README.md).
