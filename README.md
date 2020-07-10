# Flower Care ESP32

Getting data of Xiaomi's Flower Care Sensor by using ESP32 via Bluetooth Low Energy (BLE) client without use of the official app.

The flower care is portable device powered by a CR2032 button battery, with a wireless 4.1 BLE connection offering multiple sensors for plant related measurements:

* **Light sensor**: measures brightness in lux, range: 0 –  100000 lux
* **Air Temperature sensor**: measures air temperature in C, range -20C – 50C, precision: 0.5 C
* **Soil moisture sensor**: measures moisture within soil in %
* **"Soil Fertility sensor"**: measures conductivity in µS/cm (Note: this measure is dependent on moisture, more moisture always will yield higher conductivity levels *).

*Note: I've used the flower care's internation version with firmware 3.2.2.*

*Sources: 
* *https://www.mobilegeeks.de/test/xiaomi-flower-care-smart-sensor-im-test-gruener-daumen-dank-technik/*
* *https://www.amazon.de/Xiaomi-Flower-Smart-Sensor-Pflanzenmonitor/dp/B074TY93JM*

* In my opinion the recommended µS/cm ranges in the app are very broad and independent of actual plant size.
 
## API Features

This is a basic API for receiving the sensor data of the Flower Care once and printing the values in Arduino's Serial Monitor:
* Real-time data with temperature, brightness, moisture and conductivity
* Historical data entries with timestamp of the same sensor measurements
* General information, like mac address, firmware version, battery level and device epoch time
* Debug option for BLE connection, services and characteristics or raw hex values. 

**Example output**:

```
Scanning for BLE devices with serviceAdvertisementUUID =  0000fe95-0000-1000-8000-00805f9b34fb...
BLE Advertised Device found: 
Name: Flower care, Address: c4:7c:8d:6a:8e:ca, serviceUUID: 0000fe95-0000-1000-8000-00805f9b34fb
Scanning done.

Create client and connecting to c4:7c:8d:6a:8e:ca
Callback: onConnect.

Name: Flower care
Mac address: c4:7c:8d:6a:8e:ca
Battery level: 90 %
Firmware version: 3.2.2
Real time data: 
Temperature: 27.3 °C
Brightness: 681 lux
Soil moisture: 40 %
Conductivity: 206 µS/cm 

Historical data: 
Epoch time: 4990145 seconds or 57 days, 18 hours, 9 minutes, 0 seconds.
Reading historical entries, this may take some time...
Found 1 history entries:
Successfully read all historical data.
Entry #0:
Timestamp: 4989600 seconds
Temperature: 27.6 °C
Brightness: 1001 lux
Soil moisture: 5 %
Conductivity: 40 µS/cm 

Callback: onDisconnect
```

**Example output with `#define DEBUG` enabled at the beginning of the script:**

