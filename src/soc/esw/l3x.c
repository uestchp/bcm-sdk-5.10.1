/*
 * $Id: l3x.c 1.41 Broadcom SDK $
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
 * XGS L3 Table Manipulation API routines.
 *
 * The L3Xm memory is an aggregate structure supported by hardware.
 * When an entry is read from it, it is constructed from the L3X_BASEm,
 * L3X_VALIDm, and L3X_HITm tables.  NOTE: the L3X table is read-only.
 */

#include <sal/core/libc.h>

#include <soc/drv.h>
#include <soc/l3x.h>
#include <soc/mem.h>
#include <soc/hash.h>
#include <soc/util.h>
#include <soc/debug.h>

#ifdef INCLUDE_L3
#ifdef BCM_EASYRIDER_SUPPORT
#include <soc/er_cmdmem.h>
#endif

#ifdef BCM_XGS_SWITCH_SUPPORT

/*
 * Function:
 *      soc_l3x_init
 * Purpose:
 *      Initialize L3 table subsystem.
 * Parameters:
 *      unit - StrataSwitch unit number.
 * Returns:
 *      SOC_E_xxx
 */
int
soc_l3x_init(int unit)
{
    return SOC_E_NONE;
}

static int
soc_l3x_lookup_hw(int unit,
	       l3x_entry_t *key_entry, l3x_entry_t *result,
	       int *index_ptr)
{
    schan_msg_t 	schan_msg;

    if (soc_cm_debug_check(DK_SOCMEM)) {
	soc_cm_debug(DK_SOCMEM, "L3 Lookup: key ");
	soc_mem_entry_dump(unit, L3Xm, key_entry);
	soc_cm_debug(DK_SOCMEM, "\n");
    }

    schan_msg_clear(&schan_msg);
    schan_msg.l3lkup.header.opcode = L3_LOOKUP_CMD_MSG;
    schan_msg.l3lkup.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3lkup.header.datalen = 16;
    schan_msg.l3lkup.address = soc_mem_addr(unit, L3Xm, ARL_BLOCK(unit), 0);

    schan_msg.l3lkup.data[0] = key_entry->entry_data[0];
    schan_msg.l3lkup.data[1] = key_entry->entry_data[1];
    schan_msg.l3lkup.data[2] = key_entry->entry_data[2];
    schan_msg.l3lkup.data[3] = key_entry->entry_data[3];

    /*
     * L3 lookup s-channel operations:
     * sends header + address + 4 data words
     * receives header + 4 data words
     */
    SOC_IF_ERROR_RETURN(soc_schan_op(unit, &schan_msg, 6, 5, 1));

    if (schan_msg.readresp.header.opcode != READ_MEMORY_ACK_MSG) {
        soc_cm_debug(DK_ERR,
		     "soc_l3x_lookup: "
                     "invalid S-Channel reply, expected READ_MEMORY_ACK:\n");
        soc_schan_dump(unit, &schan_msg, 5);
        return SOC_E_INTERNAL;
    }

    /*
     * response format:
     *		readresp.header		memory read ack
     *		readresp.data[0]	l3_table_data[31:0]
     *		readresp.data[1]	2'h0 l3_table_data[61:32]
     *		readresp.data[2]	19'h0 hit l3_table_data[73:62]
     *		readresp.data[3]	19'h0 l3index[12:0]
     * if the lookup failed all readresp.data[n] non x'h0 bits will be 1
     */

    if (schan_msg.readresp.data[0] == 0xffffffff &&
	schan_msg.readresp.data[1] == 0xffffffff) {
	*index_ptr = -1;
	return SOC_E_NOT_FOUND;
    }

    *index_ptr = schan_msg.readresp.data[3];
    result->entry_data[0] = key_entry->entry_data[0];	/* use key's ip_addr */
    result->entry_data[1] = schan_msg.readresp.data[0];
    result->entry_data[2] = schan_msg.readresp.data[1];
    result->entry_data[3] = schan_msg.readresp.data[2];
    result->entry_data[3] |= 1 << 13;			/* set valid bit */

    if (soc_cm_debug_check(DK_SOCMEM)) {
        soc_cm_debug(DK_SOCMEM, "L3 lookup: result %d ", *index_ptr);
        soc_mem_entry_dump(unit, L3Xm, result);
        soc_cm_debug(DK_SOCMEM, "\n");
    }

    return SOC_E_NONE;
}

static int
soc_l3x_lookup_sw(int unit,
		  int sip0,
		  int hash_sel,
		  l3x_entry_t *key_entry, l3x_entry_t *result,
		  int *index_ptr)
{
    l3x_entry_t		chip_entry;
    int 		bucket;
    int 		mem_table_index, mem_table_index_bound;
    uint32		ipmc_config;
    int			ipmc;
    uint8		key[XGS_HASH_KEY_SIZE];

    ipmc_config = SOC_CONTROL(unit)->hash_key_config;

    soc_draco_l3x_base_entry_to_key(unit, key_entry, sip0, key);

    ipmc = (ipmc_config & L3X_IPMC_ENABLE) &&
	   SOC_L3X_IP_MULTICAST(soc_L3Xm_field32_get(unit, key_entry,
						     IP_ADDRf));
    bucket = soc_draco_l3_hash(unit, hash_sel, ipmc, key);

    /* Lookup makes "sense" by definition */
    soc_mem_field32_set(unit, L3Xm, (uint32 *) key_entry, IPMCf, ipmc);

    mem_table_index = (bucket * SOC_L3X_BUCKET_SIZE(unit));
    mem_table_index_bound = mem_table_index + SOC_L3X_BUCKET_SIZE(unit);

    for (; mem_table_index < mem_table_index_bound; mem_table_index++) {
	SOC_IF_ERROR_RETURN
	    (soc_mem_read(unit, L3Xm, MEM_BLOCK_ANY,
			  mem_table_index, &chip_entry));

	if (!soc_L3Xm_field32_get(unit, &chip_entry, L3_VALIDf)) {
	    /* Valid bit unset, entry blank */
	    continue;
	}

	if (soc_mem_compare_key(unit, L3Xm, key_entry, &chip_entry) == 0) {
	    /* Found the matching entry */
	    sal_memcpy(result, &chip_entry, sizeof(chip_entry));
	    *index_ptr = mem_table_index;
	    return SOC_E_NONE;
	}
    }

    *index_ptr = -1;
    return SOC_E_NOT_FOUND;
}


/*
 * Function:
 *	soc_l3x_lock
 * Purpose:
 *	Lock all of the L3 host tables
 * Parameters:
 *	unit - StrataSwitch PCI device unit number
 * Returns:
 *	SOC_E_XXX
 */

int
soc_l3x_lock(int unit)
{
#ifdef BCM_EASYRIDER_SUPPORT
    if (SOC_IS_EASYRIDER(unit)) {
        soc_mem_lock(unit, L3_ENTRY_V4m);
        soc_mem_lock(unit, L3_ENTRY_V6m);
    } else
#endif
#ifdef BCM_FIREBOLT_SUPPORT
    if (SOC_IS_FBX(unit)) {
        soc_mem_lock(unit, L3_ENTRY_ONLYm);
    } else
#endif
    if (SOC_IS_XGS12_SWITCH(unit)) {
        soc_mem_lock(unit, L3Xm);
    }
    return SOC_E_NONE;
}

/*
 * Function:
 *	soc_l3x_unlock
 * Purpose:
 *	Unlock all of the L3 host tables
 * Parameters:
 *	unit - StrataSwitch PCI device unit number
 * Returns:
 *	SOC_E_XXX
 */

int
soc_l3x_unlock(int unit)
{
#ifdef BCM_EASYRIDER_SUPPORT
    if (SOC_IS_EASYRIDER(unit)) {
        soc_mem_unlock(unit, L3_ENTRY_V6m);
        soc_mem_unlock(unit, L3_ENTRY_V4m);
    } else
#endif
#ifdef BCM_FIREBOLT_SUPPORT
    if (SOC_IS_XGS3_SWITCH(unit)) {
        soc_mem_unlock(unit, L3_ENTRY_ONLYm);
    } else
#endif
    if (SOC_IS_XGS12_SWITCH(unit)) {
        soc_mem_unlock(unit, L3Xm);
    }
    return SOC_E_NONE;
}

/*
 * Function:
 *      soc_l3x_lookup
 * Purpose:
 *	Locate the L3 entry matching the provided key by searching
 *	the corresponding bucket.
 * Parameters:
 *	unit - StrataSwitch unit #
 *	key_entry - L3X entry to look up; only IP_ADDR field is relevant
 *	result - L3X entry to receive entire found entry
 *	index_ptr (OUT) - If found, receives table index where found
 * Returns:
 *      SOC_E_XXX on other failure
 *      SOC_E_NOT_FOUND if entry is not found
 *      SOC_E_NONE (0) on success (entry found):
 * Notes:
 *	Some chips have hardware support for L3 lookup.  If not, we
 *	simulate it using a software search of the expected hash bucket.
 *      This is kinda slow.
 */

int
soc_l3x_lookup(int unit,
	       l3x_entry_t *key_entry, l3x_entry_t *result,
	       int *index_ptr)
{
    uint32	regval;
    int		hash_sel;
    uint32	ipmc_config;
    int		rv;

    /* Otherwise, we are wasting our time */
    soc_mem_field32_set(unit, L3Xm, (uint32 *)key_entry, L3_VALIDf, 1);

    if (soc_feature(unit, soc_feature_l3_lookup_cmd)) {
	return soc_l3x_lookup_hw(unit, key_entry, result, index_ptr);
    }

    SOC_IF_ERROR_RETURN(READ_HASH_CONTROLr(unit, &regval));
    hash_sel = soc_reg_field_get(unit, HASH_CONTROLr, regval, HASH_SELECTf);

    SOC_IF_ERROR_RETURN(_soc_mem_cmp_l3x_sync(unit));
    ipmc_config = SOC_CONTROL(unit)->hash_key_config;

    if (!(ipmc_config & L3X_IPMC_ENABLE)) {
	return soc_l3x_lookup_sw(unit, 0, hash_sel,
				 key_entry, result, index_ptr);
    }

    /* possible do two lookups in ipmc_enable case */
    rv = SOC_E_NOT_FOUND;
    if (ipmc_config & L3X_IPMC_SIP) {
	rv = soc_l3x_lookup_sw(unit, 0, hash_sel,
			       key_entry, result, index_ptr);
	if (rv != SOC_E_NOT_FOUND) {
	    return rv;
	}
    }
    if (ipmc_config & L3X_IPMC_SIP0) {
        if (!SOC_IS_LYNX(unit)) {
            /* Lynx uses the full key for entry comparison within the bucket */
            soc_mem_field32_set(unit, L3Xm, (uint32 *)key_entry,
                                SRC_IP_ADDRf, 0);
        }
	rv = soc_l3x_lookup_sw(unit, 1, hash_sel,
			       key_entry, result, index_ptr);
    }
    return rv;
}

