# Rivet/286

Rivet/286 runs 16-bit protected mode programs under DOS on an Intel 80286.
It gives each program a 512 KiB heap above the first megabyte of RAM. DOS
still provides the filesystem and can run other DOS programs on the
application's behalf.

The extender is `RIVET.COM`. Applications use the PM16 executable format.
The included assembly example writes a file and returns to the DOS prompt.
Rivet grew out of PMTEST, a DOS hosted operating system for testing the
IA-16 compiler suite. [To-do list](TODO.md)

## Build

You need GNU Make, NASM, DOSBox-X and a `legacycc` installation with the
`msc600` profile. Make uses NASM for the assembly sources and runs
Microsoft C 6.00A and LINK through `legacycc`.

```sh
make all demo
```

The output files are `build/RIVET.COM` and `build/DEMO.P16`. Make looks for
`legacycc` in `../legacycc`. To use a different installation, pass
`LEGACYCC_HOME=/path/to/legacycc` to Make. Set `LEGACYCC_DOSBOX` to the
DOSBox-X executable if it is outside your PATH. `make clean` removes the
build directory.

On macOS, open `Rivet286.xcodeproj` and press Command-B in the `Rivet286`
scheme. Xcode runs `make all demo` in the checkout. If necessary, set
`LEGACYCC_HOME` in the target's build settings.

## Run the example

Boot DOS with HIMEM.SYS loaded and EMM386 disabled. The example runs under
MS-DOS 6.22 with HIMEM.SYS 2.50 on the DOSBox-X 286 and 386 configurations
with 4 MiB of RAM. Copy both output files into a writable DOS directory.

```text
RIVET DEMO.P16
TYPE RIVET.TXT
```

The first command creates or overwrites `RIVET.TXT`. The second prints
`Hello from Rivet/286.` [Assembly source](examples/demo.asm) ·
[Writing a PM16 program](docs/pm16.md)

## Addressing memory on the 286

On a typical PC, DOS, its drivers and the running program share 640 KiB
of conventional RAM. A real mode program addresses a byte with two numbers.
The CPU multiplies the segment number by 16 and adds the offset. The highest
address it can form this way is just under 64 KiB past the first megabyte,
with address line A20 enabled.

Suppose a program has a 200 KiB table. Under DOS it can keep the table in
conventional memory, use an XMS driver to copy parts of it down from
extended memory, or use expanded memory (EMS) to map parts into a window.
Under Rivet the program can read and write the table directly in extended
memory.

Protected mode changes the meaning of the segment register. It holds a
**selector** that identifies a table entry called a **descriptor**. Rivet
fills in the segment's starting address, length and access rights. The
CPU adds the program's offset to that starting address and checks that the
access is allowed.

A descriptor can point anywhere in the 286's **16 MiB** physical address
space. A 16-bit offset can reach **64 KiB** from that starting address.
The program can therefore cover the 200 KiB table with four segments.
On a machine with 16 MiB of address space, 15 MiB lie above the first
megabyte. HIMEM manages the available RAM in that range. The 286 also
supports separate privilege levels. Rivet runs one trusted application at
the same privilege level as the extender. [Intel 80286 Programmer's
Reference Manual, figure 6-6][intel286]

## Executable and heap limits

Rivet can load **195,840 bytes of code** in a PM16 file of up to **457,648
bytes**. The loader places code, static data and stack in conventional
memory. Each code segment, or bank, holds up to 65,280 bytes. A far call
transfers control from one bank to another.

| PM16 model and version | Code banks | Maximum code (bytes) | Maximum file (bytes) | Conventional RAM reserved for the application |
| --- | ---: | ---: | ---: | ---: |
| Small, 0100h (demo) | 1 | 65,280 | 130,078 | 128 KiB |
| Medium, 0200h | 2 | 130,560 | 195,406 | 192 KiB |
| Compact, 0200h | 1 | 65,280 | 260,976 | 320 KiB |
| Compact, 0201h or large, 0200h | 2 | 130,560 | 326,272 | 384 KiB |
| Huge, 0300h | 2 | 130,560 | 392,344 | 448 KiB |
| Huge, 0301h | 3 | 195,840 | 457,648 | 512 KiB |

The file contains a header, code and initialized data. The loader allocates
space for the stack and zero-initialized variables. Small and medium
programs share one segment between data and stack. The other models have
a separate stack.

The heap is the same size in every model. Rivet asks HIMEM for one
contiguous 512 KiB block below the 16 MiB mark. An application can take the
whole heap in one allocation and access it in eight 64 KiB segments, or
divide it among up to 112 allocations. For the largest huge program, Rivet
reserves 512 KiB of conventional memory plus the 512 KiB heap. The extender
itself occupies another 64 KiB of conventional memory.

A 286 can execute a program with 1 MiB of code. At PM16's bank size, the
linker would have to divide the code among at least 17 banks, and the
loader would have to place them in extended memory. Rivet currently loads
at most three.

## DOS services

PM16 programs call Rivet through interrupt 60h. For a file write, Rivet
copies up to 512 bytes at a time into a buffer DOS can address. It switches
to real mode, calls DOS, then resumes the program in protected mode. On a
286 the return to real mode requires a CPU reset. Rivet gives the PC/AT
BIOS a return address before the reset, so execution resumes with the
program still in RAM.

