# NuttX on LISA (the Arty A7, lisa_sim, the chip testbench)

The LISA port runs Apache NuttX on Ken Pettit's LISA 8-bit microcontroller:
on branch `lisa` the second chip (`ttsky26d-um-lisa-ttlc` main, `lisa_sim
--rev2`), the proof of its code banks (RESPIN item 22: 1M words of code in
64 banks of 16K words behind a 16K-word window) and of its task banks on the
24K data map (RESPIN 28); on branch `twenty-four-bit` the third chip
(`ttsky26d-um-lisa-ttlc` twenty-four-bit, `REV3.md`, `lisa_sim --rev3`):
24-bit IX, SP and return addresses, a flat 8 MB of data, no banking - the
flat kernel described at the end.  The RTL runs on the Digilent Arty A7 at
50 MHz.  The sections up to the UART interrupts describe the second chip's
port; the third chip's differs where the last section says.

## Toolchain

`sdcc -mlisa` from `lisa-tools/sdcc-lisa` (branch `lisa`, built in place,
`README-lisa.md`), in the huge model: `--rev2 --model-huge`.  Every function
is a banked call through `___sdcc_bcall` unless `__nonbanked`, all data is
far (through IX) unless `__data`, function pointers are 3 bytes (the window
address and the bank).  `arch/lisa/src/tools/lisa-sdcc` is the compiler
wrapper: it adds `--codeseg BANKn` for the bank of each source from the
board's `scripts/banks.map`, a per-file map that `tools/banks_pack.py`
generates from the object sizes of a build (first fit into 32 KB banks).
`tools/lisa-ar` writes the libraries as the lists of object files
`sdldlisa` takes (there is no sdar), with absolute paths.

The common area below word 0x4000 holds the vectors, the trampoline, the
interrupt entry and the context switch, the constants and the initializers
(code-space data is two words per byte on LISA) and `arch/`: 16 KB of the
32 KB in the first build.

## The port

* `arch/lisa/include/irq.h`: the 16-byte register frame on the task's stack
  (SP, IE, SB, DB, CB, PC with the RA condition bit, the flag shadows, IRQ,
  RA, IX, A); `xcp.regs` points at it while the task is not running.
* `arch/lisa/src/lisa/lisa_head.asm`: the vectors at code address 0, the
  reset (sdcc's startup: zero DATA/FDATA, copy the initializers, then
  `nx_start` in its bank), the interrupt entry (saves the frame, calls
  `lisa_doirq(irq, regs)`, resumes the frame it returns).
* `lisa_context.asm`: `lisa_restore` (an interrupt-captured frame returns
  through IA with `rets`, a function-boundary frame through RA with `ret`),
  `lisa_switchcontext` (a voluntary switch), `up_saveusercontext`,
  `up_getsp`, `lisa_heapbase`.  Data pointers handed to C must not carry
  IX's condition bit: bit 15 of a pointer marks code space.
* The interrupt enable bit cannot be read back: `g_lisa_ie` shadows it
  (`up_irq_save` / `up_irq_restore` / `up_irq_enable` in `lisa_irq.c`).
  `up_interrupt_context()` is a macro on `g_lisa_inirq`.  Vector 9 (several
  sources pending) is served by scanning INT_STATUS.
* The data and stack banks stay 1 (the flat 32K); every frame saves all
  three bank registers, so a task could own a stack bank once the kernel
  reaches other tasks' stacks only SP-relative (it does not today: TLS,
  argument copies).
* Board `arty`: UART1 as the console (ack INT_STATUS first, then drain the
  8-byte FIFO; the TX request is an edge, so `txint` feeds the transmitter
  when it is already empty), timer 1 as the system tick (a 1 ms unit from
  the pre-divider, `CONFIG_USEC_PER_TICK` / 1000 as the preload).

## Building and running

    cd nuttx
    ./tools/configure.sh -a ../nuttx-apps arty:nsh
    make -j8                      # nuttx.ihx (+ nuttx.map, nuttx.cdb)
    ../lisa-tools/lisa_sim/lisa_sim --rev2 -b -c 50000000 nuttx.ihx

