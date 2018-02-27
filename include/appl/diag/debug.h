/*
 * $Id: debug.h 1.8 Broadcom SDK $
 * $Copyright: Copyright 2011 Broadcom Corporation.
 * This program is the proprietary software of Broadcom Corporation
 * and/or its licensors, and may only be used, duplicated, modified
 * or distributed pursuant to the terms and conditions of a separate,
 * written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized
 * License, Broadcom grants no license (express or implied), right
 * to use, or waiver of any kind with respect to the Software, and
 * Broadcom expressly reserves all rights in and to the Software
 * and all intellectual property rights therein.  IF YOU HAVE
 * NO AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE
 * IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE
 * ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of
 * Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS
 * PROVIDED "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY,
 * OR OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL,
 * INCIDENTAL, SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER
 * ARISING OUT OF OR IN ANY WAY RELATING TO YOUR USE OF OR INABILITY
 * TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF
 * THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR USD 1.00,
 * WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING
 * ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.$
 *
 * File:        debug.h
 * Purpose:     Debug flag definitions for diag and test directories
 */

#ifndef   _DIAG_DEBUG_H_
#define   _DIAG_DEBUG_H_

#include <bcm/types.h>

#if defined(BROADCOM_DEBUG)

#include <sal/appl/io.h>

#define DIAG_DBG_NORMAL       (1 << 0)   /* Normal output */
#define DIAG_DBG_ERR          (1 << 1)   /* Print errors */
#define DIAG_DBG_WARN         (1 << 2)   /* Print warnings */
#define DIAG_DBG_VERBOSE      (1 << 3)   /* General verbose output */
#define DIAG_DBG_VVERBOSE     (1 << 4)   /* Very verbose output */
#define DIAG_DBG_COUNTER      (1 << 5)   /* Counter operations */
#define DIAG_DBG_L3           (1 << 6)   /* L3, DefIP and IPMC operations */
#define DIAG_DBG_RCLOAD       (1 << 7)   /* Echo cmds before exec */
#define DIAG_DBG_SOCMEM       (1 << 8)   /* Memory table operations */
#define DIAG_DBG_SYMTAB       (1 << 9)   /* Symbol parsing routines */
#define DIAG_DBG_DMA          (1 << 10)  /* Counter operations */
#define DIAG_DBG_TESTS        (1 << 11)  /* Verbose during tests */
#define DIAG_DBG_TX           (1 << 12)  /* Packet transmit */
#define DIAG_DBG_RX           (1 << 13)  /* Packet transmit */
#define DIAG_DBG_ACL          (1 << 14)  /* Access Control Lists / ACES */

#define DIAG_DBG_COUNT        15

#define DIAG_DBG_NAMES   \
    "NORmal",            \
    "ERRor",             \
    "WARN",              \
    "VERbose",           \
    "VVERbose",          \
    "CounTeR",           \
    "L3",                \
    "RCLOAD",            \
    "SOCMEM",            \
    "SYMTAB",            \
    "DMA",               \
    "TEsts",             \
    "TX",                \
    "RX",                \
    "AcessControlList"

extern uint32 diag_debug_level;
extern char *diag_debug_names[];

#if defined(NO_DEBUG_OUTPUT_DEFAULT)
#define DIAG_DBG_DEFAULT 0
#else
#define DIAG_DBG_DEFAULT (DIAG_DBG_ERR | DIAG_DBG_WARN)
#endif

/*
 * Proper use requires parentheses.  E.g.:
 *     DIAG_DEBUG(DIAG_DBG_FOO, ("Problem %d with unit %d\n", pr, unit));
 */

#define DIAG_DEBUG_CHECK(flags) (((flags) & diag_debug_level) == (flags))
#define DIAG_DEBUG(flags, stuff) \
    if (DIAG_DEBUG_CHECK(flags)) printk stuff

#define TEST_DEBUG_CHECK(flags, stuff) DIAG_DEBUG_CHECK(flags, stuff)
#define TEST_DEBUG(flags, stuff) DIAG_DEBUG(flags, stuff)

#else 

#define DIAG_DEBUG_CHECK(flags) 0
#define DIAG_DEBUG(flags, stuff) 

#define TEST_DEBUG_CHECK(flags) 0
#define TEST_DEBUG(flags, stuff)

#endif /* defined(BROADCOM_DEBUG) */


#endif /* _DIAG_DEBUG_H_ */