# Introduction
The goal for this protocol is to be small and simple. It is not made to transfer large amounts of data at a time.

The first byte of the protocol is a command while the last is crc. The CRC used is CRC-8-AUTOSAR. The recommended payload length is 12 bytes. the other 2 bytes are reserved for the CRC and the command.

It is meant to be built on top of protocols like I2C and UART