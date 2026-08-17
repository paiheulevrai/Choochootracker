# MobileGroove agent notes

## Builds

- Windows: run from `tracker` with MSYS2 UCRT64: `make -j4 windows`.
- Windows releases must ship as a complete package: include the executable,
  required DLLs, and all runtime assets/dependencies, like the PortMaster
  package.
- PortMaster: use the existing WSL2 ARM64 toolchain: `make -j4 PortMaster`.
- For a PortMaster release, run `make -j4 -f Makefile.portmaster PortMaster-deploy`.
  It writes `releases/choochootracker.zip`; validate it with
  `unzip -t releases/choochootracker.zip`. Final releases still require a
  hardware check on the target console.
- Web: Emscripten is already installed at `.tmp/emsdk`. Use PowerShell, not
  MSYS2 Bash. The SDK requires its bundled Python:

  ```powershell
  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
  . .\.tmp\emsdk\emsdk_env.ps1
  $env:PATH += ';C:\msys64\usr\bin;C:\msys64\ucrt64\bin'
  Set-Location tracker
  & "$env:EMSDK\upstream\emscripten\emmake.exe" `
    'C:\msys64\usr\bin\make.exe' -j8 -f Makefile.web web-deploy `
    COMMON_CFLAGS="-std=c++17 -Wall -g -Os -DTEST"
  ```

- If web reports `clang++.exe: permission denied`, repair the execution
  permission/security block on `.tmp/emsdk/upstream/bin/clang++.exe`; do not
  install another Emscripten SDK.
- `web-deploy` updates the checked-in `web/dist/` bundle. Regenerate and
  commit it after WebAssembly source changes; Vercel serves that bundle
  directly and does not build Emscripten itself.

## Verification and commits

- Run `make -f Makefile.test -j4` from `tracker` after engine changes.
- Before an alpha commit, update `docs/USER_MANUAL.md` and the in-app help.
- The worktree can contain user changes and deletions. Stage only files that
  belong to the current task; never include unrelated deletions in a commit.
