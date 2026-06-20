#SPI Library for Teensy#

http://www.pjrc.com/teensy/td_libs_SPI.html

![](http://www.pjrc.com/teensy/td_libs_SPI_1.jpg)

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
