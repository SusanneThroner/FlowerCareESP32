#include "BLEDevice.h"
// Usage:
// Select your ESP32 board under: Tools > "Board: [current selected board]" > i.e. "NodeMCU-32s"
// Connect your ESP32 via USB
// Open Serial Monitor: Tools > Serial Monitor
// On bottom set Serial Monitor to: 115200 baud for receiving output
// Then upload to your ESP 32

// Resource
// For reading and writing values, as well as data structure: https://github.com/vrachieru/xiaomi-flower-care-api
// For BLE connection:

// Uncomment to get more information printed out
 #define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print(x)
  #define DEBUG_PRINTF(x,y)  Serial.printf(x,y)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTF(x,y)
#endif

// Service #0: Root service used for advertising of Bluetooth low energy (BLE) connection
static BLEUUID serviceAdvertisementUUID("0000fe95-0000-1000-8000-00805f9b34fb");

// Service #1: Real-time sensor data, battery level and firmware version
// Real-time sensor data with properties: READ, WRITE, NOTIFY
static BLEUUID serviceBatteryFirmwareRealTimeDataUUID("00001204-0000-1000-8000-00805f9b34fb");
// Read request needs to be written prior to be able to read real-time data.
uint8_t CMD_REAL_TIME_READ_INIT[2] = {0xA0, 0x1F};                                                  //  write these two bytes...
static BLEUUID characteristicReadRequestToRealTimeDataUUID("00001a00-0000-1000-8000-00805f9b34fb"); // ...to this characteristic 
static BLEUUID characteristicRealTimeDataUUID("00001a01-0000-1000-8000-00805f9b34fb");              // ...in order to read actual values of this characteristic
// Firmware version and battery level with properties: READ can be directly read returning battery level in % and firmware version in '3.2.2' notation.
static BLEUUID characteristicFirmwareAndBatteryUUID("00001a02-0000-1000-8000-00805f9b34fb");

// Service #2: Epoch time and Historical data
static BLEUUID serviceHistoricalDataUUID("00001206-0000-1000-8000-00805f9b34fb");
// Read request needs to be written prior to be able to read real-time data.
uint8_t CMD_HISTORY_READ_INIT[3] = {0xa0, 0x00, 0x00};                                              // write these three bytes...
static BLEUUID characteristicReadRequestToHistoricalDataUUID("00001a10-0000-1000-8000-00805f9b34fb");// ...to this characteristic (Write Characteristic)
static BLEUUID characteristicHistoricalDataUUID("00001a11-0000-1000-8000-00805f9b34fb");            // ...in order to read number of historical entries (Read Characteristic)
// afterwards you can write the history entry addresses to Write Characteristic and then read the value stored in the Read Characteristic one by one
uint8_t CMD_HISTORY_READ_ENTRY[3] = {0xa1, 0x00, 0x00}; // 0xa1 is fixed, 0x00 00 represents entry #1, 0x01 00 represents entry #2, 0x02 00 entry #3, ...
// Epoch time values can be directly read of this UUID returning time in seconds since device boot.
static BLEUUID characteristicEpochTimeUUID("00001a12-0000-1000-8000-00805f9b34fb");

// Then connect to server
static BLEAdvertisedDevice* myDevice;
static BLEClient*  pClient;

// Flags
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean readingHistoricalEntries = false;
static int lastValidHistoricalEntryIndx = 0;

// CLASSES -------------------------------------------------------------------------------------------

// A real-time entry ouput example of 16 bytes in Hex: "0E 01 00 48 02 00 00 28 D0 00 02 3C 00 FB 34 9B". 
// If using a hex converter, remember to swap bytes order (little endian encoded) to gain correct values.
class RealTimeEntry
{
  public:
  RealTimeEntry(std::string value)
  {
    const char *val = value.c_str();
    temperature = ((int16_t*) val)[0];    // 2 bytes at pos 00-01: "0E 01"        -> 270 * 0.1°C = 27.0 °C
    // 1 byte at pos 02 unknown, seems to be fixed value of "0x00"
    brightness = *(uint32_t*) (val+3);    // 4 bytes at pos 03-06: "48 02 00 00"  -> 584 lux
    moisture = *(uint8_t*) (val+7);       // 1 byte  at pos 07:    "28"           -> 40%
    conductivity = *(uint16_t*) (val+8);  // 2 bytes at pos 08-09: "D0 00"        -> 208 µS/cm
    // 6 bytes at pos 10-15 unknown, seems to be fixed value of "0x02 3C 00 FB 34 9B" 
  }
  std::string toString()
  {
    char buffer[120];
    int count = 0;
    count += snprintf(buffer, sizeof(buffer), "Temperature: %2.1f °C\n", ((float)this->temperature)/10);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Brightness: %u lux\n", this->brightness);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Soil moisture: %d %s\n", this->moisture, "%");
    count += snprintf(buffer+count, sizeof(buffer)-count, "Conductivity: %" PRIu16 " µS/cm \n", this->conductivity);
    return (std::string)buffer;
  }
  int16_t temperature; // in 0.1 °C
  uint32_t brightness; // in lux
  uint8_t moisture; // in %
  uint16_t conductivity; // in µS/cm
};

