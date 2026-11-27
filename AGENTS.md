# Rivet/286

## Source style

- Format new and edited C sources and headers with `clang-format --style=file`.
  The root `.clang-format` sets the default LLVM style. Check those files with
  `clang-format --style=file --dry-run --Werror` before finishing. On macOS,
  `xcrun clang-format` can use the formatter bundled with Xcode.
- Preserve the declaration order of implementation includes in the kernel
  and path extension. Keep the existing scoped `clang-format off` markers
  around those includes.
- Follow the NASM layout and C compatibility guidance in the
  [source style section of the README](README.md#source-style).
