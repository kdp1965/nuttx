/****************************************************************************
 * arch/lisa/include/types.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* This file should never be included directly but, rather, only indirectly
 * through sys/types.h
 */

#ifndef __ARCH_LISA_INCLUDE_TYPES_H
#define __ARCH_LISA_INCLUDE_TYPES_H

/****************************************************************************
 * Type Declarations
 ****************************************************************************/

#ifndef __ASSEMBLY__

/* These are the sizes of the standard integer types.  NOTE that these type
 * names have a leading underscore character.  This file will be included
 * (indirectly) by include/stdint.h and typedef'ed to the final name without
 * the underscore character.
 *
 * These are the sizes of the standard SDCC types: sizeof(int) is 16 and
 * sizeof(long) is 32.  Data pointers are 2 bytes (generic: bit 15 marks
 * code space); function pointers are 3 bytes in the huge model (the
 * address and the code bank).
 */

typedef signed char        _int8_t;
typedef unsigned char      _uint8_t;

typedef signed int         _int16_t;
typedef unsigned int       _uint16_t;

typedef signed long        _int32_t;
typedef unsigned long      _uint32_t;

typedef signed long long   _int64_t;
typedef unsigned long long _uint64_t;
#define __INT64_DEFINED

typedef _int64_t           _intmax_t;
typedef _uint64_t          _uintmax_t;

#if defined(__WCHAR_TYPE__)
typedef __WCHAR_TYPE__     _wchar_t;
#else
typedef int                _wchar_t;
#endif

typedef int                _wint_t;
typedef int                _wctype_t;

/* A size is 2 bytes */

typedef signed int         _ssize_t;
typedef unsigned int       _size_t;

/* The interrupt state up_irq_save() returns: the enable bit in a byte */

typedef _uint8_t           irqstate_t;

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_LISA_INCLUDE_TYPES_H */