/*
 * Function:
 *	    _soc_l3x_entry_mem_view_get
 * Purpose:
 *  	Given base table entry & memory extract view name 
 *      & number of base slots entry occupies.
 * Parameters: 
 *      unit         -  (IN) SOC device number.
 *      base_mem     -  (IN) Base memory name. 
 *      base_entry   -  (IN) Base memory entry pointer.
 *      entry_mem    -  (OUT)Entry memory. 
 *      entry_size   -  (OUT)Number of base entry slots entry occupies.
 * Returns:
 *     SOC_E_XXX
 */

STATIC int
_soc_l3x_entry_mem_view_get(int unit,
                            soc_mem_t base_mem,
                            uint32 *base_entry,
                            soc_mem_t *entry_mem,   
                            int *entry_size) 
{
    int key_type;                /* Entry type.           */
    soc_mem_t mem = INVALIDm;    /* Resolved entry memory.*/             
    int v6 = 0;                  /* IPv6 entry flag.      */
    int ipmc = 0;                /* IPMC entry flag.      */  


    /* Input parameters check. */
    if ((NULL == entry_mem) || (NULL == entry_size) || 
        (!SOC_MEM_IS_VALID(unit, base_mem)))  {
        return (SOC_E_PARAM);
    }

    /* Check valid bit. */
    if (!SOC_MEM_FIELD_VALID(unit, base_mem, VALIDf)) {
        return (SOC_E_UNAVAIL);
#ifndef BCM_TRIDENT_SUPPORT
    } else if (!soc_mem_field32_get(unit, base_mem, base_entry, VALIDf)) { 
        *entry_mem = INVALIDm;
        *entry_size = 1;
        return (SOC_E_NONE);
#endif
    }

    /* We start by reading one minimal size L3 entry and
     * identifying the actual entry type. */
    if (SOC_MEM_FIELD_VALID(unit, base_mem, KEY_TYPEf)) {
        key_type = soc_mem_field32_get(unit, base_mem, base_entry, KEY_TYPEf);
        switch (key_type) {
          case TR_L3_HASH_KEY_TYPE_V4UC:
              mem = L3_ENTRY_IPV4_UNICASTm;
              break;
          case TR_L3_HASH_KEY_TYPE_V4MC:
              mem = L3_ENTRY_IPV4_MULTICASTm;
              break;
          case TR_L3_HASH_KEY_TYPE_V6UC:
              mem = L3_ENTRY_IPV6_UNICASTm;
              break;
          case TR_L3_HASH_KEY_TYPE_V6MC:
              mem = L3_ENTRY_IPV6_MULTICASTm;
              break;
          case TR_L3_HASH_KEY_TYPE_LMEP:
          case TR_L3_HASH_KEY_TYPE_RMEP:
          case TR_L3_HASH_KEY_TYPE_TRILL:
              mem = L3_ENTRY_IPV4_UNICASTm;
              break;
          default:
              return (SOC_E_PARAM);
        }
    } else if ((SOC_MEM_FIELD_VALID(unit, base_mem, V6f)) &&
               (SOC_MEM_FIELD_VALID(unit, base_mem, IPMCf))) {
        v6 = soc_mem_field32_get(unit, base_mem, base_entry, V6f);
        ipmc =  soc_mem_field32_get(unit, base_mem, base_entry, IPMCf);
        if (v6 && ipmc)  {
            mem = L3_ENTRY_IPV6_MULTICASTm;
        } else if (v6) {
            mem = L3_ENTRY_IPV6_UNICASTm;
        } else if (ipmc) {
            mem = L3_ENTRY_IPV4_MULTICASTm;
        } else {
            mem = L3_ENTRY_IPV4_UNICASTm;
        }
    } else {
        return (SOC_E_UNAVAIL);
    }

    *entry_size = soc_mem_index_count(unit, base_mem) / soc_mem_index_count(unit, mem);
    *entry_mem = mem;
    return (SOC_E_NONE);
}

/*
 * Function:
 *	_soc_l3x_hit_sync
 * Purpose:
 *	Adjusts the hit bits after an insert, as necessary
 * Parameters:
 *	unit - StrataSwitch PCI device unit number
 *	entry - Entry to operate on
 * Returns:
 *	SOC_E_XXX
 * Notes:
 *	The L3 insert command on the 5665, 5673, 5674, and 5690 does
 *	not change the hit bit of the inserted entry.  This helper routine
 *	changes it to match the inserted entry.
 *
 *	There is a race condition where if other hit bit(s) got set in the
 *	same bucket between our read and write operations, these hardware
 *	updates to the hit bits could be lost.
 */

STATIC int
_soc_l3x_hit_sync(int unit, l3x_entry_t *entry)
{
    l3x_entry_t		result;
    l3x_hit_entry_t	hit_bits;
    int			index, bucket, bit, r;
    uint32		l3_hit;

    

    if ((r = soc_l3x_lookup(unit, entry, &result, &index)) < 0) {
	if (r == SOC_E_NOT_FOUND) {
	    r = SOC_E_NONE;
	}
        return r;
    }

    bucket = index / SOC_L3X_BUCKET_SIZE(unit);
    bit = index % SOC_L3X_BUCKET_SIZE(unit);

    /* Read/modify/write bucket hit bits */

    soc_mem_lock(unit, L3X_HITm);

    if ((r = soc_mem_read(unit, L3X_HITm, MEM_BLOCK_ANY,
                          bucket, &hit_bits)) < 0) {
        soc_mem_unlock(unit, L3X_HITm);
        return r;
    }

    l3_hit = soc_L3Xm_field32_get(unit, entry, L3_HITf);

    if (((hit_bits.entry_data[0] >> bit) & 1) != l3_hit) {
	hit_bits.entry_data[0] ^= (1 << bit);

	r = soc_mem_write(unit, L3X_HITm, MEM_BLOCK_ANY,
			  bucket, &hit_bits);
    }

    soc_mem_unlock(unit, L3X_HITm);

    return r;
}

/*
 * Function:
 *      soc_l3x_insert
 * Purpose:
 *	Insert an entry into the L3X hash table.
 * Parameters:
 *	unit - StrataSwitch unit #
 *	entry - L3X entry to insert
 * Returns:
 *	SOC_E_NONE - success
 *	SOC_E_FULL - hash bucket full
 * Notes:
 *	Uses hardware insertion; sends an L3 INSERT message over the
 *	S-Channel.  The hardware needs the L3Xm entry type.
 *	This affects the L3_ENTRYm table, L3_VALIDm table,
 *	and the L3_HITm table.
 */

int
soc_l3x_insert(int unit, l3x_entry_t *entry)
{
    schan_msg_t 	schan_msg;
    int			rv;

    if (soc_cm_debug_check(DK_SOCMEM)) {
	soc_cm_debug(DK_SOCMEM, "Insert table[L3X]: ");
	soc_mem_entry_dump(unit, L3Xm, entry);
	soc_cm_debug(DK_SOCMEM, "\n");
    }

    schan_msg_clear(&schan_msg);
    schan_msg.l3ins.header.opcode = L3_INSERT_CMD_MSG;
    schan_msg.l3ins.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3ins.header.datalen = 16;	/* Bytes in message */

    /* Fill in L3 packet data */

    sal_memcpy(schan_msg.l3ins.data, entry, sizeof (schan_msg.l3ins.data));

    /* Execute S-Channel operation (header word + 4 DWORDs) */

    rv = soc_schan_op(unit, &schan_msg, 5, 1, 1);

    if (rv == SOC_E_FAIL) {
        soc_cm_debug(DK_SOCMEM, "Insert table[L3X]: hash bucket full\n");
	return SOC_E_FULL;
    }

    if (rv >= 0 &&
	soc_feature(unit, soc_feature_l3x_ins_igns_hit)) {
	rv = _soc_l3x_hit_sync(unit, entry);
    }

    return rv;
}

/*
 * Function:
 *	soc_l3x_delete
 * Purpose:
 *	Delete an entry from the L3X hash table.
 * Parameters:
 *	unit - StrataSwitch unit #
 *	entry - L3X entry to delete
 * Returns:
 *	SOC_E_NONE - success
 * Notes:
 *	Uses hardware deletion; sends an L3 DELETE message over the
 *	S-Channel.  The hardware needs the L3Xm entry type.
 *	Only the L3_VALIDm table is affected.
 *	There is no indication if the entry is not found.
 */

