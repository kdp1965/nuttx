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
	; the interrupt entry below pushes, from the last byte pushed up.

	XCPT_SP_LO	== 0
	XCPT_SP_HI	== 1
	XCPT_IE		== 2
	XCPT_SB		== 3
	XCPT_DB		== 4
	XCPT_CB		== 5
	XCPT_PC_HI	== 6
	XCPT_PC_LO	== 7
	XCPT_FLAGS	== 8
	XCPT_CSAVE	== 9
	XCPT_IRQ	== 10
	XCPT_RA_HI	== 11
	XCPT_RA_LO	== 12
	XCPT_IX_HI	== 13
	XCPT_IX_LO	== 14
	XCPT_A		== 15
	XCPT_SIZE	== 16

	; The bank registers (direct peripheral addresses)

	LISA_CB		== 0x215
	LISA_DB		== 0x216
	LISA_SB		== 0x217

	; CONFIG_STACK_END, CONFIG_STACK_BASE from the Makefile

	.include	"asm_mem.h"

;**************************************************************************
; Global symbols
;**************************************************************************

	.globl	_nx_start		; OS entry point
	.globl	b_nx_start		; .. and its code bank
	.globl	_lisa_doirq		; Interrupt decoding logic
	.globl	_lisa_restore		; Resume a saved context (lisa_context.asm)
	.globl	_g_lisa_ie		; The interrupt enable state

;**************************************************************************
; The link order of the areas: the head is the first object, so its order
; is the layout.  Code: the vectors first (HOME at 0), then the common code,
; the constants and the initializers (all below word 0x4000, where every
; code bank sees them); the banks BANKn are placed by the link script.
; Data: from 0, DATA (the direct 512 bytes) first, the heap after the last
; area, the idle thread's stack at the top of RAM.
;**************************************************************************

	.area	HOME (CODE)
	.area	GSINIT (CODE)
	.area	GSFINAL (CODE)
	.area	CODE (CODE)
	.area	CONST (CODE,CDATA)
	.area	INITIALIZER (CODE,CDATA)
	.area	FINITIALIZER (CODE,CDATA)
	.area	CABS (ABS,CODE,CDATA)

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
; startup does it (the initial values are ldi/ret pairs in code space, one
; pair per byte, read with call ix), then nx_start in its code bank.
;**************************************************************************

_lisa_reset:
	ldx	#CONFIG_STACK_END
	xchg	sp
	amode	1
	ldi	#1			; the banks as after a hardware reset (a jump
	sta	LISA_CB			; to 0 from running code keeps them)
	sta	LISA_DB
	sta	LISA_SB

	; Zero DATA: IX = s_DATA, the count l_DATA in the two bytes on top
	; of the stack, counting down

	ldi	#>l_DATA
	push	a
	ldi	#<l_DATA
	push	a
	ldx	#s_DATA
1$:
	ldax	0(sp)
	or	1(sp)
	bz	2$
	ldi	#0
	stax	0(ix)
	adx	#1
	dcx	0(sp)
	if	c
	dcx	1(sp)
	br	1$
2$:
	; Zero FDATA the same way

	ldi	#>l_FDATA
	stax	1(sp)
	ldi	#<l_FDATA
	stax	0(sp)
	ldx	#s_FDATA
3$:
	ldax	0(sp)
	or	1(sp)
	bz	4$
	ldi	#0
	stax	0(ix)
	adx	#1
	dcx	0(sp)
	if	c
	dcx	1(sp)
	br	3$
4$:
	; Copy INITIALIZER -> INITIALIZED: the source in IX (code space, call
	; ix reads a byte), the destination in 1(sp):0(sp), the count in
	; 3(sp):2(sp)

	ldi	#>s_INITIALIZED
	push	a
	ldi	#<s_INITIALIZED
	push	a
	ldi	#>l_INITIALIZED
	stax	3(sp)
	ldi	#<l_INITIALIZED
	stax	2(sp)
	ldx	#s_INITIALIZER
5$:
	ldax	2(sp)
	or	3(sp)
	bz	6$
	call	ix
	adx	#1
	push	ix
	ldxx	2(sp)
	stax	0(ix)
	inx	2(sp)
	if	c
	inx	3(sp)
	pop	ix
	dcx	2(sp)
	if	c
	dcx	3(sp)
	br	5$
6$:
	; The same for FINITIALIZER -> FINITIALIZED

	ldi	#>s_FINITIALIZED
	stax	1(sp)
	ldi	#<s_FINITIALIZED
	stax	0(sp)
	ldi	#>l_FINITIALIZED
	stax	3(sp)
	ldi	#<l_FINITIALIZED
	stax	2(sp)
	ldx	#s_FINITIALIZER
7$:
	ldax	2(sp)
	or	3(sp)
	bz	8$
	call	ix
	adx	#1
	push	ix
	ldxx	2(sp)
	stax	0(ix)
	inx	2(sp)
	if	c
	inx	3(sp)
	pop	ix
	dcx	2(sp)
	if	c
	dcx	3(sp)
	br	7$
8$:
	ads	#4

	; nx_start, a banked function entered directly: its code bank first
	; (a write to CB takes effect at the next fetch from the window)

	ldi	#b_nx_start
	sta	LISA_CB
	jal	_nx_start

_lisa_halt::
	br	_lisa_halt

;**************************************************************************
; The common interrupt entry: A = the vector, 0(sp) = the interrupted A.
; Saves what the hardware does not - IX, RA, the cflag_save shadow - plus
; the ISR flag shadows (restored by rets), the return address (IA), the
; three bank registers and the interrupt enable state, computes the
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
	push	ix			; XCPT_PC: the return address
	lda	LISA_CB
	push	a			; XCPT_CB
	lda	LISA_DB
	push	a			; XCPT_DB
	lda	LISA_SB
	push	a			; XCPT_SB
	lda	_g_lisa_ie
	push	a			; XCPT_IE (1: an interrupt needs them enabled)
	ldi	#0
	sta	_g_lisa_ie
	spix				; the interrupted SP = this SP + 14
	txa
	ldc	#0
	adc	#14
	push	a
	txau
	adc	#0
	andi	#0x7f			; without the condition bit spix leaves in IX
	swap	0(sp)
	push	a			; XCPT_SP
	spix				; IX = the frame
	txau
	andi	#0x7f			; a data pointer: bit 15 would mean code space
	push	a
	txa
	push	a			; the regs argument (low byte first)
	ldax	12(sp)			; A = the irq (XCPT_IRQ + 2)
	jal	_lisa_doirq		; IX = the frame to resume
	ads	#2
	jal	_lisa_restore

	.area	HEAP