```
Scanning for BLE devices with serviceAdvertisementUUID =  0000fe95-0000-1000-8000-00805f9b34fb...
Found a BLE device, checking if service UUID is present...BLE Advertised Device found: 
Name: Flower care, Address: c4:7c:8d:6a:8e:ca, serviceUUID: 0000fe95-0000-1000-8000-00805f9b34fb
Scanning done.

Create client and connecting to c4:7c:8d:6a:8e:ca
Callback: onConnect.

Name: Flower care
Mac address: c4:7c:8d:6a:8e:ca
DEBUG: Reading value of characteristicUUID = 00001204-0000-1000-8000-00805f9b34fb of serviceUUID = 00001a02-0000-1000-8000-00805f9b34fb:
DEBUG: Value length n = 7, Hex: 5a 2b 33 2e 32 2e 32  
Battery level: 90 %
DEBUG: Reading value of characteristicUUID = 00001204-0000-1000-8000-00805f9b34fb of serviceUUID = 00001a02-0000-1000-8000-00805f9b34fb:
DEBUG: Value length n = 7, Hex: 5a 2b 33 2e 32 2e 32  
Firmware version: 3.2.2
DEBUG: Writing the following 2 bytes: ' a0 1f ' to characteristicUUID = 00001a00-0000-1000-8000-00805f9b34fb of serviceUUID = 00001204-0000-1000-8000-00805f9b34fb 
DEBUG: Reading value of characteristicUUID = 00001204-0000-1000-8000-00805f9b34fb of serviceUUID = 00001a01-0000-1000-8000-00805f9b34fb:
DEBUG: Value length n = 16, Hex: 0e 01 00 48 02 00 00 28 d0 00 02 3c 00 fb 34 9b  
Real time data: 
Temperature: 27.0 °C
Brightness: 584 lux
Soil moisture: 40 %
Conductivity: 208 µS/cm 

Historical data: 
Epoch time: 4990370 seconds or 57 days, 18 hours, 12 minutes, 0 seconds.
Reading historical entries, this may take some time...
// This is the read init command for before being able to read the actual historical values
DEBUG: Writing the following 3 bytes: ' a0 00 00 ' to characteristicUUID = 00001a10-0000-1000-8000-00805f9b34fb of serviceUUID = 00001206-0000-1000-8000-00805f9b34fb 
DEBUG: Reading value of characteristicUUID = 00001206-0000-1000-8000-00805f9b34fb of serviceUUID = 00001a11-0000-1000-8000-00805f9b34fb:
DEBUG: Value length n = 16, Hex: 01 00 f0 9d 82 13 08 00 c8 15 08 00 00 00 00 00 
// Actual history entry 
Found 1 history entries:
DEBUG: Writing the following 3 bytes: ' a1 00 00 ' to characteristicUUID = 00001a10-0000-1000-8000-00805f9b34fb of serviceUUID = 00001206-0000-1000-8000-00805f9b34fb 
DEBUG: Reading value of characteristicUUID = 00001206-0000-1000-8000-00805f9b34fb of serviceUUID = 00001a11-0000-1000-8000-00805f9b34fb:
DEBUG: Value length n = 16, Hex: a0 22 4c 00 14 01 00 e9 03 00 00 05 28 00 00 00  
Successfully read all historical data.
Entry #0:
Timestamp: 4989600 seconds
Temperature: 27.6 °C
Brightness: 1001 lux
Soil moisture: 5 %
Conductivity: 40 µS/cm 

Callback: onDisconnect.
```

## Usage

* Download the Arduino sketch `flowerCareBLE` and open it in Arduino
* Connect your ESP32 via USB to your computer
* Open Serial Monitor: `Tools > Serial Monitor`
* On bottom set Serial Monitor to: `11520 baud` for receiving outpu
* Select your board, i.e.: `Tools > Board 'NodeMCU-32s'`
* Upload sketch to your ESP
* You can receive more detailled information, like BLE properties and hex values, by uncommenting the debug preprocessor: `#define DEBUG` at the beginning of the script
* If you have problems connecting your ESP32, ensure a) your not connected with the flower care's app on your smartphone, b) the flower care's battery is charged or c) ESP32 and device are close enough.

## Limitations/Known Bugs

* The Api currently only supports connection with one Flower Care device. It uses the Bluetooth low energy advertisement service for connection, thus no entering of mac address is needed but in case of multiple devices it will connect to the one it finds first. However, it's possible and can be extended.
* After reading approximately the first 110 entries it will loose the BLE connection. I did not find the reason yet. There's an error logged in case this happens. But be aware. And as always: let me know in case you find the error in my script or maybe in the BLE library itself.
* I do not know if this script works for the chinese version of the flower care sensor because I have the international one. Also I do not know if the Xiaomi Mija Flower 'Pale Blue Lily' uses the same BLE data structure.
* There's no clearing of the historical data implemented. 

Maybe todos:
BLE connection of flower care and ESP32, 
Getting Started

# Data structure of Flower Care's BLE

## Root service for BLE advertisement

Description | Service UUID | Start  handle | End handle |
| ------ | ------ | ------ | ------ |
BLE advertisement for connection | 0000**fe95**-0000-1000-8000-00805f9b34fb | - | - |

## Service for Real-time data, Battery level, firmware version

Description | Service UUID | Start  handle | End handle |
| ------ | ------ | ------ | ------ |
Real-time data, Battery Level, Firmware version | 0000**1204**-0000-1000-8000-00805f9b34fb | 49 0x0031 | 57 0x0039 |

