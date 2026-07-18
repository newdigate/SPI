/* SPIIMXRT1176.h - SPI master for the NXP MIMXRT1176 (RT1170-EVKB), LPSPI1.
 *
 * Clean MIT implementation for the imxrt1176 Arduino core: the class is
 * written from the documented Arduino/Teensy SPI API surface, and every
 * register, clock, and pin operation comes from this project's HW-verified
 * RT1176 bring-up (originally cores/imxrt1176; developed from the NXP
 * MCUXpresso SDK, BSD-3-Clause). Not derived from the GPL/LGPL-dual SPI.h
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

#ifndef SPIIMXRT1176_h
#define SPIIMXRT1176_h

#if defined(__IMXRT1176__)

#include <Arduino.h>
#include <DMAChannel.h>
#include <EventResponder.h>
#include "lpspi1176.h"

/* Normally included from SPI.h after its common constants; keep fallbacks so
 * this header also works standalone. These are the standard Arduino SPI API
 * constants. */
#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#ifndef SPI_MODE0
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
#endif
#ifndef SPI_HAS_TRANSACTION
#define SPI_HAS_TRANSACTION 1
#endif

#define SPI_HAS_NOTUSINGINTERRUPT 1
#define SPI_ATOMIC_VERSION 1

class SPISettings {
public:
	SPISettings(uint32_t clockIn, uint8_t bitOrderIn, uint8_t dataModeIn)
		: _clock(clockIn), _bitOrder(bitOrderIn), _dataMode(dataModeIn) {}
	SPISettings()
		: _clock(4000000), _bitOrder(MSBFIRST), _dataMode(SPI_MODE0) {}
private:
	inline uint32_t clock() const { return _clock; }
	inline uint8_t bitOrder() const { return _bitOrder; }
	inline uint8_t dataMode() const { return _dataMode; }

	uint32_t _clock;
	uint8_t _bitOrder;
	uint8_t _dataMode;
	friend class SPIClass;
};

class SPIClass { // MIMXRT1176-EVKB — LPSPI1, full Teensy API over the RT1176 core driver
public:
	static const uint8_t CNT_MISO_PINS = 1;
	static const uint8_t CNT_MOSI_PINS = 1;
	static const uint8_t CNT_SCK_PINS = 1;
	static const uint8_t CNT_CS_PINS = 1;
	typedef struct {
		lpspi1176_hw_t hw;                       // shared C core hardware desc (lpspi1176.h)
		void (*dma_rxisr)();
		const uint8_t  miso_pin[CNT_MISO_PINS];  // SDI
		const uint8_t  mosi_pin[CNT_MOSI_PINS];  // SDO
		const uint8_t  sck_pin[CNT_SCK_PINS];
		const uint8_t  cs_pin[CNT_CS_PINS];
	} SPI_Hardware_t;
	static const SPI_Hardware_t spiclass_lpspi1_hardware;

	SPIClass(uintptr_t myport, const SPI_Hardware_t &myhardware)
		: port_addr(myport), hardware(myhardware) {}

	void begin();
	void end();
	void usingInterrupt(uint8_t n) {}
	void usingInterrupt(IRQ_NUMBER_t interruptName) {}
	void notUsingInterrupt(IRQ_NUMBER_t interruptName) {}
	void beginTransaction(SPISettings settings);
	void endTransaction();
	uint8_t  transfer(uint8_t data);
	uint16_t transfer16(uint16_t data);
	void     transfer(void *buf, size_t count);
	void     transfer(const void *buf, void *retbuf, size_t count);
	bool     transfer(const void *buf, void *retbuf, size_t count, EventResponderRef event_responder);
	void     setBitOrder(uint8_t bitOrder);
	void     setDataMode(uint8_t dataMode);
	void     setClockDivider(uint8_t clockDiv) {}
	uint8_t  setCS(uint8_t pin) { return 0; }
	void     setMOSI(uint8_t pin) {}
	void     setMISO(uint8_t pin) {}
	void     setSCK(uint8_t pin) {}
	bool     pinIsChipSelect(uint8_t pin) { return false; }
	bool     pinIsMOSI(uint8_t pin) { return pin == hardware.mosi_pin[0]; }
	bool     pinIsMISO(uint8_t pin) { return pin == hardware.miso_pin[0]; }
	bool     pinIsSCK(uint8_t pin)  { return pin == hardware.sck_pin[0]; }

	IMXRT_LPSPI_t & port() { return *(IMXRT_LPSPI_t *)port_addr; }
private:
	lpspi1176_regs_t *lp() { return (lpspi1176_regs_t *)port_addr; }
	uintptr_t port_addr;
	const SPI_Hardware_t &hardware;
	uint32_t tcr_base = 0;
	DMAChannel *_dmaTX = nullptr;
	DMAChannel *_dmaRX = nullptr;
	EventResponder *_dma_event_responder = nullptr;
	volatile bool _transfer_done = true;
	void setClockDividerHz(uint32_t clockHz);
	void startDMA(const void *txbuf, void *rxbuf, size_t count);
	static void dma_rxisr();
};

extern SPIClass SPI;

#endif /* __IMXRT1176__ */
#endif /* SPIIMXRT1176_h */
