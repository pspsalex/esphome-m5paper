#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file it8951e_priv.h
 * @brief Private header file for IT8951 EPD controller Command, modes, and registers definitions.
 *
 * This file contains the command definitions, mode definitions, and register addresses
 * for the IT8951 EPD (E-Paper Display) controller.
 */

#include <stdint.h>

namespace esphome {
namespace it8951e {

/**
 * @enum UpdateMode
 * @brief Enumeration of IT8951 update modes.
 *
 * This enumeration defines the update modes of the IT8951 EPD controller.
 */
enum class UpdateMode : uint16_t
{
    /**
     * @brief Init mode, used to initialize display to an all-white display (2 sec, no ghosting, white)
     *
     * The initialization (INIT) mode is used to completely erase the display
     * and leave it in the white state. It is useful for situations where the display
     * information in memory is not a faithful representation of the optical state of
     * the display, for example, after the device receives power after it has been
     * fully powered down. This waveform switches the display several times and leaves
     * it in the white state.
     */
    Init  = 0,

    /**
     * @brief DU Direct Update, Monochrome menu, text input, and touch screen input (260ms, low ghosting, BW)
     *
     * The direct update (DU) is a very fast, non-flashy update. This mode supports
     * transitions from any graytone to black or white only. It cannot be used to
     * update to any graytone other than black or white. The fast update time for this
     * mode makes it useful for response to touch sensor or pen input or menu selection
     * indicators.
     */
    DU    = 1,

    /**
     * @brief GC16 Grayscale Clearing 16, High quality images (450ms, very low ghosting, 16 colors)
     *
     * The grayscale clearing (GC16) mode is used to update the full display and
     * provide a high image quality. When GC16 is used with Full Display Update the
     * entire display will update as the new image is written. If a Partial Update
     * command is used the only pixels with changing graytone values will update. The
     * GC16 mode has 16 unique gray levels.
     */
    GC16  = 2,

    /**
     * @brief GL16, Text with white background (450ms, medium ghosting, 16 colors)
     *
     * The GL16 waveform is primarily used to update sparse content on a white
     * background, such as a page of anti-aliased text, with reduced flash. The GL16
     * waveform has 16 unique gray levels.
     */
    GL16  = 3,

    /**
     * @brief GLR16, Text with white background (450ms, low ghosting, 16 colors)
     *
     * The GLR16 mode is used in conjunction with an image preprocessing algorithm to
     * update sparse content on a white background with reduced flash and reduced image
     * artifacts. The GLR16 mode supports 16 graytones. If only the even pixel states
     * are used (0, 2, 4, … 30), the mode will behave exactly as a traditional GL16
     * waveform mode. If a separately-supplied image preprocessing algorithm is used,
     * the transitions invoked by the pixel states 29 and 31 are used to improve
     * display quality. For the AF waveform, it is assured that the GLR16 waveform data
     * will point to the same voltage lists as the GL16 data and does not need to be
     * stored in a separate memory.
     *
     */
    GLR16 = 4,

    /**
     * @brief GLD16 Text and graphics with white background (450ms, low ghosting, 16 colors)
     *
     * The GLD16 mode is used in conjunction with an image preprocessing algorithm to
     * update sparse content on a white background with reduced flash and reduced image
     * artifacts. It is recommended to be used only with the full display update. The
     * GLD16 mode supports 16 graytones. If only the even pixel states are used (0, 2,
     * 4, … 30), the mode will behave exactly as a traditional GL16 waveform mode. If a
     * separately-supplied image preprocessing algorithm is used, the transitions
     * invoked by the pixel states 29 and 31 are used to refresh the background with a
     * lighter flash compared to GC16 mode following a predetermined pixel map as
     * encoded in the waveform file, and reduce image artifacts even more compared to
     * the GLR16 mode. For the AF waveform, it is assured that the GLD16 waveform data
     * will point to the same voltage lists as the GL16 data and does not need to be
     * stored in a separate memory.
     */
    GLD16 = 5,

    /**
     * @brief DU4 Fast page flipping at reduced contrast (120ms, medium ghosting, 4 colors)
     *
     * The DU4 is a fast update time (similar to DU), non-flashy waveform. This mode
     * supports transitions from any gray tone to gray tones 1,6,11,16 represented by
     * pixel states [0 10 20 30]. The combination of fast update time and four gray
     * tones make it useful for anti-aliased text in menus. There is a moderate
     * increase in ghosting compared with GC16.
     */
    DU4   = 6,

    /**
     * @brief A2 Anti-aliased text in menus / touch and screen input (290ms, medium ghosting, BW)
     *
     * The A2 mode is a fast, non-flash update mode designed for fast paging turning or
     * simple black/white animation. This mode supports transitions from and to black
     * or white only. It cannot be used to update to any graytone other than black or
     * white. The recommended update sequence to transition into repeated A2 updates is
     * shown in Figure 1. The use of a white image in the transition from 4-bit to
     * 1-bit images will reduce ghosting and improve image quality for A2 updates.
     */
    A2    = 7,

    /**
     * @brief no update
     */
    None  = 8
};

/**
 * @enum Command
 * @brief Enumeration of IT8951 command codes.
 *
 * This enumeration defines the command codes used to control the IT8951 EPD controller.
 *
 */
enum class Command : uint16_t
{
    /**
     * @brief Run the system
     */
    TCON_SYS_RUN          = 0x0001,

    /**
     * @brief Put system in standby
     */
    TCON_STANDBY          = 0x0002,

