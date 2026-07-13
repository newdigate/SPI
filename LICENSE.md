# License

This repository contains code under two licenses, per file:

## MIMXRT1176 port — MIT

`SPIIMXRT1176.h`, `SPIIMXRT1176.cpp`, and the `tests/` directory are
Copyright (c) 2026 Nicholas Newdigate and licensed under the **MIT License**
(full text in each file header). They implement the documented Arduino/Teensy
SPI API over the RT1176 LPSPI hardware, developed from the NXP MCUXpresso SDK
(BSD-3-Clause) and this project's own hardware bring-up. They are not derived
from the GPL/LGPL platform branches below.

A build for `__IMXRT1176__` compiles **only** the MIT files: `SPI.h`'s other
platform branches are preprocessor-excluded and `SPI.cpp` contains no
IMXRT1176 code.

## Upstream Arduino/Teensy platforms — GPL-2.0 / LGPL-2.1 dual

`SPI.h` and `SPI.cpp` (the AVR / Teensy 2/3/4 platform branches) are the
upstream Arduino/Teensyduino SPI library:

> Copyright (c) 2010 by Cristian Maglie, (c) 2014 by Paul Stoffregen,
> (c) 2014 by Matthijs Kooijman.
> Free software; redistributable under either the GNU General Public License
> version 2 or the GNU Lesser General Public License version 2.1.

Builds for those platforms are governed by that dual license.