// A historical entry ouput example of 16 bytes in Hex: "A0 22 4C 00 14 01 00 E9 03 00 00 05 28 00 00 00". 
// If using a hex converter, remember to swap bytes order (little endian encoded) to gain correct values.
class HistoricalEntry
{
  public:
  HistoricalEntry(std::string value)
  {
    const char *val = value.c_str();
    timestamp = ((uint32_t*) val)[0];     // 4 bytes at pos 00-03: "A0 22 4C 00"  -> 4989600 seconds
    temperature = *(int16_t*) (val+4);    // 2 bytes at pos 04-05: "14 01"        -> 276 * 0.1°C = 27.6 °C
    // 1 byte at pos 6 unknown, seems to be fixed value of "0x00"
    brightness = *(uint32_t*) (val+7);    // 4 bytes at pos 07-10: "E9 03 00 00"  -> 1001 lux
    moisture = *(uint8_t*) (val+11);      // 1 byte  at pos 11:    "05"           -> 5 % 
    conductivity = *(uint16_t*) (val+12); // 2 bytes at pos 12-13: "28 00"        -> 40 µS/cm 
    // 2 bytes at pos 14-15 unknown, seems to be fixed value of "0x00 00"
  }

  std::string toString()
  {
    char buffer[300];
    int count = 0;
    count += snprintf(buffer, sizeof(buffer), "Timestamp: %" PRIu32 " seconds\n", this->timestamp);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Temperature: %2.1f °C\n", ((float)this->temperature)/10);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Brightness: %u lux\n", this->brightness);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Soil moisture: %d %s\n", this->moisture, "%");
    count += snprintf(buffer+count, sizeof(buffer)-count, "Conductivity: %" PRIu16 " µS/cm \n", this->conductivity);
    return (std::string)buffer;
  }

  uint32_t timestamp; // seconds since device epoch (boot)
  int16_t temperature; // in 0.1 °C
  uint32_t brightness; // in lux
  uint8_t moisture; // in %
  uint16_t conductivity; // in µS/cm
};

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // We have found a device, let us now see if it contains the service we are looking for.
    DEBUG_PRINT("Found a BLE device, checking if service UUID is present...");
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceAdvertisementUUID)) {
      Serial.printf("BLE Advertised Device found: \n%s\n", advertisedDevice.toString().c_str());

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
    else
      DEBUG_PRINT("Service not found on this BLE device.\n");
  } // onResult
}; // MyAdvertisedDeviceCallbacks

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.printf("Callback: onConnect.\n\n");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.printf("Callback: onDisconnect.\n\n");
  }
};

class FlowerCare {
  public:
  FlowerCare()
  {
    scanBLE();
    if(myDevice->haveName())
      _name = myDevice->getName();
    else
      _name = "Unnamed";

    _macAddress = myDevice->getAddress().toString();
  }

  void scanBLE()
  {
    Serial.printf("Scanning for BLE devices with serviceAdvertisementUUID =  %s...\n", serviceAdvertisementUUID.toString().c_str());
    BLEDevice::init("");
    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
    delay(1000);
    Serial.printf("Scanning done.\n\n");
  }

  void connectToServer()
  {
      Serial.printf("Create client and connecting to %s\n", myDevice->getAddress().toString().c_str());
      pClient  = BLEDevice::createClient();
      pClient->setClientCallbacks(new MyClientCallback());
      pClient->connect(myDevice);  // Connect to the remove BLE Server.
  }

  // Device's mac address usually starts with: "c4:7c:8d:xx:xx:xx"
  std::string getMacAddress()
  {
    return _macAddress;
  }

  // The name usually is "Flower care"
  std::string getName()
  {
    return _name;
  }
  
