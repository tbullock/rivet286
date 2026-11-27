# Rivet/286

## Source style

- Run `clang-format --style=file -i` on C source and header files you add or
  edit. The root `.clang-format` selects the default LLVM style. Before
  finishing, check those files with `clang-format --style=file --dry-run --Werror`.
  On macOS, `xcrun clang-format` runs the copy bundled with Xcode.
- Keep the `.c` includes in their current order in the kernel and path
  extension. Leave the `clang-format off` and `clang-format on` comments around
  those includes.
- Follow the NASM layout and C compatibility guidance in the
  [source style section of the README](README.md#source-style).