Attribute Description | Characteristic UUID| Handle | Byte Positions | Value/Type | Properties |
| ------ | ------ | ------ | ------ | ------ | ------ |
**Real-time data read request** | 0000**1a00**-0000-1000-8000-00805f9b34fb| 51 0x0033 | length = 2 bytes | - | read, write |
Write this command before reading real-time data | " | " | - | `0xa01f` | write |
**Real-time sensor data** | 0000**1a01**-0000-1000-8000-00805f9b34fb| 53 0x0035 | length = 16 bytes | - | read, write, notify |
Temperature in +/- 0.1 °C| " | " | 00-01 | int16 | read |
*unknown, seems to be constantly 0x00* | " | " | 02 | ? | read |
Brightness in lux | " | " | 03-06 | uint32 | read |
Soil moisture in % | " | " | 07 | uint8 | read |
µS/cm (indirect measure for pH level) | " | " | 08-09 | uint16 | read |
*unknown, seems to be constantly 0x02 3c 00 fb 34 9b*  | " | " | 10-15 | ? | read | 
**Battery level & Firmware version** | 0000**1a02**-0000-1000-8000-00805f9b34fb| 56 0x0038 | length = 7 bytes | - | read |
Battery level in % | " | " | 00 | uint8 | read |
*unknown (possible delimiter/space holder 0x2B)* | " | " | 01 | ? | read |
Firmware version (e.g. 3.2.2) | " | " | 02-06 | ASCII Text | read |

## Service for history data and device epoch time

Description | Service UUID | Start  handle | End handle |
| ------ | ------ | ------ | ------ |
History data, device epoch time | 0000**1206**-0000-1000-8000-00805f9b34fb | 58 0x003a | 66 0x0042 |

Attribute Description | Characteristic UUID| Handle | Byte Positions | Value/Type | Properties |
| ------ | ------ | ------ | ------ | ------ | ------ |
**History data read request** | 0000**1a10**-0000-1000-8000-00805f9b34fb| 62 0x003e | length = 3 bytes | - | read, write, notify |
Write this command to init reading of history data | " | " | - | `0xa00000` | write |
Then this command to read entry #0 | " | " | - | `0xa10000` | write |
Write this command to read entry #1 | " | " | - | `0xa10100` | write |
Write this command to read entry ... | " | " | - | ... | write |
**History data read previous selected entry** | 0000**1a11**-0000-1000-8000-00805f9b34fb| 60 0x003c | length = 16 bytes | - | read |
Timestamp since device boot in seconds | " | " | 00-03 | uint32 | read |
Temperature in +/- 0.1 °C | " | " | 04-05 | int16  | read |
*unknown, seems to be constantly 0x00* | " | " | 06 | ? | read |
Brightness in lux | " | " | 07-10 | uint32 | read |
Soil moisture in % | " | " | 11 | uint8  | read |
µS/cm (indirect measure for pH level) | " | " | 12-13 | uint16  | read |
*unknown, seems to be constantly 0x00 00* | " | " | 14-15 | ? | read |
**Device epoch time** | 0000**1a12**-0000-1000-8000-00805f9b34fb| 65 0x00**41** | length = 4 bytes | - | read |
Seconds since boot | " | " | 00-03 | uint32 | read |

## Examples

**Real-time data read request**

In order to read the real-time data sensor, you first need to change its mode.
The original hex value is `00 00`. If you would read the real-time sensor data directly it will just output the following constant hex value `aa bb cc dd ee ff 99 88 77 66 00 00 00 00 00 00`. 
Writing the value `a01f` to its handle will change the data mode and you'll receive the real data values afterwards. 

