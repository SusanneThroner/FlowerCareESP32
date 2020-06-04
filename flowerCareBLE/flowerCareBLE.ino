#include "BLEDevice.h"
// Usage
// Open Serial Monitor: Tools > Serial Monitor
// Then upload to your ESP 32

// Resource
// https://github.com/vrachieru/xiaomi-flower-care-api
// https://github.com/sidddy/flora/blob/master/flora/flora.ino

// Uncomment to get more information printed out
//#define DEBUG 0
#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(x,y)  Serial.printf(x,y)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x,y)
#endif

// sensor address
//std::string macAddress = "C4:7C:8D:6A:8E:CA";
// The remote service we wish to connect to,
// basically that's the very first service (unknown service) of BLE device, root service used for connecting
// advertised Service used to discover
static BLEUUID serviceUUID("0000fe95-0000-1000-8000-00805f9b34fb");
// Service of battery level, firmware version and real-time sensor data
static BLEUUID dataServiceUUID("00001204-0000-1000-8000-00805f9b34fb");
// Handle
static BLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");
// Characteristics
// Firmware version and battery level with properties: READ
static BLEUUID uuid_version_battery("00001a02-0000-1000-8000-00805f9b34fb");
// Real-time sensor data with properties: READ, WRITE, NOTIFY
static BLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");

// Service #2
static BLEUUID historicalServiceUUID("00001206-0000-1000-8000-00805f9b34fb");
static BLEUUID historicalWriteUUID("00001210-0000-1000-8000-00805f9b34fb");
// Device time
static BLEUUID uuid_device_time("00001a12-0000-1000-8000-00805f9b34fb");
// Historical sensor values 
static BLEUUID uuid_historical_sensor_data("00001a11-0000-1000-8000-00805f9b34fb");

// HANDLES
//static *uint8_t[2] _CMD_REAL_TIME_READ_INIT = {0xa0, 0x1f};//bytes([0xa0, 0x1f]);//uint8_t buf[2] = {0xA0, 0x1F};
//static auto _HANDLE_FIRMWARE_AND_BATTERY = 0x38;
// First advertise: scan for device
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
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.printf("BLE Advertised Device found: \n%s\n", advertisedDevice.toString().c_str());

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
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
  Serial.printf("Scanning for BLE devices with advertisedServiceUUID =  %s...\n", serviceUUID.toString().c_str());
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

bool setServiceInDataMode(BLERemoteService* service)
{
  DEBUG_PRINTF("Getting real-time sensor data: Writing to characteristic %s in order to read values...\n", uuid_write_mode.toString().c_str());
  BLERemoteCharacteristic* characteristic = service->getCharacteristic(uuid_write_mode);
  if(characteristic == nullptr)
  {
    Serial.println("- ERROR: failed getting characteristic.");
    return false;
  }
  DEBUG_PRINTLN("- Writing to characteristic and wait half a second.");
  // write the magic data
  //static *uint8_t[2] _CMD_REAL_TIME_READ_INIT = {0xa0, 0x1f};//bytes([0xa0, 0x1f]);//uint8_t buf[2] = {0xA0, 0x1F};
   uint8_t _CMD_REAL_TIME_READ_INIT[2] = {0xA0, 0x1F};
  characteristic->writeValue(_CMD_REAL_TIME_READ_INIT, 2, true);

 // delay(500);
  return true;
}

void readRequest(BLERemoteService* service, uint8_t* handle)
{
  Serial.println("Read request from client:");
 // Serial.println(handle);
  BLERemoteCharacteristic* characteristic = service->getCharacteristic(uuid_write_mode);
  if(characteristic == nullptr)
  {
    Serial.println("- ERROR: failed getting characteristic.");
  }
  else
  {
    Serial.println("- Writing to characteristic and wait half a second.");
    delay(500);
    characteristic->writeValue(handle, 1, true);
  }
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
   Serial.printf("Value length n = %d, Hex: ", len);
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

void printBatteryAndFirmwareVersion(BLERemoteCharacteristic* characteristic)
{
  std::string value;
  try{
      value = characteristic->readValue();
      const char *printedValue = value.c_str();
      //Serial.println(printedValue); // returns d+3.2.2
      printDebugHex(printedValue, value.length()); //  Hex: 64 2B 33 2E 32 2E 32 
      
      int battery = printedValue[0]; // first value, here 64, with value integer is battery level represnting 100 % battery level
      Serial.printf("Battery level is: %d %% \n", battery);
      
      const char* firmwareVersion = &printedValue[2];
      Serial.printf("Firmware version is: %s \n", firmwareVersion);
      //Serial.println(firmwareVersion); // Here it's second (the one after battery level is unknown) with value char array, so here 3.2.2
    }
    catch(...)
    {
      Serial.println("ERROR: failed reading battery level.");
    }
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
    // DATA SERVICE #2
    Serial.println("# Service: Device time and history data");
    service = getService(historicalServiceUUID);

    // Device time
    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_device_time);
    uint32_t deviceBootTime = read_uint32_CharacteristicValue(pRemoteCharacteristic);
    Serial.printf("Epoch time = %" PRIu32 " seconds since boot or ", deviceBootTime);
    printSecondsInDays(deviceBootTime);

     // Historical sensor data
//    Serial.println("- History sensor data");
//    uint8_t historicalTimeHandle = 0x3e;
//    readRequest(service, &historicalTimeHandle);
//    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_historical_sensor_data); // ERROR: failed to get sensor data characteristic. if service does not have this characteristic
//    std::string value = readCharacteristicValue(pRemoteCharacteristic);
    
    // Data service #1
    Serial.println("\n# Service: Firmware version, battery level, real-time sensor data");
    service = getService(dataServiceUUID);
    
    // Characteristic with battery level and firmware version
    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_version_battery);
    printBatteryAndFirmwareVersion(pRemoteCharacteristic);

    // Real-time sensor data characteristic
    if(setServiceInDataMode(service)==true)
    {
      pRemoteCharacteristic = getCharacteristicOfService(service, uuid_sensor_data);

      if(pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

      //DEBUG_PRINTF("DEBUG: Real-time sensor data characteristic uuid = %s", uuid_sensor_data.toString().c_str());
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
    }
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
