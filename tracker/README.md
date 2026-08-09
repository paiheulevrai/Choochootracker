# ChooChooTracker build instructions

The English user manual is available at [`../docs/USER_MANUAL.md`](../docs/USER_MANUAL.md).

Run commands from this `tracker` directory.

## Windows

Requirements: MSYS2 UCRT64, GCC and SDL2.

```bat
build-windows.bat
```

Output: `build/windows/choochootracker.exe`.

## Tests

```sh
make -f Makefile.test
```

The suite covers the tracker engine, Braids, the PCM Sample voice and instrument FX behavior.

## PortMaster / ArkOS

The current target is AArch64, including the Anbernic RG353V. From Ubuntu under WSL2:

```sh
make -f Makefile.portmaster PortMaster-deploy
```

Outputs:

- `build/portmaster/choochootracker.aarch64`
- `build/portmaster/package/`
- `../releases/choochootracker.zip`

The ZIP contains `ChooChooTracker.sh`, the gamepad mapping, data files, licenses and the ARM64 executable.

See [development notes](../docs/development-notes.md) for toolchain details and [fork maintenance](../docs/fork-maintenance.md) for upstream policy.
