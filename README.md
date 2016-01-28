# BusPirate
Exploration of I2C read/write serial communication with 24LC08B EEPROM.  Yes, these programs are desperately in need of modularization (and use of a struct probably wouldn't hurt anything), but they were written as an I2C serial communications learning exercise and it was easier for me to lump everything into main.  Feel free to create your own fork and improve as necessary.

bus_pirate_read.c:  A very simple EEPROM reader.  Use a gigantic loop to read up to 1024 bytes from 24LC08B (WARNING:  see below note about first block) or until an EOL byte is encountered.  This results in single byte reads ... very slow ... but it works!
bus_pirate_write.c:  An equally simple EEPROM writer.  Use a gigantic loop to write up to 255 bytes to 24LC08B or until an EOL byte is encountered.  This results in single write bytes.  We can do better.  See below.
bus_pirate_write_all.c:  A more advanced writer ... Write in blocks of 8 bytes!

All these programs are constrained to operate on the first block of memory within the 24LC08B (4 blocks supported).
