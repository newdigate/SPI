/* SPIIMXRT1176.cpp - SPI master for the NXP MIMXRT1176 (RT1170-EVKB), LPSPI1.
 *
 * Clean MIT implementation for the imxrt1176 Arduino core: the methods
 * implement the documented Arduino/Teensy SPI API, and every register,
 * clock, and pin operation is this project's HW-verified RT1176 LPSPI1
 * bring-up (originally cores/imxrt1176/SPI.cpp; developed from the NXP
 * MCUXpresso SDK, BSD-3-Clause). Not derived from the GPL/LGPL-dual SPI.cpp
 * platform branches in this repository -- see LICENSE.md.
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

#if defined(__IMXRT1176__)

#include "SPIIMXRT1176.h"
#include "imxrt1176.h"

// Register-op source of truth: the HW-verified RT1176 bring-up sequences now
// live ONCE in the shared C core lpspi1176.c (Phase 3.3) — consumed by this
// class (addresses from imxrt1176.h via hardware.hw) and by the CM4 gate
// images (same addresses as literals). This file keeps only the Arduino API
// surface and the CM7-only DMA path.

#include <stddef.h>

// The shared C core's overlay must equal the core header's (same silicon).
static_assert(offsetof(lpspi1176_regs_t, CR)    == offsetof(IMXRT_LPSPI_t, CR),    "CR");
static_assert(offsetof(lpspi1176_regs_t, DER)   == offsetof(IMXRT_LPSPI_t, DER),   "DER");
static_assert(offsetof(lpspi1176_regs_t, CFGR1) == offsetof(IMXRT_LPSPI_t, CFGR1), "CFGR1");
static_assert(offsetof(lpspi1176_regs_t, CCR)   == offsetof(IMXRT_LPSPI_t, CCR),   "CCR");
static_assert(offsetof(lpspi1176_regs_t, FCR)   == offsetof(IMXRT_LPSPI_t, FCR),   "FCR");
static_assert(offsetof(lpspi1176_regs_t, TCR)   == offsetof(IMXRT_LPSPI_t, TCR),   "TCR");
static_assert(offsetof(lpspi1176_regs_t, TDR)   == offsetof(IMXRT_LPSPI_t, TDR),   "TDR");
static_assert(offsetof(lpspi1176_regs_t, RSR)   == offsetof(IMXRT_LPSPI_t, RSR),   "RSR");
static_assert(offsetof(lpspi1176_regs_t, RDR)   == offsetof(IMXRT_LPSPI_t, RDR),   "RDR");
static_assert(sizeof(lpspi1176_regs_t) == sizeof(IMXRT_LPSPI_t), "LPSPI size");

void SPIClass::begin() {
	lpspi1176_begin(lp(), &hardware.hw, 4000000u, &tcr_base);   // default 4 MHz
}

void SPIClass::end() { lpspi1176_end(lp(), &hardware.hw); }

void SPIClass::setClockDividerHz(uint32_t clockHz) {
	lpspi1176_set_clock_hz(lp(), hardware.hw.func_clock, clockHz, &tcr_base);
}

void SPIClass::beginTransaction(SPISettings s) {
	tcr_base = 0u;
	if (s.dataMode() & 0x2) tcr_base |= LPSPI1176_TCR_CPOL;
	if (s.dataMode() & 0x1) tcr_base |= LPSPI1176_TCR_CPHA;
	if (s.bitOrder() == LSBFIRST) tcr_base |= LPSPI1176_TCR_LSBF;
	setClockDividerHz(s.clock());                // adds PRESCALE bits to tcr_base
}

void SPIClass::endTransaction() { /* manual CS; nothing to release */ }

void SPIClass::setBitOrder(uint8_t bitOrder) {
	if (bitOrder == LSBFIRST) tcr_base |= LPSPI1176_TCR_LSBF;
	else tcr_base &= ~LPSPI1176_TCR_LSBF;
}

void SPIClass::setDataMode(uint8_t dataMode) {
	tcr_base &= ~(LPSPI1176_TCR_CPOL | LPSPI1176_TCR_CPHA);
	if (dataMode & 0x2) tcr_base |= LPSPI1176_TCR_CPOL;
	if (dataMode & 0x1) tcr_base |= LPSPI1176_TCR_CPHA;
}

uint8_t SPIClass::transfer(uint8_t data) {
	return (uint8_t)lpspi1176_transfer_frame(lp(), tcr_base, data, 7u);    // 8-bit frame
}

uint16_t SPIClass::transfer16(uint16_t data) {
	return (uint16_t)lpspi1176_transfer_frame(lp(), tcr_base, data, 15u);  // 16-bit frame
}

void SPIClass::transfer(void *buf, size_t count) {
	uint8_t *p = (uint8_t *)buf;
	for (size_t i = 0; i < count; i++) p[i] = transfer(p[i]);
}

