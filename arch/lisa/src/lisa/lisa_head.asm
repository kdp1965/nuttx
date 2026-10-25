;**************************************************************************
; arch/lisa/src/lisa/lisa_head.asm
;
; SPDX-License-Identifier: Apache-2.0
;
; Licensed to the Apache Software Foundation (ASF) under one or more
; contributor license agreements.  See the NOTICE file distributed with
; this work for additional information regarding copyright ownership.  The
; ASF licenses this file to you under the Apache License, Version 2.0 (the
; "License"); you may not use this file except in compliance with the
; License.  You may obtain a copy of the License at
;
;   http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
; WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
; License for the specific language governing permissions and limitations
; under the License.
;
;**************************************************************************

	.title	NuttX for LISA
	.module	lisa_head

;**************************************************************************
; Constants
;**************************************************************************

	; The register save area (arch/lisa/include/irq.h XCPT_*): the frame
	; the interrupt entry below pushes, from the last byte pushed up.  The
	; third chip (ttsky26d REV3.md): SP, RA, IX and the return address are
	; 24 bits, three bytes each; a return address carries its code bank.
	XCPT_SP_LO	== 0
	XCPT_SP_MID	== 1
	XCPT_SP_HI	== 2
	XCPT_IE		== 3
	XCPT_PC_HI	== 4
	XCPT_PC_MID	== 5
	XCPT_PC_LO	== 6
	XCPT_FLAGS	== 7
	XCPT_CSAVE	== 8
	XCPT_IRQ	== 9
	XCPT_RA_HI	== 10
	XCPT_RA_MID	== 11
	XCPT_RA_LO	== 12
	XCPT_IX_HI	== 13
	XCPT_IX_MID	== 14
	XCPT_IX_LO	== 15
	XCPT_A		== 16
	XCPT_SIZE	== 17

	; CONFIG_STACK_END, CONFIG_STACK_BASE from the Makefile
	.include	"asm_mem.h"

;**************************************************************************
; Global symbols
;**************************************************************************

	.globl	_nx_start		; OS entry point
	.globl	_lisa_doirq		; Interrupt decoding logic
	.globl	_lisa_restore		; Resume a saved context (lisa_context.asm)
	.globl	_g_lisa_ie		; The interrupt enable state

; The link order of the areas: the head is the first object, so its order
; is the layout.  Code: the vectors first (HOME at 0), then the common code,
; the constants and the initializers (all below word 0x4000, where every
; code bank sees them); the banks BANKn are placed by the link script.
; Data: from 0, DATA (the direct 512 bytes) first, the heap after the last
; area, the idle thread's stack at the top of RAM - a flat 8 MB on the
; third chip.

	.area	HOME (CODE)
	.area	GSINIT (CODE)
	; sdcc's static initializations: the stores of a function's static
	; with an initializer (sdcc emits those as code, not as INITIALIZER
	; bytes) from every module, run by the reset; GSFINAL returns there
	; by a jump - an initialization may call a function, so RA is not kept
_lisa_gsinit::
	.area	GSFINAL (CODE)
	ldxf	#_lisa_gsinit_done
	jmp	ix
	.area	CODE (CODE)
	.area	CONST (CODE,KDATA)		; packed constants (sdcc --pack-const): this
	.area	INITIALIZER (CODE,KDATA)	; module is linked first, so its flags are
	.area	FINITIALIZER (CODE,KDATA)	; the areas' - the linker reads them as bytes,
	.area	CABS (ABS,CODE,KDATA)		; lda (ix++) from the pointer's own bank
	.area	DATA
	.area	OSEG (OVR,DATA)
	.area	INITIALIZED
	.area	FDATA
	.area	FINITIALIZED
	.area	SSEG
	.area	HEAP
	.area	DABS (ABS)
	.area	RSEG (ABS)

;**************************************************************************
; The vectors: word 0 the reset, words 1..8 the eight interrupt sources
; (the lowest bit of INT_STATUS the highest priority), word 9 a request that
; is not exactly one source.  Each slot is one jal; the core writes no RA
; for the first jump after an interrupt, so the handler finds the
; interrupted RA intact.
;**************************************************************************

	.area	HOME (CODE)

	jal	_lisa_reset		; 0: reset
	jal	_lisa_irq0		; 1: timer 1
	jal	_lisa_irq1		; 2: timer 2
	jal	_lisa_irq2		; 3: UART 1
	jal	_lisa_irq3		; 4: UART 2
	jal	_lisa_irq4		; 5: port A
	jal	_lisa_irq5		; 6: TTLC
	jal	_lisa_irq6		; 7: I2C
	jal	_lisa_irq7		; 8: source 7
	jal	_lisa_irq8		; 9: several sources at once

_lisa_irq0:
	push	a
	ldi	#0
	br	_lisa_irqcommon
_lisa_irq1:
	push	a
	ldi	#1
	br	_lisa_irqcommon
_lisa_irq2:
	push	a
	ldi	#2
	br	_lisa_irqcommon
_lisa_irq3:
	push	a
	ldi	#3
	br	_lisa_irqcommon
_lisa_irq4:
	push	a
	ldi	#4
	br	_lisa_irqcommon
_lisa_irq5:
	push	a
	ldi	#5
	br	_lisa_irqcommon
_lisa_irq6:
	push	a
	ldi	#6
	br	_lisa_irqcommon
