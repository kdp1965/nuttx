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

	; The register save area (arch/lisa/include/irq.h XCPT_*)

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

	LISA_CB		== 0x215
	LISA_DB		== 0x216
	LISA_SB		== 0x217

;**************************************************************************
; Global symbols
;**************************************************************************

	.globl	_lisa_restore
	.globl	_lisa_switchcontext
	.globl	_lisa_fullcontextrestore
	.globl	_up_saveusercontext
	.globl	_up_getsp
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
;   shadows are written back and rets restores the live flags and enables
;   interrupts in one step.  A frame with interrupts disabled was built at a
;   function boundary (lisa_switchcontext) where only RA, SP and the banks
;   matter: the return address goes to RA, with its condition bit set (ret
;   reloads the predicate from it), and ret takes it.
;
;   Everything is read IX-relative and staged below the frame SP-relative,
;   so the frame must be visible through both the data bank and the stack
;   bank it names: the port keeps DB = SB = 1 (the flat 32K), see
;   arch/lisa/src/common/lisa_initialstate.c.
;
;**************************************************************************

_lisa_restore:
	di
	ldax	XCPT_IE(ix)
	sta	_g_lisa_ie
	ldax	XCPT_CB(ix)
	sta	LISA_CB			; this code is below word 0x4000
	ldax	XCPT_DB(ix)
	sta	LISA_DB
	ldax	XCPT_SB(ix)
	sta	LISA_SB			; the next instruction moves SP
	xchg	sp			; SP = the frame
	spix				; IX = the frame again
	ldax	XCPT_FLAGS(ix)
	stirq
	ldax	XCPT_CSAVE(ix)
	shr				; bit 0 into C ..
	savec				; .. into the cflag_save shadow
	ldax	XCPT_RA_LO(ix)
	push	a
	ldax	XCPT_RA_HI(ix)
	push	a
	lra				; RA (the high byte on top)
	ldax	XCPT_PC_HI(ix)
	push	a
	ldax	XCPT_PC_LO(ix)
	push	a			; the return address, low byte first for ldxx
	ldax	XCPT_IX_HI(ix)
	push	a
	ldax	XCPT_IX_LO(ix)
	push	a			; IX, the same
	ldax	XCPT_A(ix)
	push	a			; A
	ldax	XCPT_IE(ix)
	cpi	#0
	bz	1$

	ldxx	3(sp)
	xchg	ia			; IA = the return address
	ldxx	1(sp)			; IX
	pop	a			; A
	ads	#4 + XCPT_SIZE		; SP above the frame
	rets				; the flags from the shadows, interrupts on
1$:
	ldxx	3(sp)
	xchg	ra			; RA = the return address
	ldxx	1(sp)
	pop	a
	ads	#4 + XCPT_SIZE
	ret

;**************************************************************************
; Name: lisa_switchcontext
;
; Description:
;   void lisa_switchcontext(FAR chipreg_t **saveregs,
;                           FAR chipreg_t *restoreregs) __nonbanked
;
;   Save the calling task's context in a frame on its stack, store the
;   frame's address in *saveregs and resume the context restoreregs points
;   at.  Returns when the saved context is resumed, as a plain return to
;   the caller: A and IX are scratch (the caller saved what it needs).
;
;**************************************************************************

_lisa_switchcontext:
	push	a			; XCPT_A
	push	ix			; XCPT_IX
	sra				; XCPT_RA: the resume address
	ldi	#0
	push	a			; XCPT_IRQ
	push	a			; XCPT_CSAVE
	push	a			; XCPT_FLAGS
	ldax	4(sp)
	push	a			; XCPT_PC_LO = RA low
	ldax	4(sp)
	push	a			; XCPT_PC_HI = RA high, the condition bit with it
	lda	LISA_CB
	push	a			; XCPT_CB
	lda	LISA_DB
	push	a			; XCPT_DB
	lda	LISA_SB
	push	a			; XCPT_SB
	lda	_g_lisa_ie
	push	a			; XCPT_IE
	spix				; the SP to resume with = this SP + 14
	txa
	ldc	#0
	adc	#14
	push	a
	txau
	adc	#0
	andi	#0x7f			; without the condition bit spix leaves in IX
	swap	0(sp)
	push	a			; XCPT_SP
	spix				; IX = the frame ..
	txau
	andi	#0x7f
	taxu				; .. as a data pointer: bit 15 would mean code space
	push	ix			; staged: 0(sp) high, 1(sp) low
	ldxx	18(sp)			; IX = saveregs (the arguments above the frame)
	ldax	1(sp)
	stax	0(ix)
	ldax	0(sp)
	stax	1(ix)			; *saveregs = the frame
	ldxx	20(sp)			; IX = restoreregs
	jal	_lisa_restore

;**************************************************************************
; Name: lisa_fullcontextrestore
;
; Description:
;   void lisa_fullcontextrestore(FAR chipreg_t *regs) __nonbanked
;
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
;
;   Fill regs with the caller's context (the crash dump uses it: the
;   resume address is the return address, SP the caller's); returns 0.
;   Declared in nuttx/arch.h without __nonbanked, so called through
;   ___sdcc_bcall: the argument is 3 bytes deeper (the caller's RA and
;   bank under it) and the bank equate is exported.
;
;**************************************************************************

	b_up_saveusercontext == 1
	.globl	b_up_saveusercontext

_up_saveusercontext:
	sra				; 0(sp) RA high, 1(sp) RA low
	spix
	push	ix			; 0(sp) SP high, 1(sp) SP low: the entry SP - 2
	ldxx	7(sp)			; IX = regs (2 + 2 + the trampoline's 3)
	ldax	1(sp)
	ldc	#0
	adc	#2
	stax	XCPT_SP_LO(ix)
	ldax	0(sp)
	adc	#0
	andi	#0x7f			; without IX's condition bit
	stax	XCPT_SP_HI(ix)
	ldax	3(sp)
	stax	XCPT_RA_LO(ix)
	stax	XCPT_PC_LO(ix)
	ldax	2(sp)
	stax	XCPT_RA_HI(ix)
	stax	XCPT_PC_HI(ix)
	ldi	#0
	stax	XCPT_A(ix)
	stax	XCPT_IX_HI(ix)
	stax	XCPT_IX_LO(ix)
	stax	XCPT_FLAGS(ix)
	stax	XCPT_CSAVE(ix)
	stax	XCPT_IRQ(ix)
	lda	LISA_CB
	stax	XCPT_CB(ix)
	lda	LISA_DB
	stax	XCPT_DB(ix)
	lda	LISA_SB
	stax	XCPT_SB(ix)
	lda	_g_lisa_ie
	stax	XCPT_IE(ix)
	ads	#4
	ldi	#0
	ret

;**************************************************************************
; Name: up_getsp
;
; Description:
;   uintptr_t up_getsp(void) __nonbanked: the stack pointer, in IX
;
;**************************************************************************

_up_getsp:
	spix
	txau
	andi	#0x7f			; a data pointer: no condition bit
	taxu
	ret

;**************************************************************************
; Name: lisa_heapbase
;
; Description:
;   uintptr_t lisa_heapbase(void) __nonbanked: the first byte after the
;   data areas (the area HEAP of lisa_head.asm), in IX
;
;**************************************************************************

_lisa_heapbase:
	ldi	#<s_HEAP		; (ldx # would set IX's condition bit: bit 15 of the
	tax				;  value, which marks a code-space pointer)
	ldi	#>s_HEAP
	taxu
	ret