  // Firmware and battery ouput example in Hex: "64 2B 33 2E 32 2E 32". 
  // Battery level is stored in very first hex: "64" represented in int as: 100%.
  int getBatteryLevel()
  {
     std::string value = readValue(serviceBatteryFirmwareRealTimeDataUUID, characteristicFirmwareAndBatteryUUID);
     _batteryLevel = (uint8_t)value[0];
     return _batteryLevel;
  }
  
  // Firmware version stored in positions 2 till end: "33 2E 32 2E 32" represented in ASCII Text: 3.2.2
  std::string getFirmwareVersion()
  {
    if(_firmwareVersion.empty())
    {
      std::string value = readValue(serviceBatteryFirmwareRealTimeDataUUID, characteristicFirmwareAndBatteryUUID);
      _firmwareVersion = &value[2]; 
      return _firmwareVersion;
    }
  }

  RealTimeEntry* getRealTimeData()
  {
    writeValue(serviceBatteryFirmwareRealTimeDataUUID, characteristicReadRequestToRealTimeDataUUID, CMD_REAL_TIME_READ_INIT, 2);
    std::string value = readValue(serviceBatteryFirmwareRealTimeDataUUID, characteristicRealTimeDataUUID);
    return new RealTimeEntry(value);
  }

  uint16_t nHistoricalEntries;

  std::vector<HistoricalEntry> getHistoricalData()
  {
    Serial.println("Reading historical entries, this may take some time...");
    readingHistoricalEntries = true;
    writeValue(serviceHistoricalDataUUID, characteristicReadRequestToHistoricalDataUUID, CMD_HISTORY_READ_INIT, 3);
    std::string value = readValue(serviceHistoricalDataUUID, characteristicHistoricalDataUUID);
    const char *val = value.c_str();
    nHistoricalEntries = ((uint16_t*) val)[0];
    Serial.printf("Found %d history entries:\n", nHistoricalEntries);

    std::vector<HistoricalEntry> historicalEntries;
    uint8_t entryAddress[3];
    memcpy(entryAddress, CMD_HISTORY_READ_ENTRY, 3);
    uint16_t* pEntry = (uint16_t*) &entryAddress[1];
    if(nHistoricalEntries > 0)
    {
      for(int i=0; i < nHistoricalEntries; i++)//nHistoricalEntries; i++)
      {
        if(connected)
        {
          writeValue(serviceHistoricalDataUUID, characteristicReadRequestToHistoricalDataUUID, entryAddress, 3);
          value = readValue(serviceHistoricalDataUUID, characteristicHistoricalDataUUID);
          HistoricalEntry* pHistoricalEntry = new HistoricalEntry(value);
          // When connection is lost, false values are read, so ensure only valid data is added
          if(pHistoricalEntry->timestamp > 1) 
          {
            historicalEntries.push_back(*pHistoricalEntry);
            *pEntry += 1;
          }
          else
          {
            Serial.printf("ERROR: Invalid historical entry #%d detected:\n%s\n", i, pHistoricalEntry->toString().c_str());
            lastValidHistoricalEntryIndx = i;
            return historicalEntries;
          }
        }
        else
        {
          Serial.println("ERROR: BLE connection lost, stopped reading historical entries");
          return historicalEntries;
        }
      }
    }
    Serial.println("Successfully read all historical data.");
    return historicalEntries;
  }
  
  int getEpochTimeInSeconds()
  {
    BLERemoteService* pService = getService(serviceHistoricalDataUUID);
    BLERemoteCharacteristic* pCharacteristic = getCharacteristic(pService, characteristicEpochTimeUUID);
    _epochTime = pCharacteristic->readUInt32();
    return _epochTime;
  }

  void printSecondsInDays(int n)
  {
    int days = n / (24*3600);
    n = n % (24*3600);
    int hours = n / 3600;
    n %= 3600;
    int minutes = n / 60;
    n %= 60;
    int seconds = n / 60;
    Serial.printf("%d days, %d hours, %d minutes, %d seconds.\n", days, hours, minutes, seconds);
  }
  
  private:
  std::string _macAddress = "c4:7c:xx:xx:xx:xx";
  std::string _name;
  uint8_t _batteryLevel;
  std::string _firmwareVersion = "";
  uint32_t _epochTime;

  std::string readValue(BLEUUID serviceUUID, BLEUUID characteristicUUID)
  {
    printDebugReadValue(serviceUUID.toString(), characteristicUUID.toString());
    std::string value = "";
    BLERemoteService* pService = getService(serviceUUID);
    if (pService == nullptr) {
      Serial.println("Returning empty string value.");
      return value;
    }
    BLERemoteCharacteristic* pCharacteristic = getCharacteristic(pService, characteristicUUID);
    if(pCharacteristic == nullptr) {
      Serial.println("Returning empty string value.");
      return value;
    }
    try
    {
      value = pCharacteristic->readValue();
      printDebugHexValue(value, value.length());
    }
    catch(...)
    {
      Serial.println("ERROR: Failed reading value of characteristic.");
    }
    return value;
  }

