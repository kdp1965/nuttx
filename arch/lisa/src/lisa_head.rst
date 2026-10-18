                                      1 ;**************************************************************************
                                      2 ; arch/lisa/src/lisa/lisa_head.asm
                                      3 ;
                                      4 ; SPDX-License-Identifier: Apache-2.0
                                      5 ;
                                      6 ; Licensed to the Apache Software Foundation (ASF) under one or more
                                      7 ; contributor license agreements.  See the NOTICE file distributed with
                                      8 ; this work for additional information regarding copyright ownership.  The
                                      9 ; ASF licenses this file to you under the Apache License, Version 2.0 (the
                                     10 ; "License"); you may not use this file except in compliance with the
                                     11 ; License.  You may obtain a copy of the License at
                                     12 ;
                                     13 ;   http://www.apache.org/licenses/LICENSE-2.0
                                     14 ;
                                     15 ; Unless required by applicable law or agreed to in writing, software
                                     16 ; distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
                                     17 ; WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
                                     18 ; License for the specific language governing permissions and limitations
                                     19 ; under the License.
                                     20 ;
                                     21 ;**************************************************************************
                                     22 
                                     23 	.title	NuttX for LISA
                                     24 	.module	lisa_head
                                     25 
                                     26 ;**************************************************************************
                                     27 ; Constants
                                     28 ;**************************************************************************
                                     29 
                                     30 	; The register save area (arch/lisa/include/irq.h XCPT_*): the frame
                                     31 	; the interrupt entry below pushes, from the last byte pushed up.
                                     32 
                           000000    33 	XCPT_SP_LO	== 0
                           000001    34 	XCPT_SP_HI	== 1
                           000002    35 	XCPT_IE		== 2
                           000003    36 	XCPT_SB		== 3
                           000004    37 	XCPT_DB		== 4
                           000005    38 	XCPT_CB		== 5
                           000006    39 	XCPT_PC_HI	== 6
                           000007    40 	XCPT_PC_LO	== 7
                           000008    41 	XCPT_FLAGS	== 8
                           000009    42 	XCPT_CSAVE	== 9
                           00000A    43 	XCPT_IRQ	== 10
                           00000B    44 	XCPT_RA_HI	== 11
                           00000C    45 	XCPT_RA_LO	== 12
                           00000D    46 	XCPT_IX_HI	== 13
                           00000E    47 	XCPT_IX_LO	== 14
                           00000F    48 	XCPT_A		== 15
                           000010    49 	XCPT_SIZE	== 16
                                     50 
                                     51 	; The bank registers (direct peripheral addresses)
                                     52 
                           000215    53 	LISA_CB		== 0x215
                           000216    54 	LISA_DB		== 0x216
                           000217    55 	LISA_SB		== 0x217
                                     56 
                                     57 	; CONFIG_STACK_END, CONFIG_STACK_BASE from the Makefile
                                     58 
                                     59 	.include	"asm_mem.h"
                           007FFF     1 	CONFIG_STACK_END == (0x0000 + 32768 - 1)
                           007E00     2 	CONFIG_STACK_BASE == (0x0000 + 32768 - 512)
                                     60 
                                     61 ;**************************************************************************
                                     62 ; Global symbols
                                     63 ;**************************************************************************
                                     64 
                                     65 	.globl	_nx_start		; OS entry point
                                     66 	.globl	b_nx_start		; .. and its code bank
                                     67 	.globl	_lisa_doirq		; Interrupt decoding logic
                                     68 	.globl	_lisa_restore		; Resume a saved context (lisa_context.asm)
                                     69 	.globl	_g_lisa_ie		; The interrupt enable state
                                     70 
                                     71 ;**************************************************************************
                                     72 ; The link order of the areas: the head is the first object, so its order
                                     73 ; is the layout.  Code: the vectors first (HOME at 0), then the common code,
                                     74 ; the constants and the initializers (all below word 0x4000, where every
                                     75 ; code bank sees them); the banks BANKn are placed by the link script.
                                     76 ; Data: from 0, DATA (the direct 512 bytes) first, the heap after the last
                                     77 ; area, the idle thread's stack at the top of RAM.
                                     78 ;**************************************************************************
                                     79 
                                     80 	.area	HOME (CODE)
                                     81 	.area	GSINIT (CODE)
                                     82 	.area	GSFINAL (CODE)
                                     83 	.area	CODE (CODE)
                                     84 	.area	CONST (CODE,CDATA)
                                     85 	.area	INITIALIZER (CODE,CDATA)
                                     86 	.area	FINITIALIZER (CODE,CDATA)
                                     87 	.area	CABS (ABS,CODE,CDATA)
                                     88 
                                     89 	.area	DATA
                                     90 	.area	OSEG (OVR,DATA)
                                     91 	.area	INITIALIZED
                                     92 	.area	FDATA
                                     93 	.area	FINITIALIZED
                                     94 	.area	SSEG
                                     95 	.area	HEAP
                                     96 	.area	DABS (ABS)
                                     97 	.area	RSEG (ABS)
                                     98 
                                     99 ;**************************************************************************
                                    100 ; The vectors: word 0 the reset, words 1..8 the eight interrupt sources
                                    101 ; (the lowest bit of INT_STATUS the highest priority), word 9 a request that
                                    102 ; is not exactly one source.  Each slot is one jal; the core writes no RA
                                    103 ; for the first jump after an interrupt, so the handler finds the
                                    104 ; interrupted RA intact.
                                    105 ;**************************************************************************
                                    106 
                                    107 	.area	HOME (CODE)
                                    108 
      000000 25 00                  109 	jal	_lisa_reset		; 0: reset
      000002 0A 00                  110 	jal	_lisa_irq0		; 1: timer 1
      000004 0D 00                  111 	jal	_lisa_irq1		; 2: timer 2
      000006 10 00                  112 	jal	_lisa_irq2		; 3: UART 1
      000008 13 00                  113 	jal	_lisa_irq3		; 4: UART 2
      00000A 16 00                  114 	jal	_lisa_irq4		; 5: port A
      00000C 19 00                  115 	jal	_lisa_irq5		; 6: TTLC
      00000E 1C 00                  116 	jal	_lisa_irq6		; 7: I2C
      000010 1F 00                  117 	jal	_lisa_irq7		; 8: source 7
      000012 22 00                  118 	jal	_lisa_irq8		; 9: several sources at once
                                    119 
      000014                        120 _lisa_irq0:
      000014 80 A0                  121 	push	a
      000016 00 80                  122 	ldi	#0
      000018 7A B0                  123 	br	_lisa_irqcommon
      00001A                        124 _lisa_irq1:
      00001A 80 A0                  125 	push	a
      00001C 01 80                  126 	ldi	#1
      00001E 77 B0                  127 	br	_lisa_irqcommon
      000020                        128 _lisa_irq2:
      000020 80 A0                  129 	push	a
      000022 02 80                  130 	ldi	#2
      000024 74 B0                  131 	br	_lisa_irqcommon
      000026                        132 _lisa_irq3:
      000026 80 A0                  133 	push	a
      000028 03 80                  134 	ldi	#3
      00002A 71 B0                  135 	br	_lisa_irqcommon
      00002C                        136 _lisa_irq4:
      00002C 80 A0                  137 	push	a
      00002E 04 80                  138 	ldi	#4
      000030 6E B0                  139 	br	_lisa_irqcommon
      000032                        140 _lisa_irq5:
      000032 80 A0                  141 	push	a
      000034 05 80                  142 	ldi	#5
      000036 6B B0                  143 	br	_lisa_irqcommon
      000038                        144 _lisa_irq6:
      000038 80 A0                  145 	push	a
      00003A 06 80                  146 	ldi	#6
      00003C 68 B0                  147 	br	_lisa_irqcommon
      00003E                        148 _lisa_irq7:
      00003E 80 A0                  149 	push	a
      000040 07 80                  150 	ldi	#7
      000042 65 B0                  151 	br	_lisa_irqcommon
      000044                        152 _lisa_irq8:
      000044 80 A0                  153 	push	a
      000046 08 80                  154 	ldi	#8
      000048 62 B0                  155 	br	_lisa_irqcommon
                                    156 
                                    157 ;**************************************************************************
                                    158 ; Reset: the stack at the top of RAM, the arithmetic mode the compiler
                                    159 ; assumes, the data areas cleared and initialized the way sdcc's own
                                    160 ; startup does it (the initial values are ldi/ret pairs in code space, one
                                    161 ; pair per byte, read with call ix), then nx_start in its code bank.
                                    162 ;**************************************************************************
                                    163 
      00004A                        164 _lisa_reset:
      00004A 80 A1 FF 7F            165 	ldx	#CONFIG_STACK_END
      00004E C8 8A                  166 	xchg	sp
      000050 41 A1                  167 	amode	1
      000052 01 80                  168 	ldi	#1			; the banks as after a hardware reset (a jump
      000054 15 FE                  169 	sta	LISA_CB			; to 0 from running code keeps them)
      000056 16 FE                  170 	sta	LISA_DB
      000058 17 FE                  171 	sta	LISA_SB
                                    172 
                                    173 	; Zero DATA: IX = s_DATA, the count l_DATA in the two bytes on top
                                    174 	; of the stack, counting down
                                    175 
      00005A 00 80                  176 	ldi	#>l_DATA
      00005C 80 A0                  177 	push	a
      00005E 02 80                  178 	ldi	#<l_DATA
      000060 80 A0                  179 	push	a
      000062 80 A1 00 00            180 	ldx	#s_DATA
      000066                        181 1$:
      000066 00 F2                  182 	ldax	0(sp)
      000068 01 DA                  183 	or	1(sp)
      00006A 08 B8                  184 	bz	2$
      00006C 00 80                  185 	ldi	#0
      00006E 00 F8                  186 	stax	0(ix)
      000070 01 98                  187 	adx	#1
      000072 00 9E                  188 	dcx	0(sp)
      000074 03 A2                  189 	if	c
      000076 01 9E                  190 	dcx	1(sp)
      000078 F7 B7                  191 	br	1$
      00007A                        192 2$:
                                    193 	; Zero FDATA the same way
                                    194 
      00007A 01 80                  195 	ldi	#>l_FDATA
      00007C 01 FA                  196 	stax	1(sp)
      00007E BD 80                  197 	ldi	#<l_FDATA
      000080 00 FA                  198 	stax	0(sp)
      000082 80 A1 02 00            199 	ldx	#s_FDATA
      000086                        200 3$:
      000086 00 F2                  201 	ldax	0(sp)
      000088 01 DA                  202 	or	1(sp)
      00008A 08 B8                  203 	bz	4$
      00008C 00 80                  204 	ldi	#0
      00008E 00 F8                  205 	stax	0(ix)
      000090 01 98                  206 	adx	#1
      000092 00 9E                  207 	dcx	0(sp)
      000094 03 A2                  208 	if	c
      000096 01 9E                  209 	dcx	1(sp)
      000098 F7 B7                  210 	br	3$
      00009A                        211 4$:
                                    212 	; Copy INITIALIZER -> INITIALIZED: the source in IX (code space, call
                                    213 	; ix reads a byte), the destination in 1(sp):0(sp), the count in
                                    214 	; 3(sp):2(sp)
                                    215 
      00009A 00 80                  216 	ldi	#>s_INITIALIZED
      00009C 80 A0                  217 	push	a
      00009E 02 80                  218 	ldi	#<s_INITIALIZED
      0000A0 80 A0                  219 	push	a
      0000A2 00 80                  220 	ldi	#>l_INITIALIZED
      0000A4 03 FA                  221 	stax	3(sp)
      0000A6 00 80                  222 	ldi	#<l_INITIALIZED
      0000A8 02 FA                  223 	stax	2(sp)
      0000AA 80 A1 B0 B2            224 	ldx	#s_INITIALIZER
      0000AE                        225 5$:
      0000AE 02 F2                  226 	ldax	2(sp)
      0000B0 03 DA                  227 	or	3(sp)
      0000B2 0E B8                  228 	bz	6$
      0000B4 80 8A                  229 	call	ix
      0000B6 01 98                  230 	adx	#1
      0000B8 68 A1                  231 	push	ix
      0000BA 02 CC                  232 	ldxx	2(sp)
      0000BC 00 F8                  233 	stax	0(ix)
      0000BE 02 E6                  234 	inx	2(sp)
      0000C0 03 A2                  235 	if	c
      0000C2 03 E6                  236 	inx	3(sp)
      0000C4 6C A1                  237 	pop	ix
      0000C6 02 9E                  238 	dcx	2(sp)
      0000C8 03 A2                  239 	if	c
      0000CA 03 9E                  240 	dcx	3(sp)
      0000CC F1 B7                  241 	br	5$
      0000CE                        242 6$:
                                    243 	; The same for FINITIALIZER -> FINITIALIZED
                                    244 
      0000CE 01 80                  245 	ldi	#>s_FINITIALIZED
      0000D0 01 FA                  246 	stax	1(sp)
      0000D2 BF 80                  247 	ldi	#<s_FINITIALIZED
      0000D4 00 FA                  248 	stax	0(sp)
      0000D6 03 80                  249 	ldi	#>l_FINITIALIZED
      0000D8 03 FA                  250 	stax	3(sp)
      0000DA 1F 80                  251 	ldi	#<l_FINITIALIZED
      0000DC 02 FA                  252 	stax	2(sp)
      0000DE 80 A1 B0 B2            253 	ldx	#s_FINITIALIZER
      0000E2                        254 7$:
      0000E2 02 F2                  255 	ldax	2(sp)
      0000E4 03 DA                  256 	or	3(sp)
      0000E6 0E B8                  257 	bz	8$
      0000E8 80 8A                  258 	call	ix
      0000EA 01 98                  259 	adx	#1
      0000EC 68 A1                  260 	push	ix
      0000EE 02 CC                  261 	ldxx	2(sp)
      0000F0 00 F8                  262 	stax	0(ix)
      0000F2 02 E6                  263 	inx	2(sp)
      0000F4 03 A2                  264 	if	c
      0000F6 03 E6                  265 	inx	3(sp)
      0000F8 6C A1                  266 	pop	ix
      0000FA 02 9E                  267 	dcx	2(sp)
      0000FC 03 A2                  268 	if	c
      0000FE 03 9E                  269 	dcx	3(sp)
      000100 F1 B7                  270 	br	7$
      000102                        271 8$:
      000102 04 94                  272 	ads	#4
                                    273 
                                    274 	; nx_start, a banked function entered directly: its code bank first
                                    275 	; (a write to CB takes effect at the next fetch from the window)
                                    276 
      000104 07 80                  277 	ldi	#b_nx_start
      000106 15 FE                  278 	sta	LISA_CB
      000108 6A 42                  279 	jal	_nx_start
                                    280 
      00010A                        281 _lisa_halt::
      00010A 00 B0                  282 	br	_lisa_halt
                                    283 
                                    284 ;**************************************************************************
                                    285 ; The common interrupt entry: A = the vector, 0(sp) = the interrupted A.
                                    286 ; Saves what the hardware does not - IX, RA, the cflag_save shadow - plus
                                    287 ; the ISR flag shadows (restored by rets), the return address (IA), the
                                    288 ; three bank registers and the interrupt enable state, computes the
                                    289 ; interrupted SP, and hands the frame to lisa_doirq(irq, regs), which
                                    290 ; returns the frame to resume (another task's after a context switch).
                                    291 ;**************************************************************************
                                    292 
      00010C                        293 _lisa_irqcommon:
      00010C 68 A1                  294 	push	ix			; XCPT_IX
      00010E 60 A1                  295 	sra				; XCPT_RA
      000110 80 A0                  296 	push	a			; XCPT_IRQ
      000112 7C A1                  297 	restc				; the cflag_save shadow into C ..
      000114 C3 A1                  298 	ldac	c			; .. into A
      000116 80 A0                  299 	push	a			; XCPT_CSAVE
      000118 20 A1                  300 	ldirq
      00011A 80 A0                  301 	push	a			; XCPT_FLAGS
      00011C C4 8A                  302 	xchg	ia
      00011E 68 A1                  303 	push	ix			; XCPT_PC: the return address
      000120 15 F6                  304 	lda	LISA_CB
      000122 80 A0                  305 	push	a			; XCPT_CB
      000124 16 F6                  306 	lda	LISA_DB
      000126 80 A0                  307 	push	a			; XCPT_DB
      000128 17 F6                  308 	lda	LISA_SB
      00012A 80 A0                  309 	push	a			; XCPT_SB
      00012C 00 F4                  310 	lda	_g_lisa_ie
      00012E 80 A0                  311 	push	a			; XCPT_IE (1: an interrupt needs them enabled)
      000130 00 80                  312 	ldi	#0
      000132 00 FC                  313 	sta	_g_lisa_ie
      000134 CC 8A                  314 	spix				; the interrupted SP = this SP + 14
      000136 10 A0                  315 	txa
      000138 08 A0                  316 	ldc	#0
      00013A 0E 90                  317 	adc	#14
      00013C 80 A0                  318 	push	a
      00013E 18 A0                  319 	txau
      000140 00 90                  320 	adc	#0
      000142 7F D4                  321 	andi	#0x7f			; without the condition bit spix leaves in IX
      000144 00 EE                  322 	swap	0(sp)
      000146 80 A0                  323 	push	a			; XCPT_SP
      000148 CC 8A                  324 	spix				; IX = the frame
      00014A 18 A0                  325 	txau
      00014C 7F D4                  326 	andi	#0x7f			; a data pointer: bit 15 would mean code space
      00014E 80 A0                  327 	push	a
      000150 10 A0                  328 	txa
      000152 80 A0                  329 	push	a			; the regs argument (low byte first)
      000154 0C F2                  330 	ldax	12(sp)			; A = the irq (XCPT_IRQ + 2)
      000156 BB 00                  331 	jal	_lisa_doirq		; IX = the frame to resume
      000158 02 94                  332 	ads	#2
      00015A BE 02                  333 	jal	_lisa_restore
                                    334 
                                    335 	.area	HEAP
