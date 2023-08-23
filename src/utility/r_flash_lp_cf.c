/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

#ifdef ARDUINO_ARCH_RENESAS_UNO

#include "r_flash_lp_cf.h"

/* Roughly 4 cycles per loop */
#define FLASH_LP_DELAY_LOOP_CYCLES                    4U

/*  flash mode definition (FENTRYR Register setting)*/
#define FLASH_LP_FENTRYR_DATAFLASH_PE_MODE            (0xAA80U)
#define FLASH_LP_FENTRYR_CODEFLASH_PE_MODE            (0xAA01U)
#define FLASH_LP_FENTRYR_READ_MODE                    (0xAA00U)

/*  flash mode definition (FPMCR Register setting)*/
#define FLASH_LP_DATAFLASH_PE_MODE                    (0x10U)
#define FLASH_LP_READ_MODE                            (0x08U)
#define FLASH_LP_LVPE_MODE                            (0x40U)
#define FLASH_LP_DISCHARGE_1                          (0x12U)
#define FLASH_LP_DISCHARGE_2                          (0x92U)
#define FLASH_LP_CODEFLASH_PE_MODE                    (0x82U)

/*  operation definition (FCR Register setting)*/
#define FLASH_LP_FCR_WRITE                            (0x81U)
#define FLASH_LP_FCR_ERASE                            (0x84U)
#define FLASH_LP_FCR_BLANKCHECK                       (0x83U)
#define FLASH_LP_FCR_CLEAR                            (0x00U)

/* Wait Process definition */
#define FLASH_LP_WAIT_TDIS                            (3U)
#define FLASH_LP_WAIT_TMS_MID                         (4U)
#define FLASH_LP_WAIT_TMS_HIGH                        (6U)
#define FLASH_LP_WAIT_TDSTOP                          (6U)

#define FLASH_LP_FENTRYR_DF_PE_MODE                   (0x0080U)
#define FLASH_LP_FENTRYR_CF_PE_MODE                   (0x0001U)
#define FLASH_LP_FENTRYR_PE_MODE_BITS                 (FLASH_LP_FENTRYR_DF_PE_MODE | FLASH_LP_FENTRYR_CF_PE_MODE)

#if BSP_FEATURE_FLASH_LP_VERSION == 4
 #define FLASH_LP_PRV_FENTRYR                         R_FACI_LP->FENTRYR_MF4
#else
 #define FLASH_LP_PRV_FENTRYR                         R_FACI_LP->FENTRYR
#endif

#define FLASH_LP_FSTATR2_ILLEGAL_ERROR_BITS           (0x10)
#define FLASH_LP_FSTATR2_ERASE_ERROR_BITS             (0x11)
#define FLASH_LP_FSTATR2_WRITE_ERROR_BITS             0x12

#define FLASH_LP_FPR_UNLOCK                           0xA5U

#define FLASH_LP_FRDY_CMD_TIMEOUT                     (19200)

#define FLASH_LP_REGISTER_WAIT_TIMEOUT(val, reg, timeout, err) \
    while (val != reg)                                         \
    {                                                          \
        if (0 == timeout)                                      \
        {                                                      \
            return err;                                        \
        }                                                      \
        timeout--;                                             \
    }