int
soc_l3x_delete(int unit, l3x_entry_t *entry)
{
    schan_msg_t 	schan_msg;

    if (soc_cm_debug_check(DK_SOCMEM)) {
	soc_cm_debug(DK_SOCMEM, "Delete table[L3]: ");
	soc_mem_entry_dump(unit, L3Xm, entry);
	soc_cm_debug(DK_SOCMEM, "\n");
    }

    if (soc_feature(unit, soc_feature_l3x_delete_bf_bug)) {
	int			bucket, offset, r;
	uint32			vbits;
	l3x_entry_t		tmp;

	/*
	 * BCM5690_A0 erratum: L3 delete fails when bucket full.
	 * Deletion must be performed by software clearing valid bit.
	 */

	bucket = soc_l3x_hash(unit, 0, entry);
	SOC_IF_ERROR_RETURN(bucket);

	soc_mem_lock(unit, L3X_VALIDm);

	if ((r = soc_mem_read(unit, L3X_VALIDm, MEM_BLOCK_ANY,
			      bucket, &vbits)) < 0) {
	    soc_mem_unlock(unit, L3X_VALIDm);
	    return r;
	}

	if ((vbits == 0xff) ||		/* Bucket full */
            (SOC_IS_TUCANA(unit))) {    /* Parity regen on delete required */
	    for (offset = 0; offset < 8; offset++) {
                if (vbits & (1 << offset)) {
		if ((r = soc_mem_read(unit, L3Xm, MEM_BLOCK_ANY,
				      bucket * 8 + offset, &tmp)) < 0) {
		    soc_mem_unlock(unit, L3X_VALIDm);
		    return r;
		}

		if (soc_mem_compare_key(unit, L3Xm, entry, &tmp) == 0) {
		    vbits &= ~(1 << offset);
		    r = soc_mem_write(unit, L3X_VALIDm, MEM_BLOCK_ALL,
				      bucket, &vbits);
                        if (r >= 0) {
                            r = soc_mem_write
                                    (unit, L3X_BASEm, MEM_BLOCK_ALL,
                                     bucket * 8 + offset,
                                     soc_mem_entry_null(unit, L3X_BASEm));
                        }
		    soc_mem_unlock(unit, L3X_VALIDm);
		    return r;
		}
	    }
	}
	}

	soc_mem_unlock(unit, L3X_VALIDm);
    }

    schan_msg_clear(&schan_msg);
    schan_msg.l3del.header.opcode = L3_DELETE_CMD_MSG;
    schan_msg.l3del.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3del.header.datalen = 16;		/* Bytes in message */

    /* Fill in packet data */

    sal_memcpy(schan_msg.l3del.data, entry, sizeof (schan_msg.l3del.data));

    /* Execute S-Channel operation (header word + 1 DWORD) */

    SOC_IF_ERROR_RETURN(soc_schan_op(unit, &schan_msg, 5, 1, 1));

    return SOC_E_NONE;
}

/*
 * Function:
 *      soc_l3x_delete_all
 * Purpose:
 *	Clear the L3 table by invalidating entries.
 * Parameters:
 *	unit - StrataSwitch unit #
 * Returns:
 *	SOC_E_XXX
 */

int
soc_l3x_delete_all(int unit)
{
    int			index_min, index_max, index;

    index_min = soc_mem_index_min(unit, L3X_VALIDm);
    index_max = soc_mem_index_max(unit, L3X_VALIDm);

    for (index = index_min; index <= index_max; index++) {
	SOC_IF_ERROR_RETURN
	    (soc_mem_write(unit, L3X_VALIDm,
			   MEM_BLOCK_ALL, index,
			   soc_mem_entry_null(unit, L3X_VALIDm)));
    }

    return SOC_E_NONE;
}

/*
 * Function:
 *      soc_l3x_entries
 * Purpose:
 *	Return number of valid entries in L3 table.
 * Parameters:
 *	unit - StrataSwitch unit #
 * Returns:
 *	If >= 0, number of entries in L3 table
 *	If < 0, SOC_E_XXX
 * Notes:
 *	Somewhat slow; has to read whole table.
 *	New improved: Now with DMA power.
 */

int
soc_l3x_entries(int unit)
{
    int			index_min, index_max, total, rv;
    l3x_valid_entry_t	*buf, *bufp;
    uint32              bucket_bmap;
    uint32              count, index; 
    index_min = soc_mem_index_min(unit, L3X_VALIDm);
    index_max = soc_mem_index_max(unit, L3X_VALIDm);
    count = soc_mem_index_count(unit, L3X_VALIDm);

    /* Allocate dma-able buffer; DMA the valid bits table. */
    buf = soc_cm_salloc(unit,
			SOC_MEM_TABLE_BYTES(unit, L3X_VALIDm),
                        "soc_l3x_entries");
    if (!buf) {
        return SOC_E_MEMORY;
    }

    rv = soc_mem_read_range(unit, L3X_VALIDm, MEM_BLOCK_ANY,
			    index_min, index_max, buf);

    if (rv < 0) {
	soc_cm_sfree(unit, buf);
	return rv;
    }

    total = 0;

    for (index = 0; index < count; index++) {
        bufp = soc_mem_table_idx_to_pointer(unit, L3X_VALIDm, 
                                     l3x_valid_entry_t*, buf, index);
        bucket_bmap = soc_mem_field32_get(unit, L3X_VALIDm, bufp, 
                    BUCKET_BITMAPf);
	total += _shr_popcount(bucket_bmap & 0xff);
    }

    soc_cm_sfree(unit, buf);

    return total;
}

/*
 * Function:
 *	soc_l3x_hash
 * Purpose:
 *	Return hash bucket into which L3 entry belongs, according to HW
 * Parameters:
 *	unit - StrataSwitch unit #
 *	key_sip0 - True if IPMC hash should ignore (zero) source IP address
 *	entry - L3X entry to hash
 * Returns:
 *	If >= 0, hash bucket number
 *	If < 0, SOC_E_XXX
 */

int
soc_l3x_hash(int unit, int key_sip0, l3x_entry_t *entry)
{
    hashinput_entry_t		hent;
    uint32			key[SOC_MAX_MEM_WORDS];
    uint32			result;
    int                         ipmc;

    sal_memset(key, 0, sizeof(key));
    soc_L3Xm_field_get(unit, entry, IP_ADDRf, &key[0]);
    ipmc = soc_L3Xm_field32_get(unit, entry, IPMCf);

    if (ipmc && SOC_L3X_IP_MULTICAST(key[0])) {
	if (key_sip0) {
	    key[1] = 0;
	} else {
	    soc_L3Xm_field_get(unit, entry, SRC_IP_ADDRf, &key[1]);
	}
	soc_HASHINPUTm_field32_set(unit, &hent, KEY_TYPEf,
				   XGS_HASH_KEY_TYPE_L3MC);
	if (soc_feature(unit, soc_feature_l3_sgv)) {
	    /* Draco 1.5 always includes VLAN ID in IPMC hash */
	    soc_L3Xm_field_get(unit, entry, VLAN_IDf, &key[2]);
	}
    } else {
	soc_HASHINPUTm_field32_set(unit, &hent, KEY_TYPEf,
				   XGS_HASH_KEY_TYPE_L3UC);
    }

    soc_HASHINPUTm_field_set(unit, &hent, KEYf, key);

    SOC_IF_ERROR_RETURN(soc_mem_write(unit, HASHINPUTm, MEM_BLOCK_ALL,
				      0, &hent));

    SOC_IF_ERROR_RETURN(READ_HASH_OUTPUTr(unit, &result));

    return (int)result;
}

/*
 * Function:
 *      soc_l3x_software_hash
 * Purpose:
 *	Return hash bucket into which L3 entry belongs, according to software
 * Parameters:
 *	unit - StrataSwitch unit #
 *	hash_select - Which hash method to use; one of XGS_HASH_xxx
 *	ipmc - TRUE if input is an IP multicast entry, FALSE if regular L3
 *	key_sip0 - FALSE to use both IP_ADDR and SRC_IP_ADDR field of entry;
 *		   TRUE to use "only" IP_ADDR field (SRC_IP_ADDR = 0).
 *	entry - L3X entry to hash
 * Returns:
 *	If >= 0, hash bucket number
 *	If < 0, SOC_E_XXX
 */
int
soc_l3x_software_hash(int unit, int hash_select, int ipmc, int key_sip0,
		      l3x_entry_t *entry)
{
    uint8	key[XGS_HASH_KEY_SIZE];
    int		rv;

    soc_draco_l3x_base_entry_to_key(unit, entry, key_sip0, key);
    rv = soc_draco_l3_hash(unit, hash_select, ipmc, key);

    return rv;
}

#ifdef BCM_FIREBOLT_SUPPORT
/*
 * Function:
 *      soc_fb_l3x_bank_lookup
 * Purpose:
 *      Send an L3 lookup message over the S-Channel and receive the
 *      response.
 * Parameters:
 *      unit - StrataSwitch unit #
 *      banks - For dual hashing, which halves are selected (inverted)
 *      key - L3X entry to look up; only MAC+VLAN fields are relevant
 *      result - L3X entry to receive entire found entry
 *      index_ptr (OUT) - If found, receives table index where found
 * Returns:
 *      SOC_E_INTERNAL if retries exceeded or other internal error
 *      SOC_E_NOT_FOUND if the entry is not found.
 *      SOC_E_NONE (0) on success (entry found):
 * Notes:
 *      The S-Channel response contains either a matching L3 entry,
 *      or an entry with a key of all f's if not found.  It is okay
 *      if the result pointer is the same as the key pointer.
 *      Retries the lookup in cases where the particular chip requires it.
 */

