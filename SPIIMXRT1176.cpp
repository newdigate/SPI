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

// Register-op source of truth: the HW-verified RT1176 core driver
// (originally cores/imxrt1176/SPI.cpp), re-expressed through the documented
// Arduino/Teensy SPI method signatures:
//   core  hw->cr     -> port().CR        core  hw->rsr -> port().RSR
//   core  hw->cfgr1  -> port().CFGR1     core  hw->rdr -> port().RDR
//   core  hw->ccr    -> port().CCR       core  hw->der -> port().DER
//   core  hw->tcr    -> port().TCR       core  hw->fcr -> port().FCR
//   core  hw->tdr    -> port().TDR
//   core  hw->lpcg/clock_root/*_mux/*_pad/*_select_input -> hardware.<field>
//   core  func_clock (member) -> hardware.func_clock

// CR
#define CR_MEN   (1u<<0)
#define CR_RST   (1u<<1)
// CFGR1
#define CFGR1_MASTER (1u<<0)
// TCR fields
#define TCR_FRAMESZ(n)  ((uint32_t)((n) & 0xFFFu))   // n = bits-1
#define TCR_PRESCALE(p) (((uint32_t)(p) & 0x7u) << 27)
#define TCR_CPHA (1u<<30)
#define TCR_CPOL (1u<<31)
#define TCR_LSBF (1u<<23)
// RSR
#define RSR_RXEMPTY (1u<<1)
// DER (DMA enable)
#define DER_TDDE (1u<<0)
#define DER_RDDE (1u<<1)

#define SPI_TIMEOUT 100000u

void SPIClass::begin() {
	hardware.lpcg = 1u;                                  // ungate LPSPI clock
	hardware.clock_root = hardware.clock_root_val;       // mux 0 => 24 MHz
	hardware.sck_mux  = hardware.sck_mux_val;   hardware.sck_pad  = hardware.pad_ctl_val;
	hardware.mosi_mux = hardware.mosi_mux_val;  hardware.mosi_pad = hardware.pad_ctl_val;  // SDO
	hardware.miso_mux = hardware.miso_mux_val;  hardware.miso_pad = hardware.pad_ctl_val;  // SDI
	hardware.sck_select_input_register  = hardware.sck_select_val;
	hardware.mosi_select_input_register = hardware.mosi_select_val;   // SDO
	hardware.miso_select_input_register = hardware.miso_select_val;   // SDI
	port().CR = CR_RST;  port().CR = 0u;                 // reset the block (MEN=0)
	port().CFGR1 = CFGR1_MASTER;                         // master mode (write while MEN=0)
	tcr_base = 0u;                                       // MODE0, MSB first (prescale added next)
	setClockDividerHz(4000000);                          // default 4 MHz: writes CCR, ORs prescale into tcr_base
	port().CR = CR_MEN;                                  // enable
}

void SPIClass::end() { port().CR = 0u; hardware.lpcg = 0u; }

// Program CCR.SCKDIV and the PRESCALE bits of tcr_base for the requested SCK.
// SCK = func_clock / (prescale_div * (SCKDIV + 2)); pick smallest prescale with
// SCKDIV in [0,255] giving SCK <= clockHz.
void SPIClass::setClockDividerHz(uint32_t clockHz) {
	if (clockHz == 0u) clockHz = 1000u;          // guard divide-by-zero; clamp to slow
	uint32_t prescale = 0, sckdiv = 0;
	for (prescale = 0; prescale < 8u; prescale++) {
		uint32_t pdiv = 1u << prescale;
		uint32_t denom = pdiv * clockHz;
		uint32_t div = (hardware.func_clock + denom - 1u) / denom;   // ceil(func/(pdiv*clk))
		if (div < 2u) div = 2u;
		sckdiv = div - 2u;
		if (sckdiv <= 255u) break;
	}
	if (prescale > 7u) { prescale = 7u; sckdiv = 255u; }
	uint32_t men = port().CR & CR_MEN;
	port().CR = 0u;                              // CCR is writable only with MEN=0
	port().CCR = (port().CCR & ~0xFFu) | (sckdiv & 0xFFu);
	if (men) port().CR = CR_MEN;
	tcr_base = (tcr_base & ~(0x7u << 27)) | TCR_PRESCALE(prescale);
}

void SPIClass::beginTransaction(SPISettings s) {
	tcr_base = 0u;
	if (s.dataMode() & 0x2) tcr_base |= TCR_CPOL;
	if (s.dataMode() & 0x1) tcr_base |= TCR_CPHA;
	if (s.bitOrder() == LSBFIRST) tcr_base |= TCR_LSBF;
	setClockDividerHz(s.clock());                // adds PRESCALE bits to tcr_base
}

void SPIClass::endTransaction() { /* manual CS; nothing to release */ }

void SPIClass::setBitOrder(uint8_t bitOrder) {
	if (bitOrder == LSBFIRST) tcr_base |= TCR_LSBF;
	else tcr_base &= ~TCR_LSBF;
}

void SPIClass::setDataMode(uint8_t dataMode) {
	tcr_base &= ~(TCR_CPOL | TCR_CPHA);
	if (dataMode & 0x2) tcr_base |= TCR_CPOL;
	if (dataMode & 0x1) tcr_base |= TCR_CPHA;
}

uint8_t SPIClass::transfer(uint8_t data) {
	port().TCR = tcr_base | TCR_FRAMESZ(7);      // 8-bit frame
	port().TDR = data;
	for (uint32_t g = 0; g < SPI_TIMEOUT; g++) {
		if (!(port().RSR & RSR_RXEMPTY)) return (uint8_t)port().RDR;
	}
	return 0xFFu;
}

uint16_t SPIClass::transfer16(uint16_t data) {
	port().TCR = tcr_base | TCR_FRAMESZ(15);     // 16-bit frame
	port().TDR = data;
	for (uint32_t g = 0; g < SPI_TIMEOUT; g++) {
		if (!(port().RSR & RSR_RXEMPTY)) return (uint16_t)port().RDR;
	}
	return 0xFFFFu;
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

	port().TCR = (tcr_base & ~TCR_FRAMESZ(0xFFF)) | TCR_FRAMESZ(7);  // 8-bit frames
	port().FCR = 0;                                                  // watermark 0
	_transfer_done = false;
	port().DER = DER_TDDE | DER_RDDE;                                // both DMA requests
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

// Hardware description for LPSPI1 (EVKB Arduino header). Values copied from the
// core cores/imxrt1176/SPI_instances.cpp lpspi1_hw, mapped to the library
// struct's field order: miso(SDI/AD_31), mosi(SDO/AD_30), sck(AD_28), cs, pad_ctl.
const SPIClass::SPI_Hardware_t SPIClass::spiclass_lpspi1_hardware = {
	CCM_LPCG104_DIRECT, CCM_CLOCK_ROOT43_CONTROL, 0u, 24000000u, SPIClass::dma_rxisr,
	{ 0 }, IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_31, 0x0u, IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_31,
		IOMUXC_LPSPI1_SDI_SELECT_INPUT, 0x1u,
	{ 0 }, IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_30, 0x0u, IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_30,
		IOMUXC_LPSPI1_SDO_SELECT_INPUT, 0x1u,
	{ 0 }, IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_28, 0x0u, IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_28,
		IOMUXC_LPSPI1_SCK_SELECT_INPUT, 0x1u,
	{ 0 },
	0x0000000Cu,
};
SPIClass SPI(IMXRT_LPSPI1_ADDRESS, SPIClass::spiclass_lpspi1_hardware);

#endif /* __IMXRT1176__ */
