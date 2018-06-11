# Esp32-MicroPyhton
Fit For BpiBit And BpiUno32.

# Config Guide

# shell exec `gedit ~/.profile`

```

export PATH=$PATH:/root/BPI-BIT-MPyFirmware/xtensa-esp32-elf/bin

export IDF_PATH=/root/BPI-BIT-MPyFirmware/esp-idf

```

#  and `source ~/.profile`

close shell console.

# into `/micropython/ports/esp32`

use `make` build micropython firmware.bin

use `make deploy` flash and download firmware.bin

use `make erase` erase and flash default factory bin

more read Makefile not is makefile.