int
soc_fb_l3x_bank_lookup(int unit, uint8 banks,
                  l3_entry_ipv6_multicast_entry_t *key,
                  l3_entry_ipv6_multicast_entry_t *result,
                  int *index_ptr)
{
    schan_msg_t         schan_msg;
    int                 i;
    int                 entry_dw;
    soc_mem_t           mem;
    int                 nbits;
    int                 rv;
    int                 entry_size;

    SOC_IF_ERROR_RETURN(_soc_l3x_entry_mem_view_get(unit,
        L3_ENTRY_IPV4_UNICASTm, (uint32 *) key, &mem, &entry_size));

    if (INVALIDm == mem) {
        return SOC_E_PARAM;
    }

    nbits = soc_mem_entry_bits(unit, mem) % 32;
    if (nbits == 0) {
        nbits = 32;
    }
    entry_dw = soc_mem_entry_words(unit, mem);
    schan_msg_clear(&schan_msg);
    schan_msg.l3x2.header.opcode = L3X2_LOOKUP_CMD_MSG;
    schan_msg.l3x2.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3x2.header.dstblk = SOC_BLOCK2SCH(unit, IPIPE_BLOCK(unit));
    schan_msg.l3x2.header.cos = banks & 0x3;
    schan_msg.l3x2.header.datalen = entry_dw * 4;

    /* Fill in entry data */

    sal_memcpy(schan_msg.l3x2.data, key, entry_dw * 4);

    /*
     * Write onto S-Channel "L3 lookup" command packet consisting of
     * header word + 4/4/7/13 words of L3 key, and read back header
     * word + 5/5/8/14 words of lookup result.
     * (index of the extry + contents of the entry)
     */

    rv = soc_schan_op(unit, &schan_msg, entry_dw + 1, entry_dw + 2, 1);

    /* Check result */

    if (schan_msg.readresp.header.opcode != L3X2_LOOKUP_ACK_MSG) {
        soc_cm_debug(DK_ERR,
                     "soc_fb_l3x_lookup: invalid S-Channel reply, "
                     "expected L3X2_LOOKUP_ACK_MSG:\n");
        soc_schan_dump(unit, &schan_msg, entry_dw + 2);
        return SOC_E_INTERNAL;
    }

    /*
     * Fill in result entry from read data.
     *
     * Format of S-Channel response:
     *          readresp.header : L3 lookup Ack S-channel header
     *          readresp.data[0-n]: entry contents
     *          readresp.data[n]: index of the entry in the L3_ENTRY_XXXm Table
     * =======================================
     * | PERR_PBM |  Index  | L3x entry data |
     * =======================================
     */

    if ((schan_msg.readresp.header.cpu) || (rv == SOC_E_FAIL)) {
        *index_ptr = -1;
        if (soc_feature(unit, soc_feature_l3x_parity)) {
            int perr_pbm_pos;
            int perr_index; 
            uint32 perr_pbm;
            if (SOC_IS_RAVEN(unit) && mem == L3_ENTRY_IPV6_MULTICASTm) {
                perr_index = entry_dw;
                perr_pbm_pos = _shr_popcount(SOC_MEM_INFO(unit, mem).index_max) -
                    soc_mem_entry_bits(unit, mem) / 32;
            } else {
                perr_index = entry_dw - 1;
                perr_pbm_pos = SOC_L3X_PERR_PBM_POS(unit, mem);
            }
            perr_pbm = ((schan_msg.readresp.data[perr_index] >>
                         perr_pbm_pos) & (SOC_L3X_BUCKET_SIZE(unit) - 1));
            if (perr_pbm) {
                uint32 index = (schan_msg.readresp.data[perr_index] >> nbits) &
                               ((1 << (32 - nbits)) - 1);
                index |= (schan_msg.readresp.data[perr_index + 1] << (32 - nbits)) &
                         soc_mem_index_max(unit, mem); /* Assume size of table 2^N */
                soc_cm_debug(DK_ERR,
                    "Lookup table[L3_ENTRY_XXX]: Parity Error Index %d Bucket Bitmap 0x%08x\n",
                     index,
                    (schan_msg.readresp.data[perr_index] >> perr_pbm_pos) & 
                        (SOC_L3X_BUCKET_SIZE(unit) - 1) );
                return SOC_E_INTERNAL;
            }
        }
        return SOC_E_NOT_FOUND;
    }


    for(i = 0; i < entry_dw - 1; i++) {
        result->entry_data[i] = schan_msg.readresp.data[i];
    }
    result->entry_data[i] = schan_msg.readresp.data[i] &  ((1 << nbits) - 1);
    *index_ptr = (schan_msg.readresp.data[i] >> nbits) &
                 ((1 << (32 - nbits)) - 1);
    *index_ptr |= (schan_msg.readresp.data[i + 1] << (32 - nbits)) &
                         soc_mem_index_max(unit, mem);

    if (soc_cm_debug_check(DK_SOCMEM)) {
        soc_cm_debug(DK_SOCMEM, "L3 entry lookup: ");
        soc_mem_entry_dump(unit, mem, result);
        soc_cm_debug(DK_SOCMEM, " (index=%d)\n", *index_ptr);
    }

    return SOC_E_NONE;
}

/*
 * Function:
 *      soc_fb_l3x_bank_insert
 * Purpose:
 *      Insert an entry into the L3X hash table.
 * Parameters:
 *      unit - StrataSwitch unit #
 *      banks - For dual hashing, which halves are selected (inverted)
 *      entry - L3X entry to insert
 * Returns:
 *      SOC_E_NONE - success
 *      SOC_E_FULL - hash bucket full
 * Notes:
 *      Uses hardware insertion; sends an L3 INSERT message over the
 *      S-Channel.  The hardware needs the L3_ENTRY_XXX entry type.
 *      This affects the L3_ENTRY_ONLYm table, L3_VALID_ONLYm table, L3_STATICm
 *      table, and the L3_HITm table.
 */

int
soc_fb_l3x_bank_insert(int unit, uint8 banks,
                  l3_entry_ipv6_multicast_entry_t *entry)
{
    schan_msg_t         schan_msg;
    int			rv;
    int                 entry_dw;
    soc_mem_t           mem;
    int entry_size;

    SOC_IF_ERROR_RETURN(_soc_l3x_entry_mem_view_get(unit,
        L3_ENTRY_IPV4_UNICASTm, (uint32 *) entry, &mem, &entry_size));
    if (INVALIDm == mem) {
        return SOC_E_PARAM;
    }

    if (soc_feature(unit, soc_feature_generic_table_ops)) {
        return soc_mem_generic_insert(unit, mem, MEM_BLOCK_ANY, banks,
                                      (void *)entry, NULL, 0);
    }

    entry_dw = soc_mem_entry_words(unit, mem);
    schan_msg_clear(&schan_msg);
    schan_msg.l3x2.header.opcode = L3_INSERT_CMD_MSG;
    schan_msg.l3x2.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3x2.header.dstblk = SOC_BLOCK2SCH(unit, IPIPE_BLOCK(unit));
    schan_msg.l3x2.header.cos = banks & 0x3;
    schan_msg.l3x2.header.datalen = entry_dw * 4;

    /* Fill in entry data */
    sal_memcpy(schan_msg.l3x2.data, entry, entry_dw * 4);

    /* Execute S-Channel operation  */
    rv = soc_schan_op(unit, &schan_msg, entry_dw + 1, entry_dw + 2, 1);

    if (schan_msg.readresp.header.opcode != L3_INSERT_DONE_MSG) {
        soc_cm_debug(DK_ERR,
                     "soc_fb_l3x_insert: invalid S-Channel reply, "
                     "expected L3_INSERT_DONE_MSG:\n");
        soc_schan_dump(unit, &schan_msg, 1);
        return SOC_E_INTERNAL;
    }

    if ((schan_msg.readresp.header.cpu) || (rv == SOC_E_FAIL)) {
        if (soc_feature(unit, soc_feature_l3x_parity)) {
            int perr_pbm_pos;
            int perr_index; 
            uint32 perr_pbm;
            if (SOC_IS_RAVEN(unit) && mem == L3_ENTRY_IPV6_MULTICASTm) {
                perr_index = entry_dw;
                perr_pbm_pos = _shr_popcount(SOC_MEM_INFO(unit, mem).index_max) -
                    soc_mem_entry_bits(unit, mem) / 32;
            } else {
                perr_index = entry_dw - 1;
                perr_pbm_pos = SOC_L3X_PERR_PBM_POS(unit, mem);
            }
            perr_pbm = ((schan_msg.readresp.data[perr_index] >>
                         perr_pbm_pos) & (SOC_L3X_BUCKET_SIZE(unit) - 1));
            if (perr_pbm) {
                uint32 index;
                int    nbits;

                nbits = soc_mem_entry_bits(unit, mem) % 32;
                if (nbits == 0) {
                    nbits = 32;
                }
                index = (schan_msg.readresp.data[perr_index] >> nbits) &
                               ((1 << (32 - nbits)) - 1);
                index |= (schan_msg.readresp.data[perr_index + 1] << (32 - nbits)) &
                         soc_mem_index_max(unit, mem); /* Assume size of table 2^N */
                soc_cm_debug(DK_ERR,
                    "Insert table[L3_ENTRY_XXX]: Parity Error Index %d Bucket Bitmap 0x%08x\n",
                     index,
                    (schan_msg.readresp.data[perr_index] >> perr_pbm_pos) & 
                        (SOC_L3X_BUCKET_SIZE(unit) - 1));
                return SOC_E_INTERNAL;
            }
        }
        soc_cm_debug(DK_SOCMEM,
            "Insert table[L3_ENTRY_XXX]: hash bucket full\n");
        rv = SOC_E_FULL;
    }

    return rv;
}

/*
 * Function:
 *	soc_fb_l3x_bank_delete
 * Purpose:
 *	Delete an entry from the L3X hash table.
 * Parameters:
 *	unit - StrataSwitch unit #
 *      banks - For dual hashing, which halves are selected (inverted)
 *	entry - L3X entry to delete
 * Returns:
 *	SOC_E_NONE - success
 * Notes:
 *	Uses hardware deletion; sends an L3 DELETE message over the
 *	S-Channel.  The hardware needs the L3_ENTRY_XXX entry type.
 */

