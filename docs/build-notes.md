# Build notes

Run all commands from the repository root unless stated otherwise.

## Windows

Use MSYS2 UCRT64:

```sh
cd tracker
make -j8 -f Makefile.windows windows
```

The executable and bundled files are written to `tracker/build/windows/`.

## Web

Emscripten is installed locally at `.tmp/emsdk`; do not search for or install
another copy. In PowerShell:

```powershell
. .\.tmp\emsdk\emsdk_env.ps1
$env:PATH += ';C:\msys64\usr\bin;C:\msys64\ucrt64\bin'
Set-Location tracker
& "$env:EMSDK\upstream\emscripten\emmake.exe" `
  'C:\msys64\usr\bin\make.exe' -j8 -f Makefile.web web-deploy `
  COMMON_CFLAGS="-std=c++17 -Wall -g -Os -DTEST"
```

The deploy target updates the checked-in browser bundle in `web/dist/`. The
explicit `COMMON_CFLAGS` currently avoids a Windows LLVM/LTO hang.

## PortMaster (ARM64)

Use the existing Ubuntu WSL2 toolchain:

```sh
cd tracker
make -j4 -f Makefile.portmaster PortMaster-deploy \
  COMMON_CFLAGS='-std=c++17 -Wall -g -Os -DTEST'
```

The package is copied to `releases/choochootracker.zip`. The local build copy
is `tracker/build/portmaster/choochootracker.zip`. The explicit flags avoid a
current GCC 9 LTO internal compiler error.

## Validation

```sh
cd tracker
make -j8 -f Makefile.test
```