Programs can open, create, read, write, seek, truncate and close files.
They can hold eight Rivet file handles at once and use DOS standard input
and output for redirection. Start an application with
`RIVET /R APP.P16 arguments` to pass it a command line and environment.
It can search DOS directories, run a DOS `.COM` or `.EXE` and collect
the child's exit status.

## DOS extenders on the 286 and 386

| Environment | Where the application lives | Available services |
| --- | --- | --- |
| [Real mode DOS][msdos] | A `.COM` starts in one 64 KiB segment. An MZ `.EXE` can span segments within the available DOS block. Overlays bring additional code in from disk. | DOS and BIOS interrupts |
| [Rational DOS/16M][dos16m] (286) | Both code and data can occupy segments in extended memory. | DOS calls, direct hardware access and a protected mode debugger |
| Rivet/286 | PM16 code and static data occupy conventional RAM. The heap occupies extended memory. | File and process services through interrupt 60h |
| [Watcom DOS/4GW][watcom] (386) | A 32-bit Linear Executable (LE) uses a flat address space. | DOS and BIOS calls, interrupt handling, common DOS Protected Mode Interface (DPMI) services and optional disk-backed virtual memory |

The 386 added 32-bit registers and offsets. With a flat memory model, one
pointer can walk the entire 200 KiB table. The CPU supports 4 GiB of linear
addresses. Paging lets an operating system map those addresses to RAM in
4 KiB pieces. Virtual 8086 mode lets it run real mode software under a
protected mode supervisor. The 386 can also return to real mode without
a CPU reset. Rivet uses the same 16-bit addressing and PC/AT reset on
both processors. [Intel 386 memory addressing][intel386] · [Virtual 8086
mode][v86]

## Running SPICE

To run Berkeley's SPICE3f5 circuit simulator, Rivet needs to load code in
extended memory. One 286 build contains 401,684 bytes of code in seven
banks. With PMTEST's v4 loader and C runtime, it runs on an emulated IBM AT
and uses software for double precision arithmetic.
[Berkeley SPICE3f5][spice]

## The 80287 math coprocessor

The 80287 keeps its own registers, rounding mode and error state. When
the 286 resets to call DOS, the original 80287 stays in protected mode.
An extender must save the program's math state, prepare the coprocessor
for DOS, then restore that state before the program resumes.

On an emulated IBM AT, PMTEST's small model runtime uses the 80287 for
single precision addition, subtraction, multiplication and division. It
falls back to software when the chip is absent. Rivet still needs the
runtime and the code that preserves the chip's state across DOS calls.

## Porting a game like Doom

The original DOS Doom requires a 386 or later, 4 MiB of RAM and VGA.
Its executable uses instructions the 286 cannot execute. Doom8088 is an
example of a source port for 16-bit PCs. Its renderer simplifies the scene
by drawing floors and ceilings without textures. [Doom system
requirements][doom] · [Doom8088][doom8088]

A Rivet port would need 286 implementations of the renderer's 32-bit fixed
point arithmetic. A 320 × 200 frame at one byte per pixel fits in a single
segment. Maps and textures would occupy other segments or load from disk as
needed. Rivet supplies the file access. The port would also need access to
VGA memory, keyboard input and a timer. Digitized sound requires sound card
interrupts and DMA buffers. [id Software's renderer notes][renderer]

## Source layout

| Directory | Contents |
| --- | --- |
| `examples/` | Assembly demo |
| `src/pcat/` | Program loader, protected mode entry and return to DOS |
| `src/abi/` | DOS service request records |
| `src/heap/`, `src/xms/` | Heap allocator and XMS driver interface |
| `src/startup/`, `src/path/` | Application startup, DOS paths and child processes |

## Source style

Format C sources and headers with the LLVM style in `.clang-format`.

```sh
git ls-files -z '*.c' '*.h' | xargs -0 clang-format -i
```

Keep shared code compatible with Microsoft C 6.00A. Use segmented pointer
qualifiers in platform code and at the application interface. The kernel
and path extension each compile several C files as one translation unit.
Keep their implementation includes in order.

In NASM files, put labels at the left margin, indent instructions four
spaces and put a space after each operand comma.

[intel286]: https://bitsavers.trailing-edge.com/components/intel/80286/210498-005_80286_and_80287_Programmers_Reference_Manual_1987.pdf
[intel386]: https://pdos.csail.mit.edu/6.828/2009/readings/i386/s05_03.htm
[v86]: https://pdos.csail.mit.edu/6.828/2009/readings/i386/s15_02.htm
[msdos]: https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/section2/
[dos16m]: https://kguttag.com/wp-content/uploads/2023/07/1990-340-Family-THIRD-PARTY-GUIDE-4th-edition.pdf
[watcom]: https://open-watcom.github.io/open-watcom-1.9/pguide.html
[spice]: https://ptolemy.berkeley.edu/projects/embedded/pubs/downloads/spice/spice.html
[doom]: https://classicdoom.com/doominfo.htm
[doom8088]: https://github.com/FrenkelS/Doom8088
[renderer]: https://github.com/id-Software/DOOM/blob/master/linuxdoom-1.10/README.asm