int
soc_fb_l3x_bank_delete(int unit, uint8 banks,
                  l3_entry_ipv6_multicast_entry_t *entry)
{
    schan_msg_t         schan_msg;
    int			rv;
    int                 entry_dw;
    soc_mem_t           mem;
    int entry_size;

    SOC_IF_ERROR_RETURN(_soc_l3x_entry_mem_view_get(unit,
        L3_ENTRY_IPV4_UNICASTm, (uint32 *) entry, &mem, &entry_size));
    if (INVALIDm == mem) {
        return SOC_E_PARAM;
    }

    entry_dw = soc_mem_entry_words(unit, mem);

    if (soc_cm_debug_check(DK_SOCMEM)) {
	soc_cm_debug(DK_SOCMEM, "Delete table[L3_ENTRY_XXXm]: ");
	soc_mem_entry_dump(unit, mem, entry);
	soc_cm_debug(DK_SOCMEM, "\n");
    }

    schan_msg_clear(&schan_msg);
    schan_msg.l3x2.header.opcode = L3_DELETE_CMD_MSG;
    schan_msg.l3x2.header.srcblk = SOC_BLOCK2SCH(unit, CMIC_BLOCK(unit));
    schan_msg.l3x2.header.dstblk = SOC_BLOCK2SCH(unit, IPIPE_BLOCK(unit));
    schan_msg.l3x2.header.cos = banks & 0x3;
    schan_msg.l3x2.header.datalen = entry_dw * 4;

    /* Fill in packet data */
    sal_memcpy(schan_msg.l3x2.data, entry, entry_dw * 4);

    /* Execute S-Channel operation */
    rv = soc_schan_op(unit, &schan_msg, entry_dw + 1, entry_dw + 2, 1);

    if (schan_msg.readresp.header.opcode != L3_DELETE_DONE_MSG) {
        soc_cm_debug(DK_ERR,
                     "soc_fb_l3x_delete: invalid S-Channel reply, "
                     "expected L3_DELETE_DONE_MSG:\n");
        soc_schan_dump(unit, &schan_msg, 1);
        return SOC_E_INTERNAL;
    }

    if ((schan_msg.readresp.header.cpu) || (rv == SOC_E_FAIL)) {
        if (soc_feature(unit, soc_feature_l3x_parity)) {
            int perr_pbm_pos;
            int perr_index; 
            uint32 perr_pbm;
            if (SOC_IS_RAVEN(unit) && mem == L3_ENTRY_IPV6_MULTICASTm) {
                perr_index = entry_dw;
                perr_pbm_pos = _shr_popcount(SOC_MEM_INFO(unit, mem).index_max) -
                    soc_mem_entry_bits(unit, mem) / 32;
            } else {
                perr_index = entry_dw - 1;
                perr_pbm_pos = SOC_L3X_PERR_PBM_POS(unit, mem);
            }
            perr_pbm = ((schan_msg.readresp.data[perr_index] >>
                         perr_pbm_pos) & (SOC_L3X_BUCKET_SIZE(unit) - 1));
            if (perr_pbm) {
                uint32 index;
                int    nbits;

                nbits = soc_mem_entry_bits(unit, mem) % 32;
                if (nbits == 0) {
                    nbits = 32;
                }
                index = (schan_msg.readresp.data[perr_index] >> nbits) &
                               ((1 << (32 - nbits)) - 1);
                index |= (schan_msg.readresp.data[perr_index + 1] << (32 - nbits)) &
                         soc_mem_index_max(unit, mem); /* Assume size of table 2^N */
                soc_cm_debug(DK_ERR,
                    "Delete table[L3_ENTRYm]: Parity Error Index %d Bucket Bitmap 0x%08x\n",
                     index,
                    (schan_msg.readresp.data[perr_index] >> perr_pbm_pos) & 
                        (SOC_L3X_BUCKET_SIZE(unit) - 1) );
                rv = SOC_E_INTERNAL;
            }
        }
        soc_cm_debug(DK_SOCMEM,
            "Delete table[L3_ENTRYm]: Not found\n");
    }

    return rv;
}

/*
 * Function:
 *      _soc_l3x_compare_size
 * Purpose:
 *      Compare two entries in l3x mem bucket by number of 
 *      base slots they occupy.
 * Parameters:
 *      b - (IN)first compared entry. 
 *      a - (IN)second compared entry. 
 * Returns:
 *      a<=>b
 */
STATIC INLINE int
_soc_l3x_compare_size(void *a, void *b)
{
    soc_mem_l3x_entry_info_t *first;     /* First compared entry.  */
    soc_mem_l3x_entry_info_t *second;    /* Second compared entry. */

    /* Cast group info pointers. */
    first = (soc_mem_l3x_entry_info_t *)a;
    second = (soc_mem_l3x_entry_info_t *)b;

    if (first->size > second->size) {
        return -1;
    } else if (first->size < second->size) {
        return 1;
    }
    return 0;
}

/*
 * Function:
 *      _soc_l3x_compare_index
 * Purpose:
 *      Compare two entries in l3x mem bucket by index 
 *      in the bucket
 * Parameters:
 *      b - (IN)first compared entry. 
 *      a - (IN)second compared entry. 
 * Returns:
 *      a<=>b
 */
STATIC INLINE int
_soc_l3x_compare_index(void *a, void *b)
{
    soc_mem_l3x_entry_info_t *first;     /* First compared entry.  */
    soc_mem_l3x_entry_info_t *second;    /* Second compared entry. */

    /* Cast group info pointers. */
    first = (soc_mem_l3x_entry_info_t *)a;
    second = (soc_mem_l3x_entry_info_t *)b;

    if (first->index > second->index) {
        return 1;
    } else if (first->index < second->index) {
        return -1;
    }
    return 0;
}





/*
 * Function:
 *	_soc_l3x_mem_bucket_map_entry_shift
 * Purpose:
 *  	Shift entry in the bucket map to a new index.
 * Parameters: 
 *      unit      -  (IN) SOC device number.
 *      bkt_map   -  (IN) Bucket L3x Entry map.
 *      idx_src   -  (IN) Source entry index.
 *      idx_dst   -  (IN) Destination entry index. 
 * Returns:
 *     SOC_E_XXX
 *
 */

STATIC int
_soc_l3x_mem_bucket_map_entry_shift(int unit, soc_mem_l3x_bucket_map_t *bkt_map,
                                    int idx_src, int idx_dst)
{
    soc_mem_l3x_entry_info_t bmap_entry;
    int map_idx;
    
    /* Input parameters check. */
    if (NULL == bkt_map) {
        return (SOC_E_PARAM);
    }

    /* Index sanity. */
    if((0 > idx_src) || (idx_src > bkt_map->size) || (0 > idx_dst) || 
       (idx_dst > bkt_map->size) || (NULL == bkt_map)) {
        return (SOC_E_PARAM);
    }

    if (idx_src == idx_dst) {
        return (SOC_E_NONE);
    }  

    /* Preserve shifted entry. */
    sal_memcpy(&bmap_entry, bkt_map->entry_arr + idx_src, 
               sizeof(soc_mem_l3x_entry_info_t));

    /* Shift bucket map 1 entry down. */
    for (map_idx = idx_src; (map_idx - 1) >= idx_dst; map_idx--) {
        sal_memcpy(bkt_map->entry_arr + map_idx,
                   bkt_map->entry_arr + map_idx - 1,
                   sizeof(soc_mem_l3x_entry_info_t));
    }

    /* Reinsert entry to a new location. */
    sal_memcpy(bkt_map->entry_arr + idx_dst, &bmap_entry,
               sizeof(soc_mem_l3x_entry_info_t));

    return (SOC_E_NONE);
}


/*
 * Function:
 *	_soc_l3x_mem_bucket_entry_shift
 * Purpose:
 *  	Shift entry in bucket to a different index in the bucket.
 * Parameters: 
 *      unit      -  (IN) SOC device number.
 *      mem       -  (IN) Entry memory.
 *      idx_src   -  (IN) Source entry index. 
 *      idx_dst   -  (IN) Destination entry index. 
 * Returns:
 *     SOC_E_XXX
 *
 */

STATIC int
_soc_l3x_mem_bucket_entry_shift(int unit, soc_mem_t mem, 
                                int idx_src, int idx_dst)
{
    uint32 entry[SOC_MAX_MEM_WORDS]; /* Hw entry buffer.         */ 
    int rv;                          /* Operation return status. */

    soc_mem_lock(unit, mem);

    rv = soc_mem_read(unit, mem, MEM_BLOCK_ANY, idx_src, entry);
    if (SOC_FAILURE(rv)) {
        soc_mem_unlock(unit, mem);
        return rv;
    }

    rv = soc_mem_write(unit, mem, MEM_BLOCK_ALL, idx_dst, entry);
    if (SOC_FAILURE(rv)) {
        soc_mem_unlock(unit, mem);
        return rv;
    }

    rv = soc_mem_write(unit, mem, MEM_BLOCK_ALL, idx_src, 
                       soc_mem_entry_null(unit, mem));
    soc_mem_unlock(unit, mem);
    return rv;
}

/*
 * Function:
 *	_soc_l3x_mem_bucket_pack
 * Purpose:
 *  	Pack l3x table bucket, to allow wide entry insertion. 
 * Parameters: 
 *      unit         -  (IN) SOC device number.
 *      buffer       -  (IN) Bucket L3x entries.  
 *      bkt_map      -  (IN) Bucket L3x Entry map inside the buffer. 
 *      num_entries  -  (IN) Number of entries caller attempts to insert.
 * Returns:
 *     SOC_E_XXX
 *
 */

