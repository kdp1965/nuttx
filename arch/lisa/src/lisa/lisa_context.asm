;**************************************************************************
; arch/lisa/src/lisa/lisa_context.asm
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

	.title	NuttX for LISA: the context switch
	.module	lisa_context

;**************************************************************************
; Constants
;**************************************************************************

	; The register save area (arch/lisa/include/irq.h XCPT_*).  The third
	; chip (ttsky26d REV3.md): SP, RA, IX and the return address are 24
	; bits, so each takes three bytes - sra / lra and push ix / pop ix move
	; them with the top byte (the condition bit, the code bank of a return
	; address) at the lowest address, ldxx / stxx with the low byte there.
	; A return address carries its code bank: ret / rets switch to it, so
	; no bank register is saved.
	XCPT_SP_LO	== 0
	XCPT_SP_MID	== 1
	XCPT_SP_HI	== 2
	XCPT_IE		== 3
	XCPT_PC_HI	== 4
	XCPT_PC_MID	== 5
	XCPT_PC_LO	== 6
	XCPT_FLAGS	== 7
	XCPT_FLAGS_AMODE1 == 0x20		; the flags byte of a frame made without an interrupt
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

;**************************************************************************
; Global symbols
;**************************************************************************

	.globl	_lisa_restore
	.globl	_lisa_switchcontext
	.globl	_lisa_fullcontextrestore
	.globl	_up_saveusercontext
	.globl	_lisa_getsp
	.globl	_lisa_heapbase
	.globl	_g_lisa_ie

	.area	CODE (CODE)

;**************************************************************************
; Name: lisa_restore
;
; Description:
;   Resume the context in the frame IX points at.  The frame lies on the
;   resuming task's stack and the task continues with SP just above it.
;   A frame whose interrupts were enabled was captured by an interrupt (or
;   laid out by up_initial_state): the return address goes to IA, the flag
;   shadows are written back and rets restores the live flags, the code
;   bank and enables interrupts in one step.  A frame with interrupts
;   disabled was built at a function boundary (lisa_switchcontext) where
;   only RA and SP matter: the return address goes to RA, with its
;   condition bit set (ret reloads the predicate from it) and its bank,
;   and ret takes it.
;   The frame is read IX-relative and staged below it SP-relative.
;
;**************************************************************************

_lisa_restore:
	di
	ldax	XCPT_IE(ix)
	sta	_g_lisa_ie
	xchg	sp			; SP = the frame
	spix				; IX = the frame again
	ldax	XCPT_FLAGS(ix)
	stirq
	ldax	XCPT_CSAVE(ix)
	shr				; bit 0 into C ..
	savec				; .. into the cflag_save shadow
	ldax	XCPT_RA_LO(ix)
	push	a
	ldax	XCPT_RA_MID(ix)
	push	a
	ldax	XCPT_RA_HI(ix)
	push	a
	lra				; RA (the top byte on top, as lra takes it)
	ldax	XCPT_PC_HI(ix)
	push	a
	ldax	XCPT_PC_MID(ix)
	push	a
	ldax	XCPT_PC_LO(ix)
	push	a			; the return address, low byte first for ldxx
	ldax	XCPT_IX_HI(ix)
	push	a
	ldax	XCPT_IX_MID(ix)
	push	a
	ldax	XCPT_IX_LO(ix)
	push	a			; IX, the same
	ldax	XCPT_A(ix)
	push	a			; A
	ldax	XCPT_IE(ix)
	cpi	#0
	bz	1$
	ldxx	4(sp)
	xchg	ia			; IA = the return address, its bank with it
	ldxx	1(sp)			; IX
	pop	a			; A
	ads	#6 + XCPT_SIZE		; SP above the frame
	rets				; the flags from the shadows, the bank, interrupts on
1$:
	ldxx	4(sp)
	xchg	ra			; RA = the return address, its bank with it
	ldxx	1(sp)
	pop	a
	ads	#6 + XCPT_SIZE
	ret

;**************************************************************************
; Name: lisa_switchcontext
;
; Description:
;   void lisa_switchcontext(FAR chipreg_t **saveregs,
;                           FAR chipreg_t *restoreregs) __nonbanked
;   Save the calling task's context in a frame on its stack, store the
;   frame's address in *saveregs and resume the context restoreregs points
;   at.  Returns when the saved context is resumed, as a plain return to
;   the caller: A and IX are scratch (the caller saved what it needs).
;
;**************************************************************************