/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static fsp_err_t r_flash_lp_pe_mode_exit(flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

static void r_flash_lp_process_command(const uint32_t start_addr, uint32_t num_bytes,
                                       uint32_t command) PLACE_IN_RAM_SECTION;

static fsp_err_t r_flash_lp_command_finish(uint32_t timeout) PLACE_IN_RAM_SECTION;

static void r_flash_lp_delay_us(uint32_t us, uint32_t mhz) PLACE_IN_RAM_SECTION __attribute__((noinline));

static void r_flash_lp_reset(flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

static void r_flash_lp_write_fpmcr(uint8_t value) PLACE_IN_RAM_SECTION;

static fsp_err_t r_flash_lp_wait_for_ready(flash_lp_instance_ctrl_t * const p_ctrl,
                                           uint32_t                         timeout,
                                           uint32_t                         error_bits,
                                           fsp_err_t                        return_code) PLACE_IN_RAM_SECTION;

static void r_flash_lp_memcpy(uint8_t * const dest, uint8_t * const src, uint32_t len) PLACE_IN_RAM_SECTION;

static void r_flash_lp_cf_enter_pe_mode(flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

static void r_flash_lp_cf_write_operation(const uint32_t psrc_addr, const uint32_t dest_addr) PLACE_IN_RAM_SECTION;

/*******************************************************************************************************************//**
 * This function erases a specified number of Code Flash blocks
 *
 * @param[in]  p_ctrl                Pointer to the Flash control block
 * @param[in]  block_address         The starting address of the first block to erase.
 * @param[in]  num_blocks            The number of blocks to erase.
 * @param[in]  block_size            The Flash block size.
 *
 * @retval     FSP_SUCCESS           Successfully erased (non-BGO) mode or operation successfully started (BGO).
 * @retval     FSP_ERR_ERASE_FAILED  Status is indicating a Erase error.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 **********************************************************************************************************************/
fsp_err_t r_flash_lp_cf_erase (flash_lp_instance_ctrl_t * const p_ctrl,
                                      uint32_t                         block_address,
                                      uint32_t                         num_blocks,
                                      uint32_t                         block_size)
{
    fsp_err_t err = FSP_SUCCESS;

    r_flash_lp_cf_enter_pe_mode(p_ctrl);

    /* Select User Area */
    R_FACI_LP->FASR_b.EXS = 0U;

    /* Start the code flash erase operation. */
    r_flash_lp_process_command(block_address, num_blocks * block_size, FLASH_LP_FCR_ERASE);

    /* Wait for the operation to complete. */
    err = r_flash_lp_wait_for_ready(p_ctrl,
                                    p_ctrl->timeout_erase_cf_block * num_blocks,
                                    FLASH_LP_FSTATR2_ERASE_ERROR_BITS,
                                    FSP_ERR_ERASE_FAILED);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return r_flash_lp_pe_mode_exit(p_ctrl);
}

/*******************************************************************************************************************//**
 * This function writes a specified number of bytes to Code Flash.
 *
 * @param[in]  p_ctrl                Pointer to the Flash control block
 * @param[in]  src_start_address     The starting address of the first byte (from source) to write.
 * @param[in]  dest_start_address    The starting address of the Flash (to destination) to write.
 * @param[in]  num_bytes             The number of bytes to write.
 *
 * @retval     FSP_SUCCESS           Data successfully written (non-BGO) mode or operation successfully started (BGO).
 * @retval     FSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation. This may be
 *                                   returned if the requested Flash area is not blank.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the Flash sequencer to become ready.
 **********************************************************************************************************************/
fsp_err_t r_flash_lp_cf_write (flash_lp_instance_ctrl_t * const p_ctrl,
                                      uint32_t const                   src_start_address,
                                      uint32_t                         dest_start_address,
                                      uint32_t                         num_bytes)
{
    fsp_err_t err = FSP_SUCCESS;

    r_flash_lp_cf_enter_pe_mode(p_ctrl);

    p_ctrl->dest_end_address     = dest_start_address;
    p_ctrl->source_start_address = src_start_address;

    /* Calculate the number of writes needed. */

    /* This is done with right shift instead of division to avoid using the division library, which would be in flash
     * and cause a jump from RAM to flash. */
    p_ctrl->operations_remaining = num_bytes / BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE;

    /* Select User Area */
    R_FACI_LP->FASR_b.EXS = 0U;

    while (p_ctrl->operations_remaining && (FSP_SUCCESS == err))
    {
        /* Initiate the code flash write operation. */
        r_flash_lp_cf_write_operation(p_ctrl->source_start_address, p_ctrl->dest_end_address);

        /* If there is more data to write then write the next data. */
        p_ctrl->source_start_address += BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE;
        p_ctrl->dest_end_address     += BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE;
        p_ctrl->operations_remaining--;
        err = r_flash_lp_wait_for_ready(p_ctrl,
                                        p_ctrl->timeout_write_cf,
                                        FLASH_LP_FSTATR2_WRITE_ERROR_BITS,
                                        FSP_ERR_WRITE_FAILED);
    }

    /* If successful exit P/E mode. */
    if (FSP_SUCCESS == err)
    {
        r_flash_lp_pe_mode_exit(p_ctrl);
    }

    return err;
}

/*******************************************************************************************************************//**
 * Execute a single Write operation on the Low Power Code Flash data.
 * See Figure 37.19 in Section 37.13.3 of the RA2L1 manual r01uh0853ej0100-ra2l1
 *
 * @param[in]  psrc_addr         Source address for data to be written.
 * @param[in]  dest_addr         End address (read form) for writing.
 **********************************************************************************************************************/
static void r_flash_lp_cf_write_operation (const uint32_t psrc_addr, const uint32_t dest_addr)
{
    uint16_t data[BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE / 2];

    /* Write flash address setting */
    R_FACI_LP->FSARH = (uint16_t) ((dest_addr >> 16));
    R_FACI_LP->FSARL = (uint16_t) (dest_addr);

    /* Copy the data and write them to the flash write buffers. CM23 parts to not support unaligned access so this
     * must be done using byte access. */
    r_flash_lp_memcpy((uint8_t *) data, (uint8_t *) psrc_addr, BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE);
    R_FACI_LP->FWBL0 = data[0];
    R_FACI_LP->FWBH0 = data[1];
 #if BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE == 8
    R_FACI_LP->FWBL1 = data[2];
    R_FACI_LP->FWBH1 = data[3];
 #endif

    /* Execute Write command */
    R_FACI_LP->FCR = FLASH_LP_FCR_WRITE;
}

/*******************************************************************************************************************//**
 * Initiates a flash command.
 * See Figures 37.23, 37.24, 37.26 and 37.27 in Section 37.13.3 of the RA2L1 manual r01uh0853ej0100-ra2l1
 *
 * @param[in]  start_addr      Start address of the operation.
 * @param[in]  num_bytes       Number of bytes beginning at start_addr.
 * @param[in]  command         The flash command
 **********************************************************************************************************************/
static void r_flash_lp_process_command (const uint32_t start_addr, uint32_t num_bytes, uint32_t command)
{
    uint32_t end_addr_idx = start_addr + (num_bytes - 1U);

    /* Select User Area */
    R_FACI_LP->FASR_b.EXS = 0U;

    /* BlankCheck start address setting */
    R_FACI_LP->FSARH = (uint16_t) ((start_addr >> 16));
    R_FACI_LP->FSARL = (uint16_t) (start_addr);

    /* BlankCheck end address setting */
    R_FACI_LP->FEARH = ((end_addr_idx >> 16));
    R_FACI_LP->FEARL = (uint16_t) (end_addr_idx);

    /* Execute BlankCheck command */
    R_FACI_LP->FCR = (uint8_t) command;
}

/*******************************************************************************************************************//**
 * This function switches the peripheral from P/E mode for Code Flash or Data Flash to Read mode.
 * See Figures 37.17 and 37.18 in Section 37.13.3 of the RA2L1 manual r01uh0853ej0100-ra2l1
 *
 * @param[in]  p_ctrl                    Pointer to the Flash control block
 * @retval     FSP_SUCCESS               Successfully entered P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for confirmation of transition to read mode
 **********************************************************************************************************************/
static fsp_err_t r_flash_lp_pe_mode_exit (flash_lp_instance_ctrl_t * const p_ctrl)
{
#if FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE
    uint32_t flash_pe_mode = FLASH_LP_PRV_FENTRYR;
#endif

    /* Timeout counter. */
    volatile uint32_t wait_cnt = FLASH_LP_FRDY_CMD_TIMEOUT;

#if BSP_FEATURE_FLASH_LP_VERSION == 3 && FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE
    if (flash_pe_mode == FLASH_LP_FENTRYR_CF_PE_MODE)
    {
        r_flash_lp_write_fpmcr(FLASH_LP_DISCHARGE_2);

        /* Wait for 2us over (tDIS) */
        r_flash_lp_delay_us(FLASH_LP_WAIT_TDIS, p_ctrl->system_clock_frequency);

        r_flash_lp_write_fpmcr(FLASH_LP_DISCHARGE_1);
    }
#endif
    r_flash_lp_write_fpmcr(FLASH_LP_READ_MODE);

    /* Wait for 5us over (tMS) */
    r_flash_lp_delay_us(FLASH_LP_WAIT_TMS_HIGH, p_ctrl->system_clock_frequency);

    /* Clear the P/E mode register */
    FLASH_LP_PRV_FENTRYR = FLASH_LP_FENTRYR_READ_MODE;

    /* Loop until the Flash P/E mode entry register is cleared or a timeout occurs. If timeout occurs return error. */
    FLASH_LP_REGISTER_WAIT_TIMEOUT(0, FLASH_LP_PRV_FENTRYR, wait_cnt, FSP_ERR_TIMEOUT);

#if FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE
    if (flash_pe_mode == FLASH_LP_FENTRYR_CF_PE_MODE)
    {
 #if BSP_FEATURE_BSP_FLASH_CACHE
        R_BSP_FlashCacheEnable();
 #endif
 #if BSP_FEATURE_BSP_FLASH_PREFETCH_BUFFER
        R_FACI_LP->PFBER = 1;
 #endif
    }
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * This function resets the Flash sequencer.
 * See Figure 37.19 in Section 37.13.3 of the RA2L1 manual r01uh0853ej0100-ra2l1
 *
 * @param[in]  p_ctrl          Pointer to the Flash control block
 **********************************************************************************************************************/
static void r_flash_lp_reset (flash_lp_instance_ctrl_t * const p_ctrl)
{
    /* Cancel any in progress BGO operation. */
    p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;

    /* If not currently in PE mode then enter P/E mode. */
    if (FLASH_LP_PRV_FENTRYR == 0x0000UL)
    {
        /* Enter P/E mode so that we can execute some FACI commands. Either Code or Data Flash P/E mode would work
         * but Code Flash P/E mode requires FLASH_LP_CFG_CODE_FLASH_PROGRAMMING_ENABLE ==1, which may not be true */
        r_flash_lp_cf_enter_pe_mode(p_ctrl);
    }

    /* Reset the flash. */
    R_FACI_LP->FRESETR_b.FRESET = 1U;
    R_FACI_LP->FRESETR_b.FRESET = 0U;

    /* Transition to Read mode */
    r_flash_lp_pe_mode_exit(p_ctrl);
}

/*******************************************************************************************************************//**
 * Delay for the given number of micro seconds at the given frequency
 *
 * @note This is used instead of R_BSP_SoftwareDelay because that may be linked in code flash.
 *
 * @param[in]  us              Number of microseconds to delay
 * @param[in]  mhz             The frequency of the system clock
 **********************************************************************************************************************/
static void r_flash_lp_delay_us (uint32_t us, uint32_t mhz)
{
    uint32_t loop_cnt;

    // @12 MHz, one loop is 332 ns. A delay of 5 us would require 15 loops. 15 * 332 = 4980 ns or ~ 5us

    /* Calculation of a loop count */
    loop_cnt = ((us * mhz) / FLASH_LP_DELAY_LOOP_CYCLES);

    if (loop_cnt > 0U)
    {
        __asm volatile ("delay_loop:\n"
#if defined(__ICCARM__) || defined(__ARMCC_VERSION)
                        "   subs %[loops_remaining], #1         \n"                 ///< 1 cycle
#elif defined(__GNUC__)
                        "   sub %[loops_remaining], %[loops_remaining], #1      \n" ///< 1 cycle
#endif
                        "cmp %[loops_remaining], #0\n"                              // 1 cycle

/* CM0 and CM23 have different instruction sets */
#if defined(__CORE_CM0PLUS_H_GENERIC) || defined(__CORE_CM23_H_GENERIC)
                        "   bne delay_loop   \n"                                    ///< 2 cycles
#else
                        "   bne.n delay_loop \n"                                    ///< 2 cycles
#endif
                        :                                                           // No outputs
                        :[loops_remaining] "r" (loop_cnt)
                        :                                                           // No clobbers
                        );
    }
}

/*******************************************************************************************************************//**
 * Sets the FPMCR register, used to place the Flash sequencer in Code Flash P/E mode.
 * @param[in]  value   - 8 bit value to be written to the FPMCR register.
 **********************************************************************************************************************/
static void r_flash_lp_write_fpmcr (uint8_t value)
{
    /* The procedure for writing to FPMCR is documented in Section 37.3.4 of the RA2L1 manual r01uh0853ej0100-ra2l1 */
    R_FACI_LP->FPR = FLASH_LP_FPR_UNLOCK;

    R_FACI_LP->FPMCR = value;
    R_FACI_LP->FPMCR = (uint8_t) ~value;
    R_FACI_LP->FPMCR = value;

    if (value == R_FACI_LP->FPMCR)
    {
        __NOP();
    }
}

/*******************************************************************************************************************//**
 * Transition to Code Flash P/E mode.
 * @param[in]  p_ctrl  Pointer to the Flash control block
 **********************************************************************************************************************/
void r_flash_lp_cf_enter_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl)
{
    /* While the Flash API is in use we will disable the Flash Cache. */
 #if BSP_FEATURE_BSP_FLASH_PREFETCH_BUFFER
    R_FACI_LP->PFBER = 0;
 #endif
 #if BSP_FEATURE_BSP_FLASH_CACHE
    R_BSP_FlashCacheDisable();
 #endif

    if (p_ctrl->p_cfg->data_flash_bgo)
    {
        R_BSP_IrqDisable(p_ctrl->p_cfg->irq); // We are not supporting Flash Rdy interrupts for Code Flash operations
    }

    FLASH_LP_PRV_FENTRYR = FLASH_LP_FENTRYR_CODEFLASH_PE_MODE;

    /* See "Procedure for changing from read mode to code flash P/E mode": See Figure 37.15 in Section 37.13.3 of the
     * RA2L1 manual r01uh0853ej0100-ra2l1 */
 #if BSP_FEATURE_FLASH_LP_VERSION == 3
    r_flash_lp_write_fpmcr(FLASH_LP_DISCHARGE_1);

    /* Wait for 2us over (tDIS) */
    r_flash_lp_delay_us(FLASH_LP_WAIT_TDIS, p_ctrl->system_clock_frequency);

    uint32_t fpmcr_command1;
    uint32_t fpmcr_command2;
    uint32_t fpmcr_mode_setup_time;

    /* If the device is not in high speed mode enable LVPE mode as per the flash documentation. */
    if (R_SYSTEM->OPCCR_b.OPCM == 0U)
    {
        fpmcr_command1        = FLASH_LP_DISCHARGE_2;
        fpmcr_command2        = FLASH_LP_CODEFLASH_PE_MODE;
        fpmcr_mode_setup_time = FLASH_LP_WAIT_TMS_HIGH;
    }
    else
    {
        fpmcr_command1        = FLASH_LP_DISCHARGE_2 | FLASH_LP_LVPE_MODE;
        fpmcr_command2        = FLASH_LP_CODEFLASH_PE_MODE | FLASH_LP_LVPE_MODE;
        fpmcr_mode_setup_time = FLASH_LP_WAIT_TMS_MID;
    }

    r_flash_lp_write_fpmcr((uint8_t) fpmcr_command1);
    r_flash_lp_write_fpmcr((uint8_t) fpmcr_command2);

    /* Wait for 5us or 3us depending on current operating mode. (tMS) */
    r_flash_lp_delay_us(fpmcr_mode_setup_time, p_ctrl->system_clock_frequency);
 #elif BSP_FEATURE_FLASH_LP_VERSION == 4
    r_flash_lp_write_fpmcr(0x02);

    /* Wait for 2us over (tDIS) */
    r_flash_lp_delay_us(FLASH_LP_WAIT_TDIS, p_ctrl->system_clock_frequency);
 #endif
}

/*******************************************************************************************************************//**
 * Wait for the current command to finish processing and clear the FCR register. If MF4 is used clear the processing
 * bit before clearing the rest of FCR.
 * See Figure 37.19 in Section 37.13.3 of the RA2L1 manual r01uh0853ej0100-ra2l1
 *
 * @param[in]  timeout         The timeout
 * @retval     FSP_SUCCESS     The command completed successfully.
 * @retval     FSP_ERR_TIMEOUT The command timed out.
 **********************************************************************************************************************/
static fsp_err_t r_flash_lp_command_finish (uint32_t timeout)
{
    /* Worst case timeout */
    volatile uint32_t wait = timeout;

    /* Check the Flash Ready Flag bit*/
    FLASH_LP_REGISTER_WAIT_TIMEOUT(1, R_FACI_LP->FSTATR1_b.FRDY, wait, FSP_ERR_TIMEOUT);

#if BSP_FEATURE_FLASH_LP_VERSION == 4

    /* Stop Processing */
    R_FACI_LP->FCR = R_FACI_LP->FCR & ((uint8_t) ~FLASH_LP_FCR_PROCESSING_MASK);
#endif

    /* Clear FCR register */
    R_FACI_LP->FCR = FLASH_LP_FCR_CLEAR;

    /* Worst case timeout */
    wait = timeout;

    /* Wait for the Flash Ready Flag bit to indicate ready or a timeout to occur. If timeout return error. */
    FLASH_LP_REGISTER_WAIT_TIMEOUT(0, R_FACI_LP->FSTATR1_b.FRDY, wait, FSP_ERR_TIMEOUT);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Wait for the current command to finish processing and check for error.
 *
 * @param      p_ctrl                Pointer to the control block
 * @param[in]  timeout               The timeout
 * @param[in]  error_bits            The error bits related to the current command
 * @param[in]  return_code           The operation specific error code
 *
 * @retval     FSP_SUCCESS           Erase command successfully completed.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for erase command completion.
 * @return     return_code           The operation specific error code.
 **********************************************************************************************************************/
static fsp_err_t r_flash_lp_wait_for_ready (flash_lp_instance_ctrl_t * const p_ctrl,
                                            uint32_t                         timeout,
                                            uint32_t                         error_bits,
                                            fsp_err_t                        return_code)
{
    fsp_err_t err = r_flash_lp_command_finish(timeout);

    /* If a timeout occurs reset the flash and return error. */
    if (FSP_ERR_TIMEOUT == err)
    {
        r_flash_lp_reset(p_ctrl);

        return err;
    }

    /* If an error occurs reset and return error. */
    if (0U != (R_FACI_LP->FSTATR2 & error_bits))
    {
        r_flash_lp_reset(p_ctrl);

        return return_code;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Local memcpy function to prevent from using memcpy linked in code flash
 *
 * @param      dest            The destination
 * @param      src             The source
 * @param[in]  len             The length
 **********************************************************************************************************************/
void r_flash_lp_memcpy (uint8_t * const dest, uint8_t * const src, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }
}

#endif