STATIC int
_soc_l3x_mem_bucket_pack(int unit, uint32 *buffer,
                         soc_mem_l3x_bucket_map_t *bkt_map, int num_entries)
{
    soc_mem_l3x_entry_info_t *entry;      /* Bucket map entry.         */
    int bucket_idx;         /* Bucket iteration index.                 */
    int map_idx;            /* Installed entries iteration index.      */
    int lkup_idx;           /* Gap fill entry lookup index.            */
    int free_slot_size;     /* Number of free entries in base memory.  */
    int idx_src;            /* Shifted entry source.                   */
    int idx_dst;            /* Shifted entry destination.              */
    int packed;             /* At least 1 entry was moved to an empty slot.  */
    int pack_candidate;     /* At least 1 entry fits to an empty slot. */


    /* Input parameters check. */
    if ((NULL == buffer) || (NULL == bkt_map)) {
        return (SOC_E_PARAM);
    }

    /* Sort entries by index in the bucket. */
    _shr_sort(bkt_map->entry_arr, bkt_map->size, 
              sizeof(soc_mem_l3x_entry_info_t), _soc_l3x_compare_index);

    /* Iterate over base entries in the bucket. */
    /* Stop 1) at the end of the bucket. 2) No more installed entries. */
    for (bucket_idx = 0, map_idx = 0; 
         ((bucket_idx < bkt_map->total_count) && (map_idx < bkt_map->size));) {

        entry = bkt_map->entry_arr + map_idx;

        /* Get free slot size. */
        free_slot_size = entry->index - bucket_idx;

        /* No gap move to the next entry. */
        if (0 == free_slot_size) {
            bucket_idx += entry->size; 
            map_idx++;
            continue;
        }

        /* Bucket has enough room for the inserted entry.*/
        if (free_slot_size >= num_entries) {
            if ((!bucket_idx) || ((bucket_idx >= num_entries) &&
                                  (0 == (bucket_idx % num_entries)))) {
                return (SOC_E_NONE);
            }
        }

        /* Fill the gap. */
        while (free_slot_size)  {
            pack_candidate = FALSE;   
            packed = FALSE;   
            /* Search for entry fitting into the free slot. */
            for (lkup_idx = bkt_map->size - 1; lkup_idx >= map_idx; lkup_idx--) {
                entry = bkt_map->entry_arr + lkup_idx;

                /* Check if candidate fits into the free slot. */ 
                if(entry->size <= free_slot_size) {
                    pack_candidate = TRUE;   
                    /* Check candidate allignment restrictions. */
                    if (((!bucket_idx) || ((bucket_idx >= entry->size) &&
                                           (0 == (bucket_idx % entry->size))))) {

                        idx_src = (bkt_map->base_idx + entry->index)/entry->size;
                        idx_dst = (bkt_map->base_idx + bucket_idx)/entry->size;

                        /* Shift hw entry. */
                        SOC_IF_ERROR_RETURN
                            (_soc_l3x_mem_bucket_entry_shift(unit, entry->mem, 
                                                             idx_src, idx_dst));

                        /* Map entry new index is free slot start index. */
                        entry->index = bucket_idx;

                        /* Free slot start index moved down by entry size. */
                        bucket_idx += entry->size;

                        /* Free slot size is decreased by entry size. */
                        free_slot_size -= entry->size; 

                        /* Swap entries in backet map to keep order. */
                        SOC_IF_ERROR_RETURN 
                            (_soc_l3x_mem_bucket_map_entry_shift(unit, bkt_map, 
                                                                 lkup_idx, map_idx));
                        /* Move to the next entry in the bucket. */
                        map_idx++;
                        packed = TRUE;
                        break;
                    }
                }
            }
            if (FALSE == packed) {
                /* There is no entry fitting into the slot move on. */
                bucket_idx += (pack_candidate) ? 1 : free_slot_size;
                free_slot_size -= (pack_candidate) ? 1 : free_slot_size;
            }
        }
    }
    return (SOC_E_NONE);
}
/*
 * Function:
 *	_soc_l3x_mem_range_read
 * Purpose:
 *  	Read l3x table into allocated buffer.     
 * Parameters: 
 *      unit         -  (IN) SOC device number.
 *      base_mem     -  (IN) Base memory name. 
 *      base_index   -  (IN) Bucket start index.    
 *      ent_count    -  (IN) Number of entries to read.
 *      buffer       -  (OUT) Allocated pointer filled with entries.  
 *      bucket_map   -  (OUT) Entry map inside the buffer. 
 * Returns:
 *     SOC_E_XXX
 *
 */

STATIC int
_soc_l3x_mem_range_read (int unit,
                          soc_mem_t base_mem,     
                          int base_index,
                          uint8 ent_count, 
                          uint32 *buffer, 
                          soc_mem_l3x_bucket_map_t *bucket_map)
{
    uint32    *entry_ptr;    /* Entry read pointer.           */
    int       entry_size;    /* Resolved entry size.          */
    soc_mem_t entry_mem;     /* Resolved entry memory.        */
    int       counter;       /* Bucket map entries counter.   */   
    int       idx_max;       /* Last index to read.           */
    int       idx;           /* Iteration index.              */

    /* Input parameters check. */
    if ((NULL == buffer) || (!SOC_MEM_IS_VALID(unit, base_mem)) || 
        (!ent_count) || (NULL == bucket_map)) {
        return (SOC_E_PARAM);
    }

    /* Start/End indexes sanity. */
    idx_max = base_index + ent_count - 1;
    if ((base_index < soc_mem_index_min(unit, base_mem)) || 
        (idx_max > soc_mem_index_max(unit, base_mem))) {
        return (SOC_E_PARAM);
    }

    /* Read entries into the buffer. */
    SOC_IF_ERROR_RETURN (soc_mem_read_range(unit, base_mem, MEM_BLOCK_ANY,
                                            base_index, idx_max, buffer));

    /* Stack variables initialization. */
    entry_size = 0;
    counter = 0;

    /* Parse the buffer. */
    for (idx = 0; idx < ent_count; idx += entry_size) {
        entry_ptr = soc_mem_table_idx_to_pointer(unit, base_mem, uint32 *, \
                                                 buffer, idx);

        /* Get entry memory view & relative to base memory size. */
        SOC_IF_ERROR_RETURN(_soc_l3x_entry_mem_view_get(unit, base_mem, 
                                                        entry_ptr, &entry_mem,
                                                        &entry_size));

        /* Skip invalid entries. */
        if (INVALIDm == entry_mem) {
            continue;
        }

        /* Initializes bucket_map of valid entry. */
        bucket_map->entry_arr[counter].size = entry_size; 
        bucket_map->entry_arr[counter].mem = entry_mem;
        bucket_map->entry_arr[counter].flags |= SOC_MEM_L3X_ENTRY_VALID;
        /* Set entry absolute index in the base view. */
        bucket_map->entry_arr[counter].index = idx;
        /* Set number of valid entries in bucket. */ 
        bucket_map->valid_count += entry_size;
        /* Increment map size. */
        counter++;
    } 

    /* Set map size. */
    bucket_map->size = counter;
    bucket_map->base_idx = base_index;
    bucket_map->base_mem = base_mem;
    bucket_map->total_count = ent_count;

    return (SOC_E_NONE);
}


/*
 * Function:
 *	_soc_l3x_mem_bucket_pack_insert
 * Purpose:
 *      Pack l3x entries bucket & reattempt insert operation.
 * 
 *      If dual hash is disabled/not supported we still 
 *      might be able to insert wide entries after bucket
 *      packing. 
 * Parameters:
 *      uint       - (IN) BCM device number. 
 *      entry_data - (IN) Inserted entry data. 
 * Return: 
 *      SOC_E_XXX
 */

STATIC int
_soc_l3x_mem_bucket_pack_insert (int unit, void *entry_data)
{
    soc_mem_l3x_bucket_map_t bkt_map;    /* L3X bucket entries map.          */
    uint32 *buffer;                      /* L3X table bucket snapshot.       */
    int alloc_size;                      /* Allocation buffer size.          */
    int bucket_size;                     /* Number of entries in bucket.     */
    int bucket_index;                    /* Entry index in base L3X memory.  */
    soc_mem_t mem;                       /* Entry view memory.               */
    int num_entries;                     /* Number of base slots entry needs.*/
    int free_slots = 0;                  /* Number of free entries.          */
    int rv;                              /* Operation return status.         */

    SOC_IF_ERROR_RETURN
        (_soc_l3x_entry_mem_view_get(unit, L3_ENTRY_IPV4_UNICASTm,
                                     entry_data, &mem, &num_entries));

    if (INVALIDm == mem) {
        return (SOC_E_INTERNAL);
    }

    /* There is no reason to pack if entry needs a single slot. */
    if (num_entries == 1) {
        return (SOC_E_FULL);
    } 

    /* Stack variales initialization & memory allocations.*/
    sal_memset(&bkt_map, 0, sizeof (soc_mem_l3x_bucket_map_t)); 
    bucket_size = SOC_L3X_BUCKET_SIZE(unit);

    /* Calcualte memory required to create bucket snapshot. */
    alloc_size = bucket_size * 
        WORDS2BYTES(soc_mem_entry_words(unit, L3_ENTRY_IPV4_UNICASTm));

    /* Allocate buffer for snapshot. */
    buffer = soc_cm_salloc(unit, alloc_size, "L3X bucket image"); 
    if (NULL == buffer) {
        return (SOC_E_MEMORY);
    }
    /* Reset allocated buffer. */
    sal_memset(buffer, 0, alloc_size);


    /* Allocate buffer for valid entries map. */ 
    alloc_size = bucket_size * sizeof(soc_mem_l3x_entry_info_t);    
    bkt_map.entry_arr = sal_alloc(alloc_size, "L3X Entries Info");
    if (NULL == bkt_map.entry_arr) {
        soc_cm_sfree(unit, buffer);
        return (SOC_E_MEMORY);
    }
    sal_memset(bkt_map.entry_arr, 0, alloc_size); 

    /* Calculate entry hash value. */
    bucket_index = bucket_size * 
        soc_fb_l3x2_entry_hash(unit, (uint32 *)entry_data);

    /* Read all entries in the bucket. */
    rv = _soc_l3x_mem_range_read(unit, L3_ENTRY_IPV4_UNICASTm, 
                                 bucket_index, bucket_size, 
                                 buffer, &bkt_map);
    if (SOC_FAILURE(rv)) {
        soc_cm_sfree(unit, buffer);
        sal_free(bkt_map.entry_arr);
        return rv;
    }
    free_slots  = bucket_size - bkt_map.valid_count;
    /* Return E_FULL if failed to free the space. */
    if (free_slots < num_entries) {
        soc_cm_sfree(unit, buffer);
        sal_free(bkt_map.entry_arr);
        return (SOC_E_FULL);
    }

    /* Pack entries in the bucket & reinsert original one.*/
    rv = _soc_l3x_mem_bucket_pack(unit, buffer, &bkt_map, num_entries);
    if (SOC_FAILURE(rv)) {
        soc_cm_sfree(unit, buffer);
        sal_free(bkt_map.entry_arr);
        return rv;
    }

    /* Insert orignal entry. . */ 
    rv =  soc_fb_l3x_bank_insert(unit, 0, entry_data);

    /* Free allocated resources. */ 
    soc_cm_sfree(unit, buffer);
    sal_free(bkt_map.entry_arr);
    return rv;
}

#if defined(BCM_FIREBOLT2_SUPPORT) || defined(BCM_TRX_SUPPORT) || \
    defined(BCM_RAVEN_SUPPORT)
/*
 * Function:
 *      _soc_l3x_mem_moved_entry_flush
 * Purpose:
 *      Remove moved entries from the bucket
 * Parameters: 
 *      unit         -  (IN) SOC device number.
 *      bucket_map   -  (IN) Bucket L3x Entry map inside the buffer. 
 * Returns:
 *     SOC_E_XXX
 *
 */