    /**
     * @brief Put the system to sleep
     */
    TCON_SLEEP            = 0x0003,

    /**
     * @brief Read a register
     */
    TCON_REG_RD           = 0x0010,

    /**
     * @brief Write a register
     */
    TCON_REG_WR           = 0x0011,

    /**
     * @brief Memory burst read prepare
     */
    TCON_MEM_BST_RD_T     = 0x0012,

    /**
     * @brief Memory burst read start
     */
    TCON_MEM_BST_RD_S     = 0x0013,

    /**
     * @brief Memory burst write
     */
    TCON_MEM_BST_WR       = 0x0014,

    /**
     * @brief End memory burst operation
     */
    TCON_MEM_BST_END      = 0x0015,

    /**
     * @brief Load full image data
     */
    TCON_LD_IMG           = 0x0020,

    /**
     * @brief Load partial image data
     */
    TCON_LD_IMG_AREA      = 0x0021,

    /**
     * @brief End image load
     */
    TCON_LD_IMG_END       = 0x0022,

    /**
     * @brief Update display area
     */
    I80_CMD_DPY_AREA      = 0x0034,

    /**
     * @brief Update buffered display area
     */
    I80_CMD_DPY_BUF_AREA  = 0x0037,

    /**
     * @brief Set VCOM voltage level
     */
    I80_CMD_VCOM          = 0x0039,

    /**
     * @brief Read device information
     */
    I80_CMD_GET_DEV_INFO  = 0x0302,
};


/**
 * @brief Display pixel depth
 */
enum class PixelMode : uint16_t
{
    /**
     * @brief 2 bits per pixel mode.
     */
    BPP_2 = 0,

    /**
     * @brief 3 bits per pixel mode.
     */
    BPP_3 = 1,

    /**
     * @brief 4 bits per pixel mode.
     */
    BPP_4 = 2,

    /**
     * @brief 8 bits per pixel mode.
     */
    BPP_8 = 3
 };


/**
 * @brief Display buffer endianness
 */
enum class Endianness : uint16_t
{
    /**
     * @brief Little endian. For a 4bpp image, high nibble is pixel 1 and low nibble is pixel 0
     */
    LITTLE = 0,

    /**
     * @brief Big endian. For a 4bpp image, high nibble is pixel 0 and low nibble is pixel 1
     */
    BIG = 1
};


/**
 * @brief IT8951 Registers
 */
enum class Register : uint16_t
{
    /**
     * @brief Base address for system registers.
     */
    SYS_REG_BASE    = 0x0000,

    /**
     * @brief Address of I80CPCR Register.
     */
    I80PCR          = SYS_REG_BASE + 0x04,

    /**
     * @brief Base address for display registers.
     */
    DISPLAY_BASE    = 0x1000,

    /**
     * @brief Address of LUT0 Engine Width Height Register.
     */
    LUT0EWHR        = DISPLAY_BASE + 0x00,

    /**
     * @brief Address of LUT0 XY Register.
     */
    LUT0XYR         = DISPLAY_BASE + 0x40,

    /**
     * @brief Address of LUT0 Base Address Register.
     */
    LUT0BADDR       = DISPLAY_BASE + 0x80,

    /**
     * @brief Address of LUT0 Mode and Frame number Register.
     */
    LUT0MFN         = DISPLAY_BASE + 0xC0,

    /**
     * @brief Address of LUT0 and LUT1 Active Flag Register.
     */
    LUT01AF         = DISPLAY_BASE + 0x114,

    /**
     * @brief Address of Update Parameter0 Setting Register.
     */
    UP0SR           = DISPLAY_BASE + 0x134,

    /**
     * @brief Address of Update Parameter1 Setting Register.
     */
    UP1SR           = DISPLAY_BASE + 0x138,

    /**
     * @brief Address of LUT0 Alpha blend and Fill rectangle Value Register.
     */
    LUT0ABFRV       = DISPLAY_BASE + 0x13C,

    /**
     * @brief Address of Update Buffer Base Address Register.
     */
    UPBBADDR        = DISPLAY_BASE + 0x17C,

    /**
     * @brief Address of LUT0 Image buffer X/Y offset Register.
     */
    LUT0IMXY        = DISPLAY_BASE + 0x180,

    /**
     * @brief Address of LUT Status Register.
     */
    LUTAFSR         = DISPLAY_BASE + 0x224,

    /**
     * @brief Address of Bitmap (1bpp) image color table Register.
     */
    BGVR            = DISPLAY_BASE + 0x250,

    /**
     * @brief Base address for memory converter registers.
     */
    CONVERTER_BASE  = 0x0200,

    /**
     * @brief Address of MCSR Register.
     */
    MCSR            = CONVERTER_BASE + 0x00,

    /**
     * @brief Address of LISAR Register (low word).
     */
    LISAR           = CONVERTER_BASE + 0x08,

    /**
     * @brief Address of LISAR Register (high word).
     */
    LISARH          = CONVERTER_BASE + 0x0C
};


/**
 * @brief IT8951 Screen rotation.
 */
enum class Rotation : uint16_t
{
    /**
     * @brief No rotation (default)
     */
    ROTATE_0,

    /**
     * @brief Rotate 90 degrees
     */
    ROTATE_90,

    /**
     * @brief Rotate 180 degrees
     */
    ROTATE_180,

    /**
     * @brief Rotate 270 degrees
     */
    ROTATE_270,
};


} // namespace it8951e
} // namespace esphome
