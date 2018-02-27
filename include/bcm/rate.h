/*
 * $Id: rate.h 1.26.6.2 Broadcom SDK $
 * 
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
 * DO NOT EDIT THIS FILE!
 * This file is auto-generated.
 * Edits to this file will be lost when it is regenerated.
 */

#ifndef __BCM_RATE_H__
#define __BCM_RATE_H__

#include <bcm/types.h>

/* Rate flags. */
#define BCM_RATE_DLF            0x01       
#define BCM_RATE_MCAST          0x02       
#define BCM_RATE_BCAST          0x04       
#define BCM_RATE_UCAST          0x08       
#define BCM_RATE_SALF           0x10       
#define BCM_RATE_RSVD_MCAST     0x20       
#define BCM_RATE_CTRL_BUCKET_1  0x00010000 
#define BCM_RATE_CTRL_BUCKET_2  0x00020000 
#define BCM_RATE_ALL            (BCM_RATE_BCAST | BCM_RATE_MCAST | BCM_RATE_DLF | BCM_RATE_UCAST | BCM_RATE_SALF | BCM_RATE_RSVD_MCAST) 

/* bcm_rate_limit_s */
typedef struct bcm_rate_limit_s {
    int br_dlfbc_rate; 
    int br_mcast_rate; 
    int br_bcast_rate; 
    int flags; 
} bcm_rate_limit_t;

/* bcm_rate_limit_t_init */
extern void bcm_rate_limit_t_init(
    bcm_rate_limit_t *rate_limit);

#ifndef BCM_HIDE_DISPATCHABLE

/* 
 * Configure/retrieve rate limit and on/off state of DLF, MCAST, and
 * BCAST limiting.
 */
extern int bcm_rate_set(
    int unit, 
    int pps, 
    int flags);

/* 
 * Configure/retrieve rate limit and on/off state of DLF, MCAST, and
 * BCAST limiting.
 */
extern int bcm_rate_get(
    int unit, 
    int *pps, 
    int *flags);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_mcast_set(
    int unit, 
    int pps, 
    int flags, 
    int port);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_mcast_get(
    int unit, 
    int *pps, 
    int *flags, 
    int port);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_dlfbc_set(
    int unit, 
    int pps, 
    int flags, 
    int port);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_dlfbc_get(
    int unit, 
    int *pps, 
    int *flags, 
    int port);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_bcast_set(
    int unit, 
    int pps, 
    int flags, 
    int port);

/* 
 * Configure or retrieve rate limit for specified packet type on given
 * port.
 */
extern int bcm_rate_bcast_get(
    int unit, 
    int *pps, 
    int *flags, 
    int port);

/* 
 * Front ends to bcm_*_rate_set/get functions. Uses a single data
 * structure to write into all the 3 rate control registers.
 */
extern int bcm_rate_type_set(
    int unit, 
    bcm_rate_limit_t *rl);

/* 
 * Front ends to bcm_*_rate_set/get functions. Uses a single data
 * structure to write into all the 3 rate control registers.
 */
extern int bcm_rate_type_get(
    int unit, 
    bcm_rate_limit_t *rl);

/* 
 * Configure/retrieve metering rate limit for specified packet type on
 * given port.
 */
extern int bcm_rate_bandwidth_set(
    int unit, 
    bcm_port_t port, 
    int flags, 
    uint32 kbits_sec, 
    uint32 kbits_burst);

/* 
 * Configure/retrieve metering rate limit for specified packet type on
 * given port.
 */
extern int bcm_rate_bandwidth_get(
    int unit, 
    bcm_port_t port, 
    int flags, 
    uint32 *kbits_sec, 
    uint32 *kbits_burst);

#endif /* BCM_HIDE_DISPATCHABLE */

#endif /* __BCM_RATE_H__ */