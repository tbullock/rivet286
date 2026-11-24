# Rivet/286

Rivet/286 runs 16-bit protected-mode programs on a DOS-hosted 80286. Its
kernel manages descriptors and memory, loads PM16 applications in five memory
models, and calls back into DOS for file and process services.

## Build

Use GNU Make, NASM, and a local `legacycc` installation with the `msc600`
profile (Microsoft C 6.00A and LINK). `legacycc` uses DOSBox-X to run the
compiler tools. The default path assumes a sibling `legacycc` directory.
Point Make at an existing installation if it lives elsewhere.

```sh
make LEGACYCC_HOME=/path/to/legacycc
make demo
```

The kernel is `build/RIVET.COM`. The demo is `build/DEMO.P16`, assembled by
NASM from [its source](examples/demo.asm). All generated files live under
`build/`. `make clean` removes those files. Set `LEGACYCC_DOSBOX` if DOSBox-X
is outside your PATH.

## Run

Use an emulated 80286 running MS-DOS 6.22 with HIMEM.SYS 2.50 loaded.
Boot DOS, copy both files to a writable DOS directory, and run:

```text
RIVET DEMO.P16
TYPE RIVET.TXT
```

The demo creates `RIVET.TXT` through the protected file interface and returns
to the DOS prompt. The file contains `Hello from Rivet/286.`

## Source layout

- `src/pcat` contains the entry code, protected-mode loader, and DOS transition.
- `src/abi` defines the protected DOS request records.
- `src/heap` and `src/xms` implement protected allocation and XMS access.
- `src/startup` and `src/path` implement program startup, file paths, and child
  execution.
- [PM16 notes](docs/pm16.md) describe the small-model image and demo interface.

The initial source came from IA16 Platform Lab revision
`9600149d12e5631a6397e3abdabdd0924f71c282`. Rivet uses that tree's DOS-hosted
kernel with heap, startup, and path services. The PM16 wire format and
`PMTEST 0100` diagnostic signature retain their original identifiers.
