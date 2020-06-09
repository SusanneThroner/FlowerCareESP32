#include "BLEDevice.h"
// Usage:
// Select your ESP32 board under: Tools > "Board: [current selected board]" > i.e. "NodeMCU-32s"
// Open Serial Monitor: Tools > Serial Monitor
// Then upload to your ESP 32

// Resource
// https://github.com/vrachieru/xiaomi-flower-care-api
// https://github.com/sidddy/flora/blob/master/flora/flora.ino

// Uncomment to get more information printed out
#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(x,y)  Serial.printf(x,y)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x,y)
#endif

// Service #0: Root service used for advertising and connecting
static BLEUUID serviceAdvertisementUUID("0000fe95-0000-1000-8000-00805f9b34fb");

// Service #1: Real-time sensor data, battery level and firmware version
// Real-time sensor data with properties: READ, WRITE, NOTIFY
static BLEUUID serviceBatteryFirmwareRealTimeDataUUID("00001204-0000-1000-8000-00805f9b34fb");
// Read request needs to be written prior to be able to read real-time data.
uint8_t CMD_REAL_TIME_READ_INIT[2] = {0xA0, 0x1F}; //  I do not know why it's these values.
static BLEUUID characteristicReadRequestToRealTimeDataUUID("00001a00-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicRealTimeDataUUID("00001a01-0000-1000-8000-00805f9b34fb");
// Firmware version and battery level with properties: READ
static BLEUUID characteristicFirmwareAndBatteryUUID("00001a02-0000-1000-8000-00805f9b34fb");

// Service #2: Epoch time and Historical data
static BLEUUID serviceHistoricalDataUUID("00001206-0000-1000-8000-00805f9b34fb");
// Characteristic to write a read request to prior to be able to read the historical sensor data values.
static BLEUUID characteristicReadRequestToHistoricalDataUUID("00001a10-0000-1000-8000-00805f9b34fb");//("00001210-0000-1000-8000-00805f9b34fb");
uint8_t CMD_HISTORY_READ_INIT[3] = {0xa0, 0x00, 0x00};
uint8_t CMD_HISTORY_READ_ENTRY[3] = {0xa1, 0x00, 0x00};
static BLEUUID characteristicHistoricalDataUUID("00001a11-0000-1000-8000-00805f9b34fb");
// Epoch time values can be directly read of this UUID returning time in seconds since device boot.
static BLEUUID characteristicEpochTimeUUID("00001a12-0000-1000-8000-00805f9b34fb");

// Then connect to server
static BLERemoteService* service;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static BLEClient*  pClient;

// Flags
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean serviceFound = false;

// CLASSES -------------------------------------------------------------------------------------------
class RealTimeEntry
{
  public:
  RealTimeEntry(std::string value)
  {
    const char *val = value.c_str();
    temperature = ((int16_t*) val)[0];    // 3 bytes 00-02, last byte is sign
    brightness = *(uint32_t*) (val+3);    // 4 bytes 03-06
    moisture = *(uint8_t*) (val+7);       // 1 byte  07
    conductivity = *(uint16_t*) (val+8);  // 2 bytes 08-09
    // bytes 10-15 unknown
  }
  std::string toString()
  {
    char buffer[120];
    int count = 0;
    count += snprintf(buffer, sizeof(buffer), "Temperature: %2.1f °C\n", ((float)this->temperature)/10);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Brightness: %u lux\n", this->brightness);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Soil moisture: %d \n", this->moisture);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Conductivity: %" PRIu16 " µS/cm \n", this->conductivity);
    return (std::string)buffer;
  }
  int16_t temperature; // in 0.1 °C
  uint32_t brightness; // in lux
  uint8_t moisture; // in %
  uint16_t conductivity; // in µS/cm
};

class HistoricalEntry
{
  public:
  HistoricalEntry(std::string value)
  {
    const char *val = value.c_str();
    timestamp = ((uint32_t*) val)[0];     // 4 bytes 00-03
    temperature = *(int16_t*) (val+4);    // 3 bytes 04-06, last byte is sign
    brightness = *(uint32_t*) (val+7);    // 4 bytes 07-10
    moisture = *(uint8_t*) (val+11);      // 1 byte  11
    conductivity = *(uint16_t*) (val+12); // 2 bytes 12-13
    // bytes 14-15 unknown
  }