  BLERemoteService* getService(BLEUUID serviceUUID)
  {
    BLERemoteService* pService = pClient->getService(serviceUUID);
    if (pService == nullptr)
        Serial.printf("ERROR: Failed to get service UUID: %s. Check if UUID is correct.\n", serviceUUID.toString().c_str());
    return pService;
  }

  BLERemoteCharacteristic* getCharacteristic(BLERemoteService* pService, BLEUUID characteristicUUID)
  {
    BLERemoteCharacteristic* pCharacteristic = pService->getCharacteristic(characteristicUUID);
    if(pCharacteristic == nullptr)
      Serial.printf("ERROR: Failed to get characteristic UUID: %s. Check if the characteristic UUID is correct and exists in given service UUID: %s.\n", characteristicUUID.toString().c_str(), pService->getUUID().toString().c_str());
    return pCharacteristic;
  }

  void writeValue(BLEUUID serviceUUID, BLEUUID characteristicUUID, uint8_t* pCMD, int cmdLength)
  {
    printDebugWriteValue(serviceUUID.toString(), characteristicUUID.toString(), pCMD, cmdLength);
    BLERemoteService* pService = getService(serviceUUID);
    BLERemoteCharacteristic* pCharacteristic = pService->getCharacteristic(characteristicUUID);
    pCharacteristic->writeValue(pCMD, cmdLength, true);
  }
  
  void disconnectOnError()
  {
    Serial.println("Disconnecting BLE connection due to error.");
    pClient->disconnect();
    connected = false;
  }
  

  // Debug prints--------------------------------------------------------------------------------------------
  void printDebugWriteValue(std::string serviceUUID, std::string characteristicUUID, uint8_t cmd[], int cmdLength)
  {
    #ifdef DEBUG
    Serial.printf("DEBUG: Writing the following %d bytes: ' ", cmdLength);
    for(int i = 0; i < cmdLength; i++)
      Serial.printf("%02x ", cmd[i] & 0xff);
    Serial.printf("' to characteristicUUID = %s of serviceUUID = %s \n", characteristicUUID.c_str(), serviceUUID.c_str());
    #endif
  }

  void printDebugReadValue(std::string serviceUUID, std::string characteristicUUID)
  {
    #ifdef DEBUG
    Serial.printf("DEBUG: Reading value of characteristicUUID = %s of serviceUUID = %s:\n", serviceUUID.c_str(), characteristicUUID.c_str()); 
    #endif
  }

  void printDebugHexValue(std::string value, int len)
  {
    #ifdef DEBUG
     Serial.printf("DEBUG: Value length n = %d, Hex: ", len);
      for (int i = 0; i < len; i++)
        Serial.printf("%02x ", (int)value[i]); 
    Serial.println(" ");
    #endif
  }
};

static FlowerCare* flowerCare;

// BUILT-IN: setup to init and loop for updates ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  flowerCare = new FlowerCare();
  //scanBLE();
}

void loop() {
  // 2. Connect to flower care server
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    flowerCare->connectToServer();
    doConnect = false;
  }

  if(connected == true)
  {
    Serial.printf("Name: %s\n", flowerCare->getName().c_str());
    Serial.printf("Mac address: %s\n", flowerCare->getMacAddress().c_str());
    Serial.printf("Battery level: %d %%\n", flowerCare->getBatteryLevel());
    Serial.printf("Firmware version: %s\n", flowerCare->getFirmwareVersion().c_str());
    Serial.printf("Real time data: \n%s\n", flowerCare->getRealTimeData()->toString().c_str());
    Serial.printf("Historical data: \n");
    int epochTimeInSeconds = flowerCare->getEpochTimeInSeconds();
    Serial.printf("Epoch time: %d seconds or ", epochTimeInSeconds);
    flowerCare->printSecondsInDays(epochTimeInSeconds); 
    std::vector<HistoricalEntry> historicalEntries = flowerCare->getHistoricalData();
    for(std::vector<HistoricalEntry>::size_type i = 0; i != historicalEntries.size(); i++)
      Serial.printf("Entry #%d:\n%s\n", i, historicalEntries[i].toString().c_str());
    delay(20000);
    connected = false; 
  }
   delay(10000);
}
