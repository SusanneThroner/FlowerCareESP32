# Flower Care ESP32

Bluetooth Low Energy (BLE) connection of Xiaomi's Flower Care Sensor and ESP32.

# Data structure of BLE

Description | Service UUID | Start  handle | End handle |
| ------ | ------ | ------ | ------ |
Real-time data, Battery Level, Firmware version | 0000**1204**-0000-1000-8000-00805f9b34fb | 49 0x0031 | 57 0x0039 |

Description | Characteristic UUID| Handle | Value | Properties |
| ------ | ------ | ------ | ------ | ------ |
**Real-time data read request** | 0000**1a00**-0000-1000-8000-00805f9b34fb| 51 0x0033 | length = 2 bytes | read, write |
Write this command before reading real-time data | " | " | 0xa01f | write |
**Real-time sensor data** | 0000**1a01**-0000-1000-8000-00805f9b34fb| 53 0x0035 | length = 16 bytes | read, write, notify |
Temperature in +/- 0.1 °C| " | " | position 0-1: int16 | read |
*unknown, seems to be a fixed 00* | " | " | position 2: ? | read |
Brightness in lux | " | " | position 3-6: uint32 | read |
Soil moisture in % | " | " | position 7: uint8 | read |
µS/cm (indirect measure for pH level) | " | " | position 8-9: uint16 | read |
*unknown, seems to be a fixed constant*  | " | " | position 10-15: ? | read | 
**Battery level & Firmware version** | 0000**1a02**-0000-1000-8000-00805f9b34fb| 56 0x0038 | length = 7 bytes | read |
Battery level in % | " | " | position 0: int8 | read |
*unknown (possible delimiter)* | " | " | position 1: ? | read |
Firmware version (e.g. 3.2.2) | " | " | position 2-6: ASCII Text | read |

## Examples

**Real-time data read request**

In order to read the real-time data sensor, you first need to change its mode.
The original hex value is `00 00`. If you would read the real-time sensor data directly it will just output the following constant hex value `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`. 
Writing the value `a01f` to its handle will change the data mode and you'll receive the real data values afterwards. 

*(Note: I do not know why its this command value, I got it from [vrachieru](https://github.com/vrachieru/xiaomi-flower-care-api) - thanks for sharing. Let me know if you know more.)*

**Real-time data**

After writing the previous mentioned value you should receive a 16 byte long hex number of your flower care containing the real-time sensor data. (If you get zeros only, check the previous section.) 
Ig you're using a hex converter (i.e. [RapidTables](https://www.rapidtables.com/convert/number/hex-to-decimal.html)) make sure you swap the bytes order because the data is encoded in little endian (the least-significant byte is stored at the smallest address).

* value output in hex: `0A 01 00 3A 01 00 00 00 00 00 02 3C 00 FB 34 9B` 
* 
| Position  | 00-01 | 02    | 03-06         | 07    | 08-09     | 10-15 |
| Hex Value | 0A01  | 00    | 3A010000      | 00    | 0000      | 02 3C 00 FB 34 9B |
| Type      | int16 | ?     | uint32        | uint8 | uint16    | ? |
| Value     | 266   | ?     | 314           | 0     | 0         | ? |
| Description | Temperature in 0.1 °C | unknown, seems to be fixed | Brightness in lux | Moisture in % | Conductivity in µS/cm | unknown, seems to be fixed |


* value[0:1] = `0A 01`: swap bytes (little endian) -> `010A`: first byte (00) is plus sign, convert `010A` to decimal: 266 -> +266 * 0.1 °C = 26.6 °C
* value[2] = `00`: unknown, seems to be constant
* value[3:6] = `3A 01 00 00`: swap bytes (little endian) -> `0000013A` = 314 lux
* value[7] = `00` = 0% soil moisture
* value[8:9] = `00` = 0 µS/cm conductivity
* value[10:15] = `00 02 3C 00 FB 34 9B` -> unknown, seems to be constant

**Battery level and firmware version**

The battery level and firmware version can be directly read and both are stored in the same value.

* value output: string = `d+3.2.2` / hex = `64 2B 33 2E 32 2E 32` 
* value[0] = `64` = 100 % battery level
* value[1] = `+` or `33`, unknown / possible delimiter or saved space for later, higher firmware versions
* value[2:6] = `2E322E32`  or the rest of string = 3.2.2 firmware version