_lisa_irq7:
	push	a
	ldi	#7
	br	_lisa_irqcommon
_lisa_irq8:
	push	a
	ldi	#8
	br	_lisa_irqcommon

;**************************************************************************
; Reset: the stack at the top of RAM, the arithmetic mode the compiler
; assumes, the data areas cleared and initialized the way sdcc's own
; startup does it (the initial values are bytes packed two to a code word,
; read with lda (ix++) from the bank the pointer names), GSINIT run (until
; 2026-10-25 it was not: a function's initialized static, which sdcc
; initializes there, started as zeros), then nx_start
; through its 24-bit pointer, which carries its code bank.
; The frame of the loops: 0..2(sp) a destination pointer, 3..4(sp) a count.
;**************************************************************************

_lisa_reset:
	ldxf	#CONFIG_STACK_END
	xchg	sp
	amode	1
	ads	#-5
	; Zero DATA: IX = s_DATA (near), the count l_DATA counting down
	ldi	#>l_DATA
	stax	4(sp)
	ldi	#<l_DATA
	stax	3(sp)
	ldx	#s_DATA
1$:
	ldax	3(sp)
	or	4(sp)
	bz	2$
	ldi	#0
	stax	0(ix)
	adx	#1
	dcx	3(sp)
	if	c
	dcx	4(sp)
	br	1$
2$:
	; Zero FDATA the same way (anywhere in the 8 MB: ldxf)
	ldi	#>l_FDATA
	stax	4(sp)
	ldi	#<l_FDATA
	stax	3(sp)
	ldxf	#s_FDATA
3$:
	ldax	3(sp)
	or	4(sp)
	bz	4$
	ldi	#0
	stax	0(ix)
	adx	#1
	dcx	3(sp)
	if	c
	dcx	4(sp)
	br	3$
4$:
	; Copy INITIALIZER -> INITIALIZED: the source in IX (lda (ix++) reads
	; a byte of its bank), the destination pointer at 0..2(sp), swapped
	; through IX, the count at 3..4(sp)
	ldxf	#s_INITIALIZED
	stxx	0(sp)
	ldi	#>l_INITIALIZED
	stax	4(sp)
	ldi	#<l_INITIALIZED
	stax	3(sp)
	ldxf	#s_INITIALIZER
5$:
	ldax	3(sp)
	or	4(sp)
	bz	6$
	lda	(ix++)
	push	ix			; the slots are 3 deeper
	ldxx	3(sp)
	stax	0(ix)
	adx	#1
	stxx	3(sp)
	pop	ix
	dcx	3(sp)
	if	c
	dcx	4(sp)
	br	5$
6$:
	; The same for FINITIALIZER -> FINITIALIZED
	ldxf	#s_FINITIALIZED
	stxx	0(sp)
	ldi	#>l_FINITIALIZED
	stax	4(sp)
	ldi	#<l_FINITIALIZED
	stax	3(sp)
	ldxf	#s_FINITIALIZER
7$:
	ldax	3(sp)
	or	4(sp)
	bz	8$
	lda	(ix++)
	push	ix
	ldxx	3(sp)
	stax	0(ix)
	adx	#1
	stxx	3(sp)
	pop	ix
	dcx	3(sp)
	if	c
	dcx	4(sp)
	br	7$
8$:
	ads	#5
	; GSINIT (above), then on here
	ldxf	#_lisa_gsinit
	jmp	ix
_lisa_gsinit_done::
	; nx_start, in whatever code bank the link put it: call ix switches
	ldxf	#_nx_start
	call	ix
_lisa_halt::
	br	_lisa_halt

;**************************************************************************
; The common interrupt entry: A = the vector, 0(sp) = the interrupted A.
; Saves what the hardware does not - IX, RA, the cflag_save shadow - plus
; the ISR flag shadows (restored by rets), the return address (IA, with the
; interrupted code bank) and the interrupt enable state, computes the
; interrupted SP, and hands the frame to lisa_doirq(irq, regs), which
; returns the frame to resume (another task's after a context switch).
;**************************************************************************

_lisa_irqcommon:
	push	ix			; XCPT_IX
	sra				; XCPT_RA
	push	a			; XCPT_IRQ
	restc				; the cflag_save shadow into C ..
	ldac	c			; .. into A
	push	a			; XCPT_CSAVE
	ldirq
	push	a			; XCPT_FLAGS
	xchg	ia
	push	ix			; XCPT_PC: the return address, its bank with it
	lda	_g_lisa_ie
	push	a			; XCPT_IE (1: an interrupt needs them enabled)
	ldi	#0
	sta	_g_lisa_ie
	spix
	adx	#14			; the interrupted SP = this SP + 14
	txah
	andi	#0x7f			; without the condition bit spix leaves in IX
	push	a			; XCPT_SP_HI
	txau
	push	a			; XCPT_SP_MID
	txa
	push	a			; XCPT_SP_LO
	spix				; IX = the frame
	txah
	andi	#0x7f			; a data pointer: bit 23 would mean code space
	push	a
	txau
	push	a
	txa
	push	a			; the regs argument (low byte first)
	ldax	12(sp)			; A = the irq (XCPT_IRQ + 3)
	jal	_lisa_doirq		; IX = the frame to resume
	ads	#3
	jal	_lisa_restore

	.area	HEAP
