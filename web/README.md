# ChooChooTracker Web

This is an additional WebAssembly build. It does not replace the Windows or
PortMaster targets.

## Local build

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
activate it, then run from the repository root. On Windows/MSYS2, point
`EMSDK_PYTHON` to the bundled interpreter *before* activation (the activation
script invokes the `emsdk` launcher itself):

```bash
export EMSDK_PYTHON="$(pwd)/.tmp/emsdk/python/3.13.3_64bit/python.exe"
source .tmp/emsdk/emsdk_env.sh
make -C tracker -f Makefile.web web-deploy
cd web/dist
python3 -m http.server 8080
```

The checked-in SDK location used by this project is `.tmp/emsdk`. Verify the
setup with `em++ --version` before starting a full build. Do not use the native
Windows `make` for this target: `Makefile.web` needs a Unix shell for `mkdir`
and `cp`.

Open `http://localhost:8080`. Click **Start tracker** once to unlock browser
audio, then use the normal keyboard mapping.

## Vercel

Import the repository as a normal Vercel project (leave the **Root Directory**
at the repository root). The root `vercel.json` publishes the checked-in
`web/dist` bundle as a static site; Vercel does not need Emscripten installed.

After changing the tracker, regenerate `web/dist` with `make -C tracker -f
Makefile.web web-deploy` and commit the generated bundle. The native Windows
and PortMaster builds remain available through their existing makefiles.
