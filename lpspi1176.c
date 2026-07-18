/* lpspi1176.c - shared LPSPI register/clock core for the NXP MIMXRT1176.
 *
 * This project's HW-verified RT1176 LPSPI1 bring-up (SPIIMXRT1176.cpp, MIT),
 * re-expressed as the single shared C core (Phase 3.3): the sequence bodies
 * below are that file's begin()/setClockDividerHz()/transfer() verbatim.
 * Consumed by the CM7 SPIClass and the CM4 gate images.
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

#include "lpspi1176.h"

void lpspi1176_set_clock_hz(lpspi1176_regs_t *p, uint32_t func_clock,
                            uint32_t clock_hz, uint32_t *tcr_base)
{
	if (clock_hz == 0u) clock_hz = 1000u;        /* guard divide-by-zero; clamp to slow */
	uint32_t prescale = 0, sckdiv = 0;
	for (prescale = 0; prescale < 8u; prescale++) {
		uint32_t pdiv = 1u << prescale;
		uint32_t denom = pdiv * clock_hz;
		uint32_t div = (func_clock + denom - 1u) / denom;   /* ceil(func/(pdiv*clk)) */
		if (div < 2u) div = 2u;
		sckdiv = div - 2u;
		if (sckdiv <= 255u) break;
	}
	if (prescale > 7u) { prescale = 7u; sckdiv = 255u; }
	uint32_t men = p->CR & LPSPI1176_CR_MEN;
	p->CR = 0u;                                  /* CCR is writable only with MEN=0 */
	p->CCR = (p->CCR & ~0xFFu) | (sckdiv & 0xFFu);
	if (men) p->CR = LPSPI1176_CR_MEN;
	*tcr_base = (*tcr_base & ~(0x7u << 27)) | LPSPI1176_TCR_PRESCALE(prescale);
}

void lpspi1176_begin(lpspi1176_regs_t *p, const lpspi1176_hw_t *hw,
                     uint32_t clock_hz, uint32_t *tcr_base)
{
	*hw->lpcg = 1u;                              /* ungate LPSPI clock */
	*hw->clock_root = hw->clock_root_val;
	*hw->sck_mux = hw->sck_mux_val;  *hw->sck_pad = hw->pad_ctl_val;
	*hw->sdo_mux = hw->sdo_mux_val;  *hw->sdo_pad = hw->pad_ctl_val;
	*hw->sdi_mux = hw->sdi_mux_val;  *hw->sdi_pad = hw->pad_ctl_val;
	*hw->sck_select = hw->sck_select_val;
	*hw->sdo_select = hw->sdo_select_val;
	*hw->sdi_select = hw->sdi_select_val;
	p->CR = LPSPI1176_CR_RST;  p->CR = 0u;       /* reset the block (MEN=0) */
	p->CFGR1 = LPSPI1176_CFGR1_MASTER;           /* master mode (write while MEN=0) */
	*tcr_base = 0u;                              /* MODE0, MSB first */
	lpspi1176_set_clock_hz(p, hw->func_clock, clock_hz, tcr_base);
	p->CR = LPSPI1176_CR_MEN;                    /* enable */
}

void lpspi1176_end(lpspi1176_regs_t *p, const lpspi1176_hw_t *hw)
{
	p->CR = 0u;
	*hw->lpcg = 0u;
}

uint32_t lpspi1176_transfer_frame(lpspi1176_regs_t *p, uint32_t tcr_base,
                                  uint32_t data, uint32_t framesz)
{
	p->TCR = tcr_base | LPSPI1176_TCR_FRAMESZ(framesz);
	p->TDR = data;
	for (uint32_t g = 0; g < LPSPI1176_TIMEOUT; g++) {
		if (!(p->RSR & LPSPI1176_RSR_RXEMPTY)) return p->RDR;
	}
	return 0xFFFFFFFFu;
}