*(Note: I do not know why its this command value, I got it from [vrachieru](https://github.com/vrachieru/xiaomi-flower-care-api) - thanks for sharing. Let me know if you know more.)*

**Real-time data**

After writing the previous mentioned value you should receive a 16 byte long hex number of your flower care containing the real-time sensor data. (If you get zeros only, check the previous section.) 
If you're using a hex converter (i.e. [RapidTables](https://www.rapidtables.com/convert/number/hex-to-decimal.html)) to get the actual values, make sure you swap the bytes order because the data is encoded in little endian (the least-significant byte is stored at the smallest address).

* value output in hex: `0E 01 00 48 02 00 00 28 D0 00 02 3C 00 FB 34 9B` 

| Bytes     | Hex value     | Swapped bytes |   Type      | Value   | Description                       |
| ------    | ------        | ------        | ------    | ------    | ------                            |
| 00-01     | 0E01          | 010E          | int16     | 270       | Temperature in 0.1 °C             |
| 02        | 00            | 00            | ?         | ?         | unknown, seems to be fixed value  |
| 03-06     | 48020000      | 00000248      | uint32    | 584       | Brightness in lux                 |
| 07        | 28            | 28            | uint8     | 40        | Moisture in %                     |
| 08-09     | D000          | 00D0          | uint16    | 208       | Conductivity in µS/cm             |
| 10-15     | 023C00FB349B  | 9B34FB003C02  | ?         | ?         | unknown, seems to be fixed value  |

**Battery level and firmware version**

The battery level and firmware version can be directly read and both are stored in the same value.

* value output: string = `d+3.2.2` / hex = `64 2B 33 2E 32 2E 32` 
* value[0] = `64` = 100 % battery level
* value[1] = `+` or `33`, unknown / possible delimiter or saved space for later, higher firmware versions
* value[2:6] = `2E322E32`  or the rest of string = 3.2.2 firmware version

**Device epoch time**

* value output in hex = `00 13 7C 7F`
* value[0-3] = `00 13 7C 7F`: swap bytes -> `7F7c1300` to decimal: 2138837760 seconds

**Historical data read request**

Similar to the real-time data you need to write an initialization command before being able to actual read the historical entries, otherwise you'll receive only 16 bytes of zeros: `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00` and you won't find any historical entries at all.
Writing the following 3 bytes `A0 00 00` to the `0x00 00 1A 10...` characteristic of service `0x00 00 12 06...` will make the data available to you.

**Select one historical entry**

After initialization you'll need to select each entry individually by its address. The first entry is at address `A1 01 00`, second at `A1 02 00`,... sixteenth at `A1 10 00` and so on. Write the corresponding 3 byte address to the characteristic UUID `0x00 00 1A 10...` of the same service.

**Historical entries**

After selection of a historical data entry you will receive 16 hex bytes, i.e.:

* value output in hex: `A0 22 4C 00 14 01 00 E9 03 00 00 05 28 00 00 00` 

| Bytes     | Hex value     | Swapped bytes |   Type    | Value   | Description                       |
| ------    | ------        | ------        | ------    | ------    | ------                            |
| 00-03     | A0224C00      | 004C22A0      | uint32    | 4989600   | Timestamp in seconds since boot   |
| 04-05     | 1401          | 0114          | int16     | 276       | Temperature in +/- 0.1°C          |
| 06        | 00            | 00            | ?         | ?         | unknown, seems to be fixed value |
| 07-10     | E9030000      | 000003E9      | uint32    | 1001      | Brightness in lux        |
| 11        | 05            | 05            | uint8     | 5         |  Moisture in %             |
| 12-13     | 2800          | 0028          | uint16    | 40        | Conductivity in µS/cm  |
| 14-15     | 0000          | 0000          | ?         | ?         | unknown, seems to be fixed value |

# Resources

Many thanks to Neil Kolban for providing a BLE library and vrarchieru for sharing the flower care project!

* Flower care python api, BLE UUIDs, data structure: https://github.com/vrachieru/xiaomi-flower-care-api
* Flower care teardown: https://wiki.hackerspace.pl/projects:xiaomi-flora
* BLE library for ESP32 with documentation (integrated in Arduino): http://www.neilkolban.com/esp32/docs/cpp_utils/html/class_b_l_e_u_u_i_d.html
* BLE GATT services: https://www.bluetooth.com/de/specifications/gatt/services/
* BLE GATT characteristics: https://www.bluetooth.com/specifications/gatt/characteristics/
* Online converter for hex to decimal, ASCII Text, etc.: https://www.rapidtables.com/convert/number/hex-to-decimal.html

# License

MIT