# ChooChooTracker Web

This is an additional WebAssembly build. It does not replace the Windows or
PortMaster targets.

## Local build

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
activate it, then run from the repository root:

```bash
make -C tracker -f Makefile.web web-deploy
cd web/dist
python3 -m http.server 8080
```

Open `http://localhost:8080`. Click **Start tracker** once to unlock browser
audio, then use the normal keyboard mapping.

## Vercel

Import the repository as a normal Vercel project (leave the **Root Directory**
at the repository root). The root `vercel.json` runs the WebAssembly build and
publishes `web/dist`.

The Vercel build environment must provide `emcc`/`em++` (or run the build in a
CI step and deploy the generated `web/dist` directory). The native Windows and
PortMaster builds remain available through their existing makefiles.
