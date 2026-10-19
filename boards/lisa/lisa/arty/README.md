# NuttX on LISA (the second chip, on the Arty A7 and on lisa_sim)

The LISA port runs Apache NuttX on Ken Pettit's LISA 8-bit microcontroller,
the second chip (`ttsky26d-um-lisa-ttlc`): its RTL on the Digilent Arty A7
at 50 MHz, or `lisa_sim --rev2`.  It is the proof of the chip's code banks
(RESPIN item 22): 1M words of code in 64 banks of 16K words behind a
16K-word window, 32K of data.

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

## The UART interrupts are edges

`lisa_periph.v` raises the UART1 request on the rising edge of "receive data
available" and of "transmit buffer empty" while the matching enable in
`LISA_UART_INT_EN` is set, not as a level.  The driver therefore drains the
FIFO itself when it enables the receive interrupt (`lisa_rxint`) and feeds
the transmitter when it enables the transmit one (`lisa_txint`); its handler
acknowledges `INT_STATUS` before it reads, so a byte arriving during the
drain raises the next request.  `lisa_sim --rev2` models the edges since
2026-10-18 (it re-asserted a level before, which hid the first case).
