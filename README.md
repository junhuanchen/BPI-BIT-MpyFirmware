# Esp32-MicroPyhton
Fit For BpiBit And BpiUno32.

# Config Guide

In root/micropython/ports/esp32/makefile

```

# shell exec `gedit ~/.profile` and source `gedit ~/.profile`

#export PATH=$PATH:/root/esp32/xtensa-esp32-elf/bin

#export IDF_PATH=/root/esp32/esp-idf



#ESPIDF = /root/esp32/esp-idf 

#PORT = /dev/ttyUSB0

#FLASH_MODE = qio

#FLASH_SIZE = 4MB

#CROSS_COMPILE = xtensa-esp32-elf-



include Makefile
```

use `make` build micropython firmware.bin

use `make deploy` flash and download firmware.bin

use `make erase` erase and flash default factory bin

more read Makefile not is makefile.
