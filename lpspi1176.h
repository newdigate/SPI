/* lpspi1176.h - shared LPSPI register/clock core for the NXP MIMXRT1176.
 *
 * This project's HW-verified RT1176 LPSPI1 bring-up (SPIIMXRT1176.cpp, MIT),
 * re-expressed as the single shared C core (Phase 3.3). Consumed by BOTH the
 * CM7 SPIClass (which passes register addresses from imxrt1176.h) and the
 * bare-metal CM4 gate images (which pass the same addresses as literals) —
 * ending the CM7/CM4 keep-in-sync sequence duplication. Freestanding C11:
 * compiles under the CM4 image flags (-ffreestanding, no core headers) and
 * inside the C++ library.
 *
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LPSPI1176_H
#define LPSPI1176_H

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
#define LPSPI1176_ASSERT(c, m) static_assert(c, m)
extern "C" {
#else
#define LPSPI1176_ASSERT(c, m) _Static_assert(c, m)
#endif

/* LPSPI register-block overlay (offsets per RT1170 RM; layout equals the
 * core's IMXRT_LPSPI_t — cross-asserted in SPIIMXRT1176.cpp). */
typedef struct {
	volatile uint32_t VERID;       /* 0x00 */
	volatile uint32_t PARAM;       /* 0x04 */
	volatile uint32_t r08, r0C;
	volatile uint32_t CR;          /* 0x10 */
	volatile uint32_t SR;          /* 0x14 */
	volatile uint32_t IER;         /* 0x18 */
	volatile uint32_t DER;         /* 0x1C */
	volatile uint32_t CFGR0;       /* 0x20 */
	volatile uint32_t CFGR1;       /* 0x24 */
	volatile uint32_t r28, r2C;
	volatile uint32_t DMR0;        /* 0x30 */
	volatile uint32_t DMR1;        /* 0x34 */
	volatile uint32_t r38, r3C;
	volatile uint32_t CCR;         /* 0x40 */
	volatile uint32_t r44[5];
	volatile uint32_t FCR;         /* 0x58 */
	volatile uint32_t FSR;         /* 0x5C */
	volatile uint32_t TCR;         /* 0x60 */
	volatile uint32_t TDR;         /* 0x64 */
	volatile uint32_t r68, r6C;
	volatile uint32_t RSR;         /* 0x70 */
	volatile uint32_t RDR;         /* 0x74 */
} lpspi1176_regs_t;

LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, CR)    == 0x10, "LPSPI CR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, DER)   == 0x1C, "LPSPI DER");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, CFGR1) == 0x24, "LPSPI CFGR1");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, CCR)   == 0x40, "LPSPI CCR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, FCR)   == 0x58, "LPSPI FCR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, TCR)   == 0x60, "LPSPI TCR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, TDR)   == 0x64, "LPSPI TDR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, RSR)   == 0x70, "LPSPI RSR");
LPSPI1176_ASSERT(offsetof(lpspi1176_regs_t, RDR)   == 0x74, "LPSPI RDR");

/* Hardware description: CCM gate/root + the SCK/SDO/SDI pads (RM pin names;
 * the Arduino-facing class maps mosi->SDO, miso->SDI). */
typedef struct {
	volatile uint32_t *lpcg;         /* CCM LPCG DIRECT (write 1 to ungate) */
	volatile uint32_t *clock_root;   /* CCM CLOCK_ROOT CONTROL */
	uint32_t clock_root_val;         /* 0 => mux0 OSC24M div1 -> 24 MHz */
	uint32_t func_clock;             /* resulting functional clock (Hz) */
	volatile uint32_t *sck_mux;  uint32_t sck_mux_val;  volatile uint32_t *sck_pad;
	volatile uint32_t *sck_select;  uint32_t sck_select_val;
	volatile uint32_t *sdo_mux;  uint32_t sdo_mux_val;  volatile uint32_t *sdo_pad;
	volatile uint32_t *sdo_select;  uint32_t sdo_select_val;
	volatile uint32_t *sdi_mux;  uint32_t sdi_mux_val;  volatile uint32_t *sdi_pad;
	volatile uint32_t *sdi_select;  uint32_t sdi_select_val;
	uint32_t pad_ctl_val;            /* one pad config for all three pins */
} lpspi1176_hw_t;

/* CR */
#define LPSPI1176_CR_MEN        (1u << 0)
#define LPSPI1176_CR_RST        (1u << 1)
/* CFGR1 */
#define LPSPI1176_CFGR1_MASTER  (1u << 0)
/* TCR fields */
#define LPSPI1176_TCR_FRAMESZ(n)  ((uint32_t)((n) & 0xFFFu))   /* n = bits-1 */
#define LPSPI1176_TCR_PRESCALE(p) (((uint32_t)(p) & 0x7u) << 27)
#define LPSPI1176_TCR_CPHA      (1u << 30)
#define LPSPI1176_TCR_CPOL      (1u << 31)
#define LPSPI1176_TCR_LSBF      (1u << 23)
/* RSR */
#define LPSPI1176_RSR_RXEMPTY   (1u << 1)
/* DER (DMA enable; used by the CM7 DMA path) */
#define LPSPI1176_DER_TDDE      (1u << 0)
#define LPSPI1176_DER_RDDE      (1u << 1)

#define LPSPI1176_TIMEOUT       100000u

/* Ungate+root the clock, mux the pins, reset the block, master mode,
 * program the divider for clock_hz (writing *tcr_base's PRESCALE), enable. */
void lpspi1176_begin(lpspi1176_regs_t *p, const lpspi1176_hw_t *hw,
                     uint32_t clock_hz, uint32_t *tcr_base);
void lpspi1176_end(lpspi1176_regs_t *p, const lpspi1176_hw_t *hw);
/* CCR.SCKDIV + PRESCALE bits of *tcr_base for the requested SCK
 * (SCK = func_clock / (2^prescale * (SCKDIV+2))). */
void lpspi1176_set_clock_hz(lpspi1176_regs_t *p, uint32_t func_clock,
                            uint32_t clock_hz, uint32_t *tcr_base);
/* Polled full-duplex single frame (framesz = bits-1): load TCR, write TDR,
 * spin on RSR.RXEMPTY, read RDR. 0xFFFFFFFF on timeout. */
uint32_t lpspi1176_transfer_frame(lpspi1176_regs_t *p, uint32_t tcr_base,
                                  uint32_t data, uint32_t framesz);

#if defined(__cplusplus)
}
#endif
#endif /* LPSPI1176_H */