_lisa_switchcontext:
	push	a			; XCPT_A
	push	ix			; XCPT_IX (the top byte first)
	sra				; XCPT_RA: the resume address, its bank with it
	ldi	#0
	push	a			; XCPT_IRQ
	push	a			; XCPT_CSAVE
	ldi	#XCPT_FLAGS_AMODE1
	push	a			; XCPT_FLAGS: amode 1 (rets takes amode from it)
	ldax	5(sp)			; RA's low byte (0..2(sp) the flags, the irq and csave; 3..5 RA)
	push	a			; XCPT_PC_LO
	ldax	5(sp)			; the middle byte, one deeper now
	push	a			; XCPT_PC_MID
	ldax	5(sp)			; the top byte: the condition bit and the bank with it
	push	a			; XCPT_PC_HI - at the lowest address, as push ix leaves it in an interrupt frame
	lda	_g_lisa_ie
	push	a			; XCPT_IE
	spix
	adx	#14			; the SP to resume with = this SP + 14 (the frame's other bytes)
	txah
	andi	#0x7f			; without the condition bit spix leaves in IX
	push	a			; XCPT_SP_HI
	txau
	push	a			; XCPT_SP_MID
	txa
	push	a			; XCPT_SP_LO
	spix				; IX = the frame ..
	txah
	andi	#0x7f
	taxh				; .. as a data pointer: bit 23 would mean code space
	push	ix			; staged: 0(sp) the top byte, 1(sp) the middle, 2(sp) the low
	ldxx	20(sp)			; IX = saveregs (the arguments above the frame)
	ldax	2(sp)
	stax	0(ix)
	ldax	1(sp)
	stax	1(ix)
	ldax	0(sp)
	stax	2(ix)			; *saveregs = the frame
	ldxx	23(sp)			; IX = restoreregs
	jal	_lisa_restore

;**************************************************************************
; Name: lisa_fullcontextrestore
;
; Description:
;   void lisa_fullcontextrestore(FAR chipreg_t *regs) __nonbanked
;   Resume the context regs points at; does not return.
;
;**************************************************************************

_lisa_fullcontextrestore:
	ldxx	0(sp)
	jal	_lisa_restore

;**************************************************************************
; Name: up_saveusercontext
;
; Description:
;   int up_saveusercontext(FAR void *regs)
;   Fill regs with the caller's context (the crash dump uses it: the
;   resume address is the return address, SP the caller's); returns 0.
;   Declared in nuttx/arch.h without __nonbanked, so a caller in another
;   bank reaches it through its 24-bit pointer (ldxf; call ix); the frame
;   is the same either way.
;
;**************************************************************************

	b_up_saveusercontext == 1
	.globl	b_up_saveusercontext
_up_saveusercontext:
	sra				; 0..2(sp) RA, the top byte first
	spix
	push	ix			; 0..2(sp) SP: the entry SP - 3
	ldxx	6(sp)			; IX = regs (3 + 3)
	ldax	2(sp)
	ldc	#0
	adc	#3
	stax	XCPT_SP_LO(ix)
	ldax	1(sp)
	adc	#0
	stax	XCPT_SP_MID(ix)
	ldax	0(sp)
	adc	#0
	andi	#0x7f			; without IX's condition bit
	stax	XCPT_SP_HI(ix)
	ldax	5(sp)
	stax	XCPT_RA_LO(ix)
	stax	XCPT_PC_LO(ix)
	ldax	4(sp)
	stax	XCPT_RA_MID(ix)
	stax	XCPT_PC_MID(ix)
	ldax	3(sp)
	stax	XCPT_RA_HI(ix)
	stax	XCPT_PC_HI(ix)
	ldi	#0
	stax	XCPT_A(ix)
	stax	XCPT_IX_HI(ix)
	stax	XCPT_IX_MID(ix)
	stax	XCPT_IX_LO(ix)
	stax	XCPT_CSAVE(ix)
	stax	XCPT_IRQ(ix)
	ldi	#XCPT_FLAGS_AMODE1
	stax	XCPT_FLAGS(ix)		; amode 1 (rets takes amode from it)
	lda	_g_lisa_ie
	stax	XCPT_IE(ix)
	ads	#3
	lra
	ldi	#0
	tax
	taxu				; 0, an int, in IX
	ret

;**************************************************************************
; Name: lisa_getsp
;
; Description:
;   FAR void *lisa_getsp(void) __nonbanked: the stack pointer, in IX
;   (up_getsp() casts it)
;
;**************************************************************************

_lisa_getsp:
	spix
	txah
	andi	#0x7f			; a data pointer: no condition bit
	taxh
	ret

;**************************************************************************
; Name: lisa_heapbase
;
; Description:
;   FAR void *lisa_heapbase(void) __nonbanked: the first byte after the
;   data areas (the area HEAP of lisa_head.asm), in IX
;
;**************************************************************************

_lisa_heapbase:
	ldxf	#s_HEAP			; the linker's 24-bit data address (bit 23 clear)
	ret