STATIC int
_soc_l3x_mem_moved_entry_flush(int unit, soc_mem_l3x_bucket_map_t *bucket_map)
{
    soc_mem_l3x_entry_info_t *entry;     /* Bucket map entry.           */
    int entry_index;                     /* Entry index in a view.      */
    int idx;                             /* Bucket iteration index.     */
    int map_idx;                         /* Bucket map iteration index. */

    /* Input parameters check. */
    if (NULL == bucket_map) {
        return (SOC_E_PARAM);
    }

    for (idx = 0; idx < bucket_map->size; idx++)  {
        entry = bucket_map->entry_arr + idx;
        if (entry->flags & SOC_MEM_L3X_ENTRY_MOVED) {
            entry_index = (entry->index + bucket_map->base_idx) / entry->size;

            SOC_IF_ERROR_RETURN
                (soc_mem_write(unit, entry->mem, SOC_BLOCK_ALL, entry_index,
                               soc_mem_entry_null(unit, entry->mem)));

            /* Pack bucket map. */
            for (map_idx = idx; (map_idx + 1) < bucket_map->size; map_idx++) {
                sal_memcpy(bucket_map->entry_arr + map_idx,
                           bucket_map->entry_arr + map_idx + 1,
                           sizeof(soc_mem_l3x_entry_info_t));
            }

            bucket_map->size--;
            bucket_map->valid_count -= entry->size;
            /* Reset last entry in the map. */
            sal_memset(bucket_map->entry_arr + map_idx, 0, 
                       sizeof(soc_mem_l3x_entry_info_t));
            idx--;
        }
    }
    return (SOC_E_NONE);
}

/*
 * Function:
 *	_soc_l3x_mem_dual_hash_move
 * Purpose:
 *	Recursive move routine for dual hash auto-move inserts
 *      Assumes feature already checked, mem locks taken.
 * Parameters:
 *      uint          - (IN) BCM device number. 
 *      mem           - (IN) Inserted entry memory. 
 *      banks         - (IN) Destination banks to insert the entry. 
 *      entry_data    - (IN) Inserted entry data. 
 *      hash_info     - (IN) Hash select information. 
 *      num_entries   - (IN) Inserted entry width. 
 *      bucket_trace  - (IN) Trace of buckets affected by recursion.
 *      recurse_depth - (IN) Maximum recursion depth. 
 * Returns: 
 *      SOC_E_XXX
 * Notes:
 *      The ugliness here is the XGS3 L3 table.  It contains 4 different
 *      L3 entry types in one hash structure.  A given logical entry
 *      may take up 1, 2, or 4 memory lines.  Since a different type
 *      might be blocking the entry to insert, we need to account
 *      for the complexity of moving multiple entries in that case.
 *      See further comments inline.
 */

STATIC int
_soc_l3x_mem_dual_hash_move(int unit,
                        soc_mem_t mem,     /* Assumes memory locked */
                        uint8 banks,
                        void *entry_data,
                        dual_hash_info_t *hash_info, 
                        int num_entries,
                        SHR_BITDCL *bucket_trace, 
                        int recurse_depth)
{
    soc_mem_l3x_entry_info_t *entry_arr; /* Bucket map entry array.          */
    soc_mem_l3x_entry_info_t *entry;     /* Bucket map entry.                */
    soc_mem_l3x_bucket_map_t bkt_map;    /* L3X bucket entries map.          */
    uint32 move_entry[SOC_MAX_MEM_WORDS];/* Entry moved to another bank.     */
    int that_bank_only;                  /* Destination bank.                */
    int this_bank_bit;                   /* Current bank id indicator        */
    int half_bucket;                     /* Number of entries in half bucket.*/
    int total_moved;                     /* Number of moved entries.         */
    int free_slots = 0;                  /* Number of free entries.          */
    int bucket_index = 0;                /* Entry index in base L3X memory.  */
    uint32 *buffer;                      /* L3X table bucket snapshot.       */
    int alloc_size;                      /* Allocation buffer size.          */
    int this_hash;                       /* Hash selection in current bank.  */
    int that_hash;                       /* Hash selection in other bank.    */
    int hash_base;                       /* Inserted entry hash value.       */ 
    int dest_hash_base;                  /* Moved entry hash value.          */ 
    int dest_bucket_index;               /* Moved entry index in base L3X.   */
    int bix;                             /* Bank iterator.                   */
    int idx;                             /* Bucket iteration index.          */
    int trace_size;                      /* Recursion trace array size.      */
    SHR_BITDCL *trace;                   /* Buckets involved in recursion.   */
    int rv = SOC_E_NONE;                 /* Operation return status.         */


    /* Maximum recursion depth reached check. */
    if (recurse_depth < 0) {
        return (SOC_E_FULL);
    }

    /* Stack variales initialization & memory allocations.*/
    half_bucket = hash_info->bucket_size / 2;

    /* Calcualte memory required to create bucket snapshot. */
    alloc_size = half_bucket * 
        WORDS2BYTES(soc_mem_entry_words(unit, hash_info->base_mem));

    /* Allocate buffer for snapshot. */
    buffer = soc_cm_salloc(unit, alloc_size, "L3X bucket image"); 
    if (NULL == buffer) {
        return (SOC_E_MEMORY);
    }
    /* Reset allocated buffer. */
    sal_memset(buffer, 0, alloc_size);


    /* Allocate buffer for valid entries map. */ 
    alloc_size = half_bucket * sizeof(soc_mem_l3x_entry_info_t);    
    entry_arr = sal_alloc(alloc_size, "L3X Entries Info");
    if (NULL == entry_arr) {
        soc_cm_sfree(unit, buffer);
        return (SOC_E_MEMORY);
    }

    /* Keep back trace of all buckets affected by recursion. */
    trace_size = 
        SHR_BITALLOCSIZE(soc_mem_index_count(unit, hash_info->base_mem));
    if (NULL == bucket_trace) {
        trace =  sal_alloc(trace_size, "Dual hash"); 
        if (NULL == trace) {
            sal_free(entry_arr);
            soc_cm_sfree(unit, buffer);
            return (SOC_E_MEMORY);
        }
    } else {
        trace = bucket_trace;  
    }

    /* Iterate over memory banks trying to free space for inserted entry.*/
    for (bix = 0; bix < 2; bix++) {

        if (bix == 0) {
            this_bank_bit =  SOC_MEM_HASH_BANK0_BIT;
            that_bank_only = SOC_MEM_HASH_BANK1_ONLY;
            this_hash = hash_info->hash_sel0;
            that_hash = hash_info->hash_sel1;
        } else {
            this_bank_bit = SOC_MEM_HASH_BANK1_BIT;
            that_bank_only = SOC_MEM_HASH_BANK0_ONLY;
            this_hash = hash_info->hash_sel1;
            that_hash = hash_info->hash_sel0;
        }

        if (banks & this_bank_bit) {
            /* Not this bank */
            continue;
        }

        /* Calculate entry hash value. */
        hash_base = soc_fb_l3x_entry_hash(unit, this_hash, entry_data);
        bucket_index = hash_base * hash_info->bucket_size + bix * half_bucket;

        /* Recursion trace initialization. */
        if (NULL == bucket_trace) {
            sal_memset(trace, 0, trace_size);
        }
        SHR_BITSET(trace, bucket_index);
        
        /* Reset bucket entries map.                     */
        /* Allocation size is preserved across the loop. */
        total_moved = 0;
        sal_memset(entry_arr, 0, alloc_size); 
        sal_memset(&bkt_map, 0, sizeof (soc_mem_l3x_bucket_map_t)); 
        bkt_map.entry_arr = entry_arr;
        /* Read all entries in the bucket. */
        rv = _soc_l3x_mem_range_read(unit, hash_info->base_mem, bucket_index,
                                     half_bucket, buffer, &bkt_map);
        if (SOC_FAILURE(rv)) {
            break;
        }

        free_slots  = half_bucket - bkt_map.valid_count;
        /* Check if bucket packing is sufficient. */
        if (free_slots >= num_entries) {
            break;
        }

        /*
         * Sort  entries by size. There is a greater chance to 
         * shift small entries vs large ones. 
         */
        _shr_sort(bkt_map.entry_arr, bkt_map.size, 
                  sizeof(soc_mem_l3x_entry_info_t), _soc_l3x_compare_size);


        /* Move entries from shorted to longest.           */
        /* There is a higher chance to move shorter entry. */
        for (idx = bkt_map.size - 1 ; idx >= 0; idx--) {

            entry = entry_arr + idx;
            rv = soc_mem_read(unit, entry->mem, MEM_BLOCK_ANY,
                              (entry->index + bkt_map.base_idx) / entry->size,
                              move_entry);
            if (SOC_FAILURE(rv)) {
                break;
            }

            /* Calculate destination entry hash value. */
            dest_hash_base = soc_fb_l3x_entry_hash(unit, that_hash, move_entry);
            dest_bucket_index = 
                dest_hash_base * hash_info->bucket_size + (!bix) * half_bucket;

            /* Make sure we are not touching buckets in bucket trace. */
            if(SHR_BITGET(trace, dest_bucket_index)) {
                continue;
            }

            /* Move entry to the other bank. */ 
            rv = soc_fb_l3x_bank_insert(unit, that_bank_only,
                                        (void *)move_entry);

            if (SOC_FAILURE(rv)) {
                if (rv != SOC_E_FULL) {
                    break;
                }
                /* Recursive call. */
                rv = _soc_l3x_mem_dual_hash_move(unit, entry->mem,
                                                 that_bank_only, 
                                                 move_entry, hash_info,
                                                 entry->size, 
                                                 trace, recurse_depth - 1);
            }

            if (SOC_SUCCESS(rv)) {
                total_moved++;
                free_slots += entry->size;
                entry->flags |= SOC_MEM_L3X_ENTRY_MOVED;
                entry->flags &= ~SOC_MEM_L3X_ENTRY_VALID;

                if (free_slots >= num_entries) {
                    break;       
                } 
            }
        }

        /* Remove moved entries. */
        if (total_moved) {
            rv = _soc_l3x_mem_moved_entry_flush(unit, &bkt_map);
            if (SOC_FAILURE(rv)) {
                soc_cm_sfree(unit, buffer);
                sal_free(entry_arr);
                if (NULL == bucket_trace) sal_free(trace);
                return rv;
            }
        }

        /* Sufficient space was freed to accomodate the entry. */
        if (free_slots >= num_entries) {
            break;       
        } 

        /* If any error happened stop. */
        if (SOC_FAILURE(rv) && (rv != SOC_E_FULL)) {
            break;
        }

    }  /* Loop over the banks. */

    /* Return if any error occured */
    if (SOC_FAILURE(rv)) {
        soc_cm_sfree(unit, buffer);
        sal_free(entry_arr);
        if (NULL == bucket_trace) sal_free(trace);
        return rv;
    }

    /* Return E_FULL if failed to free the space. */
    if (free_slots < num_entries) {
        soc_cm_sfree(unit, buffer);
        sal_free(entry_arr);
        if (NULL == bucket_trace) sal_free(trace);
        return (SOC_E_FULL);
    }

    /* If inserted entry size is > 1 pack the bucket. */
    if (num_entries > 1) {
        /* Pack entries in the bucket & reinsert original one.*/
        rv = _soc_l3x_mem_bucket_pack(unit, buffer, &bkt_map, num_entries);
        if (SOC_FAILURE(rv)) {
            soc_cm_sfree(unit, buffer);
            sal_free(entry_arr);
            if (NULL == bucket_trace) sal_free(trace);
            return rv;
        }
    }

    /* Insert orignal entry. . */ 
    rv = soc_fb_l3x_bank_insert(unit, banks, 
                                (l3_entry_ipv6_multicast_entry_t *)entry_data);
    /* Free allocated resources. */ 
    soc_cm_sfree(unit, buffer);
    sal_free(entry_arr);
    if (NULL == bucket_trace) sal_free(trace);
    return rv;
}

