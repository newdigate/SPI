#include "Arduino.h"
#include "HardwareSerial.h"
#include "SPI.h"

// ST7735 128x160 SPI TFT demo on the RT1176 EVKB Arduino header.
//   SCK  -> D13 (GPIO_AD_28)   } driven by SPI.begin()
//   MOSI -> D11 (GPIO_AD_30)   }  (connect MOSI -> ST7735 SDA, SCK -> SCL)
//   DC   -> D8  (GPIO_AD_07 = GPIO9 bit 6)   raw-GPIO here
//   CS   -> D10 (GPIO_AD_29 = GPIO9 bit 28)  raw-GPIO here
//   RESET unplugged -> we use the ST7735 software reset (0x01).
// The core's digital pin table only maps the LED, so DC/CS are driven directly.

// ---- GPIO9 raw access (base 0x40C64000; GDIR=+0x04, DR_SET=+0x84, DR_CLEAR=+0x88) ----
#define GPIO9_GDIR    (*(volatile uint32_t *)(0x40C64000u + 0x04u))
#define GPIO9_DR_SET  (*(volatile uint32_t *)(0x40C64000u + 0x84u))
#define GPIO9_DR_CLR  (*(volatile uint32_t *)(0x40C64000u + 0x88u))
#define REG32(a)      (*(volatile uint32_t *)(a))

#define DC_BIT   6u    // GPIO_AD_07 -> GPIO9.6
#define CS_BIT  28u    // GPIO_AD_29 -> GPIO9.28

static inline void dc_command() { GPIO9_DR_CLR = (1u << DC_BIT); }   // DC low = command
static inline void dc_data()    { GPIO9_DR_SET = (1u << DC_BIT); }   // DC high = data
static inline void cs_low()     { GPIO9_DR_CLR = (1u << CS_BIT); }
static inline void cs_high()    { GPIO9_DR_SET = (1u << CS_BIT); }

// Route a GPIO_AD pad to GPIO9 (ALT 0xA) as a push-pull output.
static void gpio9_out(uint32_t mux_reg, uint32_t pad_reg, uint32_t bit) {
	REG32(mux_reg) = 0xAu;          // ALT10 = GPIO9
	REG32(pad_reg) = 0x0008u;       // DSE drive, push-pull
	GPIO9_GDIR |= (1u << bit);      // output
}

// ---- ST7735 dimensions / colors ----
#define TFT_W 128
#define TFT_H 160
#define ST_SWRESET 0x01
#define ST_SLPOUT  0x11
#define ST_INVOFF  0x20
#define ST_DISPON  0x29
#define ST_CASET   0x2A
#define ST_RASET   0x2B
#define ST_RAMWR   0x2C
#define ST_COLMOD  0x3A
#define ST_MADCTL  0x36

#define RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define WHITE  0xFFFF
#define BLACK  0x0000
#define YELLOW 0xFFE0

// CS is held low for the whole session (single device). DC selects cmd/data.
static void tftCmd(uint8_t c) { dc_command(); SPI.transfer(c); }
static void tftDat(uint8_t d) { dc_data();    SPI.transfer(d); }

static void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
	tftCmd(ST_CASET); tftDat(0); tftDat(x0); tftDat(0); tftDat(x1);
	tftCmd(ST_RASET); tftDat(0); tftDat(y0); tftDat(0); tftDat(y1);
	tftCmd(ST_RAMWR);
}

static void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
	if (x >= TFT_W || y >= TFT_H) return;
	if (x + w > TFT_W) w = TFT_W - x;
	if (y + h > TFT_H) h = TFT_H - y;
	setAddrWindow(x, y, x + w - 1, y + h - 1);
	dc_data();
	uint32_t n = (uint32_t)w * h;
	for (uint32_t i = 0; i < n; i++) SPI.transfer16(color);
}

static void fillScreen(uint16_t color) { fillRect(0, 0, TFT_W, TFT_H, color); }

static void st7735_init() {
	tftCmd(ST_SWRESET); delay(150);
	tftCmd(ST_SLPOUT);  delay(255);      // wake from sleep
	tftCmd(ST_COLMOD);  tftDat(0x05);    // 16-bit/pixel (RGB565)
	tftCmd(ST_MADCTL);  tftDat(0xC8);    // row/col order, BGR (flip to 0x08/0x00 if colors look off)
	tftCmd(ST_INVOFF);                    // some panels need INVON (0x21) instead
	tftCmd(ST_CASET); tftDat(0); tftDat(0); tftDat(0); tftDat(TFT_W - 1);
	tftCmd(ST_RASET); tftDat(0); tftDat(0); tftDat(0); tftDat(TFT_H - 1);
	tftCmd(ST_DISPON); delay(100);
}

void setup() {
	Serial1.begin(115200);
	while (!Serial1) {}
	Serial1.println("ST7735 demo: SCK=D13 MOSI=D11(->SDA) DC=D8 CS=D10, SW-reset");

	// DC + CS as GPIO9 outputs; idle CS high, DC data.
	gpio9_out(0x400E8128u, 0x400E836Cu, DC_BIT);   // DC = GPIO_AD_07
	gpio9_out(0x400E8180u, 0x400E83C4u, CS_BIT);   // CS = GPIO_AD_29
	cs_high(); dc_data();

	SPI.begin();
	SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

	cs_low();               // single device: hold CS low for the whole session
	st7735_init();

	// Pixel test: fill, then colored quadrants + a white box + corner pixels.
	fillScreen(BLACK);
	fillRect(0,   0,   64, 80, RED);
	fillRect(64,  0,   64, 80, GREEN);
	fillRect(0,   80,  64, 80, BLUE);
	fillRect(64,  80,  64, 80, YELLOW);
	fillRect(44,  60,  40, 40, WHITE);      // center box
	Serial1.println("drew quadrants + center box; cycling full-screen colors");
}

void loop() {
	static const uint16_t seq[] = {RED, GREEN, BLUE, WHITE, BLACK};
	static int i = 0;
	fillScreen(seq[i]);
	i = (i + 1) % 5;
	delay(1000);
}
