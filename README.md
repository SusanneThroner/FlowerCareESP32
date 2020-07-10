# Flower Care ESP32

Bluetooth Low Energy (BLE) connection of Xiaomi's Flower Care Sensor and ESP32.

# Data structure of BLE

## Service for Real-time data, Battery level, firmware version

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

## Service for history data and device epoch time

Description | Service UUID | Start  handle | End handle |
| ------ | ------ | ------ | ------ |
History data, device epoch time | 0000**1206**-0000-1000-8000-00805f9b34fb | 58 0x003a | 66 0x0042 |

Attribute Description | Characteristic UUID| Handle | Value | Properties |
| ------ | ------ | ------ | ------ | ------ |
**History data read request** | 0000**1a10**-0000-1000-8000-00805f9b34fb| 0x003e | length = 3 bytes | read, write TODO |
Write this command to init reading of history data | " | " | 0xa00000 | write |
Then this command to read entry #0 | " | " | a10000 | write |
Write this command to read entry #1 | " | " | a10100 | write |
Write this command to read entry ... | " | " | ... | write |
**History data read previous selected entry** | 0000**1a11**-0000-1000-8000-00805f9b34fb| 60 0x003c | length = 16 bytes | read |
**Device epoch time** | 0000**1a12**-0000-1000-8000-00805f9b34fb| 65 0x00**41** | length = 4 bytes | read |
Seconds since boot | " | " | position 0-3: uint32 | read |




## Examples

**Real-time data read request**

In order to read the real-time data sensor, you first need to change its mode.
The original hex value is `00 00`. If you would read the real-time sensor data directly it will just output the following constant hex value `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`. 
Writing the value `a01f` to its handle will change the data mode and you'll receive the real data values afterwards. 

*(Note: I do not know why its this command value, I got it from [vrachieru](https://github.com/vrachieru/xiaomi-flower-care-api) - thanks for sharing. Let me know if you know more.)*

**Real-time data**

After writing the previous mentioned value you should receive a 16 byte long hex number of your flower care containing the real-time sensor data. (If you get zeros only, check the previous section.) 
If you're using a hex converter (i.e. [RapidTables](https://www.rapidtables.com/convert/number/hex-to-decimal.html)) to get the actual values, make sure you swap the bytes order because the data is encoded in little endian (the least-significant byte is stored at the smallest address).

* value output in hex: `0A 01 00 3A 01 00 00 00 00 00 02 3C 00 FB 34 9B` 

| Bytes     | Hex value     | Swapped bytes |   Type      | Value   | Description                       |
| ------    | ------        | ------        | ------    | ------    | ------                            |
| 00-01     | 0A01          | 010A          | int16     | 266       | Temperature in 0.1 °C             |
| 02        | 00            | 00            | ?         | ?         | unknown, seems to be fixed value  |
| 03-06     | 3A010000      | 0000013A      | uint32    | 314       | Brightness in lux                 |
| 07        | 00            | 00            | uint8     | 0         | Moisture in %                     |
| 08-09     | 0000          | 0000          | uint16    | 0         | Conductivity in µS/cm             |
| 10-15     | 023C00FB349B  | 9B34FB003C02  | ?         | ?         | unknown, seems to be fixed value  |

* value[0:1] = `0A 01`: swap bytes (little endian) -> `010A`, convert `010A` to decimal: 266 -> +266 * 0.1 °C = 26.6 °C
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

**Device epoch time**

* value output in hex = `00 13 7C 7F`
* value[0-3] = `00 13 7C 7F`: swap bytes -> `7F7c1300` to decimal: 2138837760 seconds
 
# Resources

* Online converter for hex to decimal, ASCII Text, etc. -> https://www.rapidtables.com/convert/number/hex-to-decimal.html

