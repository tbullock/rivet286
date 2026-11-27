# Rivet/286 to-do list

Rivet grew out of PMTEST, a DOS hosted operating system for testing the IA-16 compiler suite.

## Extender

- [ ] Extend the loader to support code in extended memory. Use the PMTEST loader as a reference.
- [ ] Let applications allocate more than 512 KiB on machines with 12 to 16 MiB of RAM.
- [ ] Detect the 80287 at startup. Use hardware floating point when available and software otherwise.
- [ ] Save and restore the 80287 registers across DOS calls.
- [ ] Keep the DOS clock running during long calculations.
- [ ] Extend the debugger to show C variables and step through far calls.

## Applications and tools

- [ ] Add a C example that builds with the IA-16 SDK and runs under DOS.
- [ ] Run ScottFree on Rivet.
- [ ] Run Berkeley SPICE3f5 with software and hardware floating point.
- [ ] Develop a SPICE test suite for Rivet.
- [ ] Build the Rivet kernel with IA-16 Clang and LLD.

## Tests

- [ ] Run the demo automatically on emulated 286 and 386 machines.
- [ ] Port the PMTEST loader, memory allocation and DOS service tests for all five memory models.
- [ ] Port the protection and task switching tests to Rivet and run them in 86Box and PCjs.