After a change in the sizes, regenerate the bank map and rebuild the
objects (the map is not a make dependency):

    arch/lisa/src/tools/banks_pack.py > boards/lisa/lisa/arty/scripts/banks.map
    find . ../nuttx-apps -name '*.rel' -delete; rm -f staging/*.lib; make -j8

The link warns about symbols both NuttX's libc and sdcc's library define
(memcpy, strlen, ...): NuttX's win, the warnings are harmless.

## The UART interrupts are levels

`lisa_periph.v` (the second chip since RESPIN 25, 2026-10-18) raises the UART1
request while a byte waits and the receive interrupt is enabled, or while the
transmitter is empty and its interrupt is enabled; the request ends when the
FIFO is drained, the next byte is written or the transmit enable is dropped,
and writing INT_STATUS clears nothing there (the timer's bit is latched and
needs the write).  The driver still drains the FIFO when it enables the
receive interrupt and feeds the transmitter when it enables the transmit one,
which is what the level does in hardware, so it also runs on an RTL that
latches the edges (the RTL before 2026-10-18, which in fact could not write
the enables at all).  `lisa_sim --rev2` models the levels.

## The third chip: the flat kernel (branch `twenty-four-bit`)

`CONFIG_LISA_REV3` (default y on this branch; `LISA_TASK_BANKS` is the
second chip's and depends on its absence) builds with `sdcc -mlisa --rev3
--model-huge` against `lib/lisa-rev3`: pointers are 3 bytes (a `__near` one
2, the first 64K), a call to another bank is `ldxf #f; call ix` (no
trampoline, no bank bytes in the frame), `jal` within a translation unit or
to a `__nonbanked` function.  The register frame (`irq.h`, 17 bytes) holds
SP, PC, RA and IX as three bytes each, the code bank in the top byte of a
return address: `push ix` / `sra` put that byte at the lowest address, so
`XCPT_PC_HI` is offset 4 and `XCPT_PC_LO` 6, and `lisa_switchcontext`
pushes the PC bytes low byte first to lay its frame out the same way.
`ret` / `rets` restore the bank from the address: no bank register is saved.
The reset clears and copies the data areas with 24-bit pointers (`ldxf`,
`lda (ix++)` from the initializer's own bank) and enters `nx_start`
through its pointer.

The data space is flat: `CONFIG_RAM_SIZE` 8388352, the idle thread's stack
at the top, one kernel heap from the end of the data areas
(`lisa_heapbase`) to it.  `size_t` stays 16 bits, so the heap is made of
regions of 0xfff0 bytes: `up_allocate_heap` hands `nx_start` the first,
`lisa_addregions` (from `up_initialize`, once the heap exists) adds the
rest with `kmm_addregion` - `CONFIG_MM_REGIONS` 130 covers the 8 MB - and
the heap's totals are `long` (`ARCH_WIDE_HEAP`: `mm_heap_s`'s sizes,
`struct mallinfo`).  `intptr_t` / `uintptr_t` are `long` on this chip
(`arch/lisa/include/types.h` defines `_intptr_t` and `ARCH_HAVE_INTPTR_T`,
which `include/stdint.h` takes): the memory manager's alignment arithmetic
and the stack-top computations need the whole address.  `vsprintf` reads a
`%p` argument as a pointer when its size is neither `int`'s nor `long`'s.

    make -j8
    ../lisa-tools/lisa_sim/lisa_sim --rev3 -b -c 6000000 nuttx.ihx   # the banner and the prompt
    # interactive: a pty pair (socat) with lisa_sim --rev3 -p on one end

`free` reports 8385208 bytes in 129 regions; `bigtest 1 30000 &` tasks get
their stacks and buffers in the regions above 64K.  After a compiler change
delete the objects that `make clean` keeps (`find libs/libc/bin mm/bin
-name '*.rel' -delete`): make rebuilds them only when their source is
newer, and an object compiled with an older frame layout misreads its
arguments.