/*
 * Function:
 *	_soc_mem_l3x_dual_hash_insert
 * Purpose:
 *	Dual hash auto-move inserts
 * Parameters:
 *      uint          - (IN) BCM device number. 
 *      entry_data    - (IN) Inserted entry data. 
 *      recurse_depth - (IN) Maximum recursion depth. 
 * Returns: 
 *      SOC_E_XXX
 * 
 */

STATIC int
_soc_mem_l3x_dual_hash_insert (int unit,
                               void *entry_data,
                               int recurse_depth)
{
    dual_hash_info_t hash_info;  /* Hash selections.         */
    soc_mem_t    mem;            /* Entry memory.            */
    int rv;                      /* Operation return status. */
    int num_entries = 0;         /* Entry width.             */

    SOC_IF_ERROR_RETURN
        (_soc_l3x_entry_mem_view_get(unit, L3_ENTRY_IPV4_UNICASTm,
                                     entry_data, &mem, &num_entries));

    if (INVALIDm == mem) {
        return (SOC_E_INTERNAL);
    }

    rv = soc_fb_l3x_bank_insert(unit, 0,
                        (l3_entry_ipv6_multicast_entry_t *)entry_data);
    if (rv != SOC_E_FULL) {
        return rv;
    }

    SOC_IF_ERROR_RETURN
        (soc_fb_l3x_entry_bank_hash_sel_get(unit, 0,
                                            &(hash_info.hash_sel0)));
    SOC_IF_ERROR_RETURN
        (soc_fb_l3x_entry_bank_hash_sel_get(unit, 1,
                                            &(hash_info.hash_sel1)));
    if (hash_info.hash_sel0 == hash_info.hash_sel1) {
        /* Can't juggle the entries */
        return  _soc_l3x_mem_bucket_pack_insert(unit, entry_data);
    }

    hash_info.bucket_size = SOC_L3X_BUCKET_SIZE(unit);
    hash_info.base_mem = L3_ENTRY_IPV4_UNICASTm;

    /* Time to shuffle the entries */

    rv = _soc_l3x_mem_dual_hash_move(unit, mem, SOC_MEM_HASH_BANK_BOTH,
                                     entry_data, &hash_info, num_entries, 
                                     NULL, recurse_depth - 1);
    return (rv);
}
#endif /* BCM_FIREBOLT2_SUPPORT || BCM_RAVEN_SUPPORT || BCM_TRX_SUPPORT */

/*
 */

/*
 * Function:
 *	soc_fb_l3x_insert
 * Purpose:
 *   Original non-bank versions of the FB L3X table op functions
 * Parameters:
 *      uint         - (IN) BCM device number. 
 *      entry        - (IN) Inserted entry data. 
 * Returns: 
 *      SOC_E_XXX
 */
int
soc_fb_l3x_insert(int unit, l3_entry_ipv6_multicast_entry_t *entry)
{
    int rv;                /* Operation return status. */

    SOC_IF_ERROR_RETURN(soc_l3x_lock(unit));
#if defined(BCM_FIREBOLT2_SUPPORT) || defined(BCM_TRX_SUPPORT) || \
    defined(BCM_RAVEN_SUPPORT) 
    if (soc_feature(unit, soc_feature_dual_hash)) {
        rv = _soc_mem_l3x_dual_hash_insert(unit, (void *)entry,
                                           SOC_DUAL_HASH_MOVE_MAX_L3X(unit));
    }  else 
#endif /* BCM_FIREBOLT2_SUPPORT || BCM_RAVEN_SUPPORT || BCM_TRX_SUPPORT */
    {
        rv = soc_fb_l3x_bank_insert(unit, 0, entry);
        if (SOC_FAILURE(rv) && (SOC_E_FULL == rv)) {
            rv = _soc_l3x_mem_bucket_pack_insert (unit, entry);
        }
    }
    SOC_IF_ERROR_RETURN(soc_l3x_unlock(unit));
    return (rv);
}

int
soc_fb_l3x_delete(int unit, l3_entry_ipv6_multicast_entry_t *entry)
{
    return soc_fb_l3x_bank_delete(unit, 0, entry);
}

int
soc_fb_l3x_lookup(int unit, l3_entry_ipv6_multicast_entry_t *key,
                  l3_entry_ipv6_multicast_entry_t *result, int *index_ptr)
{
    return soc_fb_l3x_bank_lookup(unit, 0, key, result, index_ptr);
}
#endif /* BCM_FIREBOLT_SUPPORT */


#ifdef BCM_EASYRIDER_SUPPORT
/*
 * Function:
 *      soc_er_l3v4_lookup
 * Purpose:
 *      Use command memory op to search L3v4 tabls.
 * Parameters:
 *      unit - StrataSwitch unit #
 *      key - L3v4 entry to look up; only MAC+VLAN fields are relevant
 *      result - L3v4 entry to receive entire found entry
 *      index_ptr (OUT) - If found, receives table index where found
 * Returns:
 *      SOC_E_INTERNAL if retries exceeded or other internal error
 *      SOC_E_NOT_FOUND if the entry is not found.
 *      SOC_E_NONE (0) on success (entry found):
 * Notes:
 *      It is okay if the result pointer is the same as the key pointer.
 */

int
soc_er_l3v4_lookup(int unit, l3_entry_v4_entry_t *key,
                   l3_entry_v4_entry_t *result, int *index_ptr)
{
    soc_mem_cmd_t cmd_info;
    uint32        hash_addr;
    int           rv;

    sal_memset(&cmd_info, 0, sizeof(cmd_info));
    cmd_info.command = SOC_MEM_CMD_SEARCH;
    cmd_info.entry_data = key;
    rv = soc_mem_cmd_op(unit, L3_ENTRY_V4m, &cmd_info, FALSE);
    if (rv == SOC_E_NOT_FOUND) {
        *index_ptr = -1;
    } else {
        sal_memcpy(result, cmd_info.output_data,
                   soc_mem_entry_words(unit, L3_ENTRY_V4m) *
                   sizeof (uint32));
        hash_addr = cmd_info.addr0;
        hash_addr &= SOC_MEM_CMD_HASH_ADDR_MASK;
        *index_ptr = hash_addr -
            SOC_PERSIST(unit)->er_memcfg.l3v4_search_offset;
    }
    return rv;
}

/*
 * Function:
 *      soc_er_l3v6_lookup
 * Purpose:
 *      Use command memory op to search L3v6 tabls.
 * Parameters:
 *      unit - StrataSwitch unit #
 *      key - L3v6 entry to look up; only MAC+VLAN fields are relevant
 *      result - L3v6 entry to receive entire found entry
 *      index_ptr (OUT) - If found, receives table index where found
 * Returns:
 *      SOC_E_INTERNAL if retries exceeded or other internal error
 *      SOC_E_NOT_FOUND if the entry is not found.
 *      SOC_E_NONE (0) on success (entry found):
 * Notes:
 *      It is okay if the result pointer is the same as the key pointer.
 */

int
soc_er_l3v6_lookup(int unit, l3_entry_v6_entry_t *key,
                   l3_entry_v6_entry_t *result, int *index_ptr)
{
    soc_mem_cmd_t cmd_info;
    uint32        hash_addr, temp_hash_addr;
    int           rv;

    sal_memset(&cmd_info, 0, sizeof(cmd_info));
    cmd_info.command = SOC_MEM_CMD_SEARCH;
    cmd_info.entry_data = key;
    rv = soc_mem_cmd_op(unit, L3_ENTRY_V6m, &cmd_info, FALSE);
    if (rv == SOC_E_NOT_FOUND) {
        *index_ptr = -1;
    } else {
        sal_memcpy(result, cmd_info.output_data,
                   soc_mem_entry_words(unit, L3_ENTRY_V6m) *
                   sizeof (uint32));
        hash_addr = cmd_info.addr0;

        hash_addr &= SOC_MEM_CMD_HASH_ADDR_MASK;

        /* reduced hash bucket size adjustment */
        temp_hash_addr = (hash_addr & SOC_MEM_CMD_EXT_L2_ADDR_MASK_HI);
        temp_hash_addr >>= SOC_MEM_CMD_EXT_L2_ADDR_SHIFT_HI;
        temp_hash_addr |= (hash_addr & SOC_MEM_CMD_EXT_L2_ADDR_MASK_LO);

        *index_ptr = temp_hash_addr -
            SOC_PERSIST(unit)->er_memcfg.l3v6_search_offset;
    }
    return rv;
}
#endif /* BCM_EASYRIDER_SUPPORT */

#endif /* BCM_XGS_SWITCH_SUPPORT */
#endif  /* INCLUDE_L3 */