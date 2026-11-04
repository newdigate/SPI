#SPI Library for Teensy#

http://www.pjrc.com/teensy/td_libs_SPI.html

![](http://www.pjrc.com/teensy/td_libs_SPI_1.jpg)

## NXP MIMXRT1176 (MIMXRT1170-EVKB) and Nosh

The RT1176 port (`SPIIMXRT1176.{h,cpp}`, `lpspi1176.{h,c}`, MIT) provides one
object, `SPI` = LPSPI1 on GPIO_AD_28 (SCK) / AD_30 (MOSI) / AD_31 (MISO), the
EVKB's Arduino SPI header (D13 / D11 / D12).

On the Nosh board (`ARDUINO_NOSH`, from teensy-cmake-macros `TEENSY_BOARD NOSH`)
no LPSPI bus reaches a connector and those three pads are BT_DEV_WAKE, the
panel backlight enable and the Wi-Fi W_DISABLE line. The `SPI` object still
exists there, because SdFat and other libraries name it at link time even with
the card on SDIO, but it owns no pads: `SPI.begin()`/`end()` touch nothing,
transfers return all-ones, the DMA transfer returns false, and
`SPIClass::busPresent()` is `false`.

## NXP MIMXRT1060-EVKB

This fork adds pin maps for the NXP MIMXRT1060-EVKB (selected with the
`ARDUINO_MIMXRT1060_EVKB` board define). Three SPI buses are wired:

| Object | Peripheral | MOSI | MISO | SCK | CS  | Notes |
|--------|-----------|------|------|-----|-----|-------|
| `SPI`  | LPSPI1    | 11   | 12   | 13  | 10  | Arduino header (J24/J25) |
| `SPI1` | LPSPI3    | 25   | 24   | 26  | 23  | chip pads (AD_B1_12-15) — not on a header |
| `SPI2` | LPSPI4    | 29   | 28   | 30  | 27  | chip pads (B0_00-03) — not on a header |

Default `SPI` is LPSPI1, which is the peripheral routed to the EVKB's Arduino
SPI header — so unmodified Arduino SPI sketches work out of the box. `SPI1` and
`SPI2` are provided for custom wiring; their pads are not broken out on the
EVKB headers.

Verified end-to-end against the in-tree i.MX RT1062 QEMU model (an `ssi-loopback`
slave on LPSPI1): `SPI.transfer(x)` round-trips correctly.