  std::string toString()
  {
    char buffer[300];
    int count = 0;
    count += snprintf(buffer, sizeof(buffer), "Timestamp: %" PRIu32 " seconds\n", this->timestamp);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Temperature: %2.1f °C\n", ((float)this->temperature)/10);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Brightness: %u lux\n", this->brightness);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Soil moisture: %d \n", this->moisture);
    count += snprintf(buffer+count, sizeof(buffer)-count, "Conductivity: %" PRIu16 " µS/cm \n", this->conductivity);
    return (std::string)buffer;
  }

  uint32_t timestamp; // seconds since device epoch (boot)
  int16_t temperature; // in 0.1 °C
  uint32_t brightness; // in lux
  uint8_t moisture; // in %
  uint16_t conductivity; // in µS/cm
};

class FlowerCare {
  public:
  FlowerCare()
  {
    if(myDevice->haveName())
      _name = myDevice->getName();
    else
      _name = "Unnamed";

    _macAddress = myDevice->getAddress().toString();
  }
  
  std::string getMacAddress()
  {
    return _macAddress;
  }
  
  std::string getName()
  {
    return _name;
  }
  
  // Firmware and battery ouput example in Hex: "64 2B 33 2E 32 2E 32". 
  // Battery level is stored in very first hex: "64" represented in int as: 100%.
  int getBatteryLevel()
  {
     std::string value = readValue(serviceBatteryFirmwareRealTimeDataUUID, characteristicFirmwareAndBatteryUUID);
     _batteryLevel = (int)value[0];
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

  void getHistoricalData()
  {
    writeValue(serviceHistoricalDataUUID, characteristicReadRequestToHistoricalDataUUID, CMD_HISTORY_READ_INIT, 3);
    std::string value = readValue(serviceHistoricalDataUUID, characteristicHistoricalDataUUID);
    const char *val = value.c_str();
    uint16_t nHistoricalEntries = ((uint16_t*) val)[0];
    Serial.printf("Found %d history entries:\n", nHistoricalEntries);

    uint8_t entryAddress[3];
    memcpy(entryAddress, CMD_HISTORY_READ_ENTRY, 3);
    uint16_t* pEntry = (uint16_t*) &entryAddress[1];
    if(nHistoricalEntries > 0)
    {
      for(int i=0; i < nHistoricalEntries; i++)//nHistoricalEntries; i++)
      {
        writeValue(serviceHistoricalDataUUID, characteristicReadRequestToHistoricalDataUUID, entryAddress, 3);
        value = readValue(serviceHistoricalDataUUID, characteristicHistoricalDataUUID);
        HistoricalEntry* pHistoricalEntry = new HistoricalEntry(value);
        Serial.printf("Reading entry #%d of %d total entries in history:\n%s\n\n", i+1, nHistoricalEntries, pHistoricalEntry->toString().c_str());
        *pEntry += 1;
      }
    }
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
  int _batteryLevel;
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
      printDebugHexValue(value, value.length()); //  Hex example for battery and firmware: 64 2B 33 2E 32 2E 32 
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
      Serial.printf("ERROR: Failed to get characteristic UUID: %s. Check if the characteristic UUID is correct and exists in given service UUID: %s.\n", characteristicUUID.toString().c_str(), service->getUUID().toString().c_str());
    return pCharacteristic;
  }

  void writeValue(BLEUUID serviceUUID, BLEUUID characteristicUUID, uint8_t* pCMD, int cmdLength)
  {
    printDebugWriteValue(serviceUUID.toString(), characteristicUUID.toString(), pCMD, cmdLength);
    BLERemoteService* pService = getService(serviceUUID);
    BLERemoteCharacteristic* pCharacteristic = pService->getCharacteristic(characteristicUUID);
    pCharacteristic->writeValue(pCMD, cmdLength, true);
  }

  void printDebugWriteValue(std::string serviceUUID, std::string characteristicUUID, uint8_t cmd[], int cmdLength)
  {
    #ifdef DEBUG
    Serial.printf("DEBUG: Writing the following %d bytes: ' ", cmdLength);
    for(int i = 0; i < cmdLength; i++)
    {
      //Serial.print("0x"); // This prints with 0x before everything
      Serial.printf("%02x ", cmd[i] & 0xff);//Serial.printf("%d\n", cmd[i]);
    }
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
      for (int i = 0; i < len; i++) {
        //Serial.print("0x"); // This prints with 0x before everything
        if(value[i] < 16)
          Serial.print("0");
         Serial.print((int)value[i], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");
    #endif
  }
};

static FlowerCare* flowerCare;
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
      flowerCare = new FlowerCare();
      doConnect = true;
      doScan = true;

    } // Found our server
    else
    {
      DEBUG_PRINT("Service not found on this BLE device.");
      DEBUG_PRINTLN();
    }
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

// FUNCTIONS ----------------------------------------------------------------------------------------
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

void disconnectOnError()
{
  Serial.println("Disconnecting...");
  pClient->disconnect();
  connected = false;
}

void connectToServer() {
    Serial.printf("Create client and connecting to %s\n",myDevice->getAddress().toString().c_str());
    
    pClient  = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
}

BLERemoteService* getService(BLEUUID uuid)
{
     // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(uuid);
    if (pRemoteService == nullptr) {
      Serial.printf("ERROR: Failed to find service UUID: %s\n", uuid.toString().c_str());
      Serial.println("Make sure the service uuid is correctly and available on your device.\n");
      return pRemoteService;
    }
    // Service info
    // Real-time data service uuid: 00001204-0000-1000-8000-00805f9b34fb, start_handle: 49 0x0031, end_handle: 57 0x0039
    DEBUG_PRINTF("Found %s\n",pRemoteService->toString().c_str()); 
    //pRemoteService->getCharacteristics(); // returns nothing
    return pRemoteService;
}

BLERemoteCharacteristic* getCharacteristicOfService(BLERemoteService* service, BLEUUID characteristicUUID)
{
  BLERemoteCharacteristic* characteristic = service->getCharacteristic(characteristicUUID);
  if(characteristic == nullptr)
    Serial.println("ERROR: failed to get sensor data characteristic.");
  else
  {
    // Characteristic info
    // firmware version and battery Characteristic: uuid: 00001a02-0000-1000-8000-00805f9b34fb, handle: 56 0x0038, props: broadcast: 0, read: 1, write_nr: 0, write: 0, notify: 0, indicate: 0, auth: 0
    // Real-time sensor data Characteristic: uuid: 00001a01-0000-1000-8000-00805f9b34fb, handle: 53 0x0035, props: broadcast: 0, read: 1, write_nr: 0, write: 1, notify: 1, indicate: 0, auth: 0
    DEBUG_PRINT("Found ");
    DEBUG_PRINTLN(characteristic->toString().c_str());
  }

  return characteristic;
}

void printDebugHex(const char *value, int len)
{
  #ifdef DEBUG
   Serial.printf("DEBUG: Value length n = %d, Hex: ", len);
    for (int i = 0; i < len; i++) {
      //Serial.print("0x"); // This prints with 0x before everything
      if(value[i] < 16)
        Serial.print("0");
       Serial.print((int)value[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
  #endif
}

std::string readCharacteristicValue(BLERemoteCharacteristic* characteristic)
{
  std::string value;
  try{
      value = characteristic->readValue();
      const char *printedValue = value.c_str();
      //Serial.println(printedValue); // returns d+3.2.2
      printDebugHex(printedValue, value.length()); //  Hex example for battery and firmware: 64 2B 33 2E 32 2E 32 
    }
    catch(...)
    {
      Serial.println("ERROR: failed reading value of characteristic.");
    }
    return value;
}

uint32_t read_uint32_CharacteristicValue(BLERemoteCharacteristic* characteristic)
{
  uint32_t value = characteristic->readUInt32();
  return value;
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    /*
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
    */
}
// BUILT-IN: setup to init and loop for updates ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  delay(100);
  scanBLE();
  
  /*
  // initialize BLE and create client
  BLEDevice::init("");  // better do once for all objects
 // BLEDevice::setPower(ESP_PWR_LVL_P7); // TODO CHeck
    */
}

void loop() {
  // 2. Connect to flower care server
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    connectToServer();
    doConnect = false;
  }


  if(connected == true)
  {
    Serial.println("TEST");
    Serial.printf("Name: %s\n", flowerCare->getName().c_str());
    Serial.printf("Mac address: %s\n", flowerCare->getMacAddress().c_str());
    Serial.printf("Battery level: %d %%\n", flowerCare->getBatteryLevel());
    Serial.printf("Firmware version: %s\n", flowerCare->getFirmwareVersion().c_str());
    Serial.printf("Real time data: \n%s\n", flowerCare->getRealTimeData()->toString().c_str());
    Serial.printf("Historical data: \n");
    int epochTimeInSeconds = flowerCare->getEpochTimeInSeconds();
    Serial.printf("Epoch time: %d seconds or ", epochTimeInSeconds);
    flowerCare->printSecondsInDays(epochTimeInSeconds); 
    flowerCare->getHistoricalData();
    
    // DATA SERVICE #2
//    Serial.println("# Service: Device time and history data");
//    service = getService(historicalServiceUUID);

    // Device time
//    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_device_time);
//    uint32_t deviceBootTime = read_uint32_CharacteristicValue(pRemoteCharacteristic);
//    Serial.printf("Epoch time = %" PRIu32 " seconds since boot or ", deviceBootTime);
//    printSecondsInDays(deviceBootTime);

     // Historical sensor data
//    Serial.println("- History sensor data");
//    uint8_t historicalTimeHandle = 0x3e;
//    readRequest(service, &historicalTimeHandle);
//    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_historical_sensor_data); // ERROR: failed to get sensor data characteristic. if service does not have this characteristic
//    std::string value = readCharacteristicValue(pRemoteCharacteristic);

    // Data service #1
    /*Serial.println("\n# Service: Firmware version, battery level, real-time sensor data");
    service = getService(serviceBatteryFirmwareRealTimeDataUUID);
    
    // Characteristic with battery level and firmware version
    pRemoteCharacteristic = getCharacteristicOfService(service, characteristicFirmwareAndBatteryUUID);
    setServiceInDataMode(service);
    
    // Real-time sensor data characteristic
    if(setServiceInDataMode(service)==true)
    {
      pRemoteCharacteristic = getCharacteristicOfService(service, characteristicRealTimeDataUUID);

      if(pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

      //DEBUG_PRINTF("DEBUG: Real-time sensor data characteristic uuid = %s", characteristicRealTimeDataUUID.toString().c_str());
      std::string value = readCharacteristicValue(pRemoteCharacteristic);
      const char *val = value.c_str();

      int16_t temperature = ((int16_t*) val)[0];
      Serial.printf("Temperature: %2.1f °C\n", ((float)temperature)/10);
      
      uint32_t brightness = *(uint32_t*) (val+3);
      Serial.printf("Brightness: %u lux\n", brightness);

      uint8_t moisture = *(uint8_t*) (val+7);
      Serial.printf("Soil moisture: %d \n", moisture);

      uint16_t conductivity = *(uint16_t*) (val+8);
      Serial.printf("Conductivity: %" PRIu16 " µS/cm \n\n", conductivity);
      // No need to swap temperature
//     Serial.printf("Temperature in 0.1 degree Celsius: %" PRIu16 "\n", temperature); // Temperature in 0.1 degree Celsius: 288
    }*/
    // Temperatur: 0x1D 0x1 
    // ?: 0x0 
    // uint32 Brightness: 0xE9 0x2 0x0 0x0 
    // uint8 Soil moisture: 0x0 0x0
    // uint16 Conductivity: 0x0 0x2 0x3C 
    // 0x0 0xFB 0x34 0x9B  

    // Fail not in data mode: Hex: AA BB CC DD EE FF 99 88 77 66 
    // 18 1 0 92 0 0 0 1 0 0 2 3C 0 FB 34 9B  
   // Serial.print(value, HEX);
   // setServiceInDataMode(service);
    connected = false; 
  }
   delay(500);
}
