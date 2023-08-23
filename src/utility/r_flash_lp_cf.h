/*
  Copyright (c) 2023 Juraj Andrassy

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef R_FLASH_LP_CF_LOW_H
#define R_FLASH_LP_CF_LOW_H

#include <r_flash_lp.h>

/* The functions r_flash_lp_cf_erase and r_flash_lp_cf_write are static
 * (private) in Renesas FSP framework. Even if they were accessible,
 * Arduino pre-builds the FSP into variants/UNOWIFIR4/libs/libfsp.a with
 * #define FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE 0
 * so these functions are not available in the Arduino core.
 *
 * The r_flash_lp_cf.c file is a reduced version of the FSP r_flash_lp.c file.
 * It contains the required functions to erase and write the code flash.
 * All the functions are located in RAM. Some of them are duplicated,
 * but their versions from r_flash_lp.c are not put in RAM, because
 * data flash operations don't require it.
 */

#undef FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE
#define FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE 1

#undef PLACE_IN_RAM_SECTION

#if defined(__ICCARM__)
#pragma section=".code_in_ram"
#endif
#if defined(__ARMCC_VERSION) || defined(__GNUC__)
#define PLACE_IN_RAM_SECTION    __attribute__((noinline)) BSP_PLACE_IN_SECTION(".code_in_ram")
#else
#define PLACE_IN_RAM_SECTION    BSP_PLACE_IN_SECTION(".code_in_ram")
#endif

  fsp_err_t r_flash_lp_cf_erase(flash_lp_instance_ctrl_t * const p_ctrl,
                                uint32_t                         block_address,
                                uint32_t                         num_blocks,
                                uint32_t                         block_size) PLACE_IN_RAM_SECTION;

  fsp_err_t r_flash_lp_cf_write(flash_lp_instance_ctrl_t * const p_ctrl,
                                uint32_t const                   src_start_address,
                                uint32_t                         dest_start_address,
                                uint32_t                         num_bytes) PLACE_IN_RAM_SECTION;


#endif