// Single SPI instance on this core: the RX-completion ISR reaches the active
// instance's DMA state through this pointer, set in startDMA().
static SPIClass *dma_active_spi = nullptr;

void SPIClass::startDMA(const void *txbuf, void *rxbuf, size_t count) {
	if (_dmaTX == nullptr) _dmaTX = new DMAChannel();
	if (_dmaRX == nullptr) _dmaRX = new DMAChannel();
	dma_active_spi = this;

	// RX drains RDR -> rxbuf; its completion is the transfer's completion.
	_dmaRX->disable();
	// 8-bit DMA access to the register's low byte (matches TCR FRAMESZ(7))
	_dmaRX->source(*(volatile uint8_t *)&port().RDR);
	_dmaRX->destinationBuffer((uint8_t *)rxbuf, count);
	_dmaRX->disableOnCompletion();
	_dmaRX->triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI1_RX);
	_dmaRX->attachInterrupt(dma_rxisr);
	_dmaRX->interruptAtCompletion();

	// TX feeds txbuf -> TDR.
	_dmaTX->disable();
	// 8-bit DMA access to the register's low byte (matches TCR FRAMESZ(7))
	_dmaTX->destination(*(volatile uint8_t *)&port().TDR);
	_dmaTX->sourceBuffer((const uint8_t *)txbuf, count);
	_dmaTX->disableOnCompletion();
	_dmaTX->triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI1_TX);

	port().TCR = (tcr_base & ~LPSPI1176_TCR_FRAMESZ(0xFFF)) | LPSPI1176_TCR_FRAMESZ(7);  // 8-bit frames
	port().FCR = 0;                                                  // watermark 0
	_transfer_done = false;
	port().DER = LPSPI1176_DER_TDDE | LPSPI1176_DER_RDDE;            // both DMA requests
	_dmaRX->enable();                                                // arm RX before TX
	_dmaTX->enable();
}

void SPIClass::dma_rxisr() {
	SPIClass *spi = dma_active_spi;
	spi->_dmaRX->clearInterrupt();
	spi->_dmaTX->clearComplete();          // both channels disableOnCompletion -> clear latched DONE
	spi->_dmaRX->clearComplete();
	asm volatile ("dsb" ::: "memory");     // DMA_CINT/CDNE writes complete before DER is touched
	spi->port().DER = 0;                    // stop DMA requests
	EventResponder *e = spi->_dma_event_responder;
	spi->_dma_event_responder = nullptr;
	spi->_transfer_done = true;
	if (e) e->triggerEvent();
}

void SPIClass::transfer(const void *txbuf, void *rxbuf, size_t count) {
	if (count == 0 || count > 32767) return;   // BITER/CITER are 16-bit; no chunking (single-shot)
	while (!_transfer_done) yield();        // wait for any in-flight transfer to finish first
	_dma_event_responder = nullptr;
	startDMA(txbuf, rxbuf, count);
	while (!_transfer_done) yield();        // cooperative wait; RX ISR sets the flag
}

bool SPIClass::transfer(const void *txbuf, void *rxbuf, size_t count, EventResponderRef event_responder) {
	if (count == 0 || count > 32767) return false;
	if (!_transfer_done) return false;      // a transfer is already in progress
	_dma_event_responder = &event_responder;
	startDMA(txbuf, rxbuf, count);
	return true;
}

// Hardware description for LPSPI1 (EVKB Arduino header). Same values as the
// HW-verified core cores/imxrt1176/SPI_instances.cpp lpspi1_hw, now expressed
// as the shared lpspi1176_hw_t desc (order: sck/AD_28, sdo=mosi/AD_30,
// sdi=miso/AD_31), followed by the C++-only dma_rxisr + pin numbers.
const SPIClass::SPI_Hardware_t SPIClass::spiclass_lpspi1_hardware = {
	{ &CCM_LPCG104_DIRECT, &CCM_CLOCK_ROOT43_CONTROL, 0u, 24000000u,
	  &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_28, 0x0u, &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_28,
	    &IOMUXC_LPSPI1_SCK_SELECT_INPUT, 0x1u,
	  &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_30, 0x0u, &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_30,
	    &IOMUXC_LPSPI1_SDO_SELECT_INPUT, 0x1u,
	  &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_31, 0x0u, &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_31,
	    &IOMUXC_LPSPI1_SDI_SELECT_INPUT, 0x1u,
	  0x0000000Cu },
	SPIClass::dma_rxisr,
	{ 0 }, { 0 }, { 0 }, { 0 },
};
SPIClass SPI(IMXRT_LPSPI1_ADDRESS, SPIClass::spiclass_lpspi1_hardware);

#endif /* __IMXRT1176__ */
