#include "BLEDevice.h"
// Usage
// Open Serial Monitor: Tools > Serial Monitor
// Then upload to your ESP 32

// Resource
// https://github.com/vrachieru/xiaomi-flower-care-api
// https://github.com/sidddy/flora/blob/master/flora/flora.ino
// sensor address
//#define FLORA_ADDR "C4:7C:8D:6A:8E:CA"
//std::string macAddress = "C4:7C:8D:6A:8E:CA";
//std::string serviceUUID = "00001204-0000-1000-8000-00805f9b34fb";
// The remote service we wish to connect to,
// basically that's the very first service (unknown service) of BLE device, root service used for connecting
// advertised Service used to discover
static BLEUUID serviceUUID("0000fe95-0000-1000-8000-00805f9b34fb");
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
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    Serial.println("Looking for service.");
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
    else
    {
      Serial.println("Service not found.");
    }
  } // onResult
}; // MyAdvertisedDeviceCallbacks

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("onConnect");
  }



  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

// FUNCTIONS ----------------------------------------------------------------------------------------
void scanBLE()
{
  Serial.println("Scanning...");
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
  Serial.println("Scanning done.");
}

void disconnectOnError()
{
  Serial.println("Disconnecting...");
  pClient->disconnect();
  connected = false;
}

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    connected = true;
    return true;
}

BLERemoteService* getService(BLEUUID uuid)
{
     // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(uuid);
    if (pRemoteService == nullptr) {
      Serial.print("ERROR: Failed to find service UUID: ");
      Serial.println(uuid.toString().c_str());
      return pRemoteService;
    }
    Serial.println(" - Found service.");
    // Service info
    // Real-time data service uuid: 00001204-0000-1000-8000-00805f9b34fb, start_handle: 49 0x0031, end_handle: 57 0x0039
    Serial.println(pRemoteService->toString().c_str()); 

    //pRemoteService->getCharacteristics(); // returns nothing
    return pRemoteService;
}

bool setServiceInDataMode(BLERemoteService* service)
{
  Serial.println("Get device mode characteristic type to READ");
  BLERemoteCharacteristic* characteristic = service->getCharacteristic(uuid_write_mode);
  if(characteristic == nullptr)
  {
    Serial.println("- ERROR: failed getting characteristic.");
    return false;
  }
  Serial.println("- Writing to characteristic and wait half a second.");
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
    Serial.println("Found characteristic:");
    Serial.println(characteristic->toString().c_str());
  }

  return characteristic;
}

void printHex(const char *value, int len)
{
   Serial.printf("Value length %d, \n", len);
   Serial.print("Hex: ");
    for (int i = 0; i < len; i++) {
      //Serial.print("0x");
    Serial.print((int)value[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
}

//void printRealtimeData(std::string data)
//{
//  Serial.println("- Realtime Data");
//  const char *val = data.c_str();
//  printHex(val, data.length());
//
//  uint16_t* temperature = val[0];
//  temperature = swap_uint16_Bytes(temperature);
//
////  int16_t* temp_raw = (int16_t*)val;
////  float temperature = (*temp_raw) / ((float)10.0);
////  Serial.print("-- Temperature raw: ");
////  Serial.println(temperature);
////
////  float temp = ((float)val[0] + val[1] * 256) / 10;
////  Serial.print("Temperature (in 0.1 Celsius): ");
////  Serial.println(temp);
//
//    int moisture = val[7];
//  Serial.print("-- Moisture (in %): ");
//  Serial.println(moisture);
//
//  int light = val[3] + val[4] * 256;
//  Serial.print("-- Light intensity (in lux): ");
//  Serial.println(light);
// 
//  int conductivity = val[8] + val[9] * 256;
//  Serial.print("-- Conductivity (in µS/cm): ");
//  Serial.println(conductivity);
//  
//}

std::string readCharacteristicValue(BLERemoteCharacteristic* characteristic)
{
  std::string value;
  try{
      value = characteristic->readValue();
      const char *printedValue = value.c_str();
      Serial.println(printedValue); // returns d+3.2.2
      printHex(printedValue, value.length()); //  Hex example for battery and firmware: 64 2B 33 2E 32 2E 32 
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
      Serial.println(printedValue); // returns d+3.2.2
      printHex(printedValue, value.length()); //  Hex: 64 2B 33 2E 32 2E 32 
      
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

uint32_t swapBytes(uint32_t value)
{
  uint8_t* pointer = (uint8_t*) &value;
  return ((uint32_t)pointer[3]) + ((uint32_t)pointer[2] << 8) + ((uint32_t)pointer[1] << 16) + ((uint32_t)pointer[0] << 24);
}

uint16_t swap_uint16_Bytes(uint16_t value)
{
  uint8_t* pointer = (uint8_t*) &value;
  return ((uint16_t)pointer[2]) + ((uint16_t)pointer[1] << 8) + ((uint16_t)pointer[0] << 16);
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
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
    Serial.println("Connecting to server...");
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if(connected == true)
  {
    Serial.println("Connected, getting services");
    // DATA SERVICE #2
    Serial.println("# Service: Device time and history data");
    service = getService(historicalServiceUUID);

    // Device time
    //uint8_t deviceTimeHandle = 0x41;
    //readRequest(service, &deviceTimeHandle);
    Serial.println("- Device time");
    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_device_time);
    uint32_t deviceBootTime = read_uint32_CharacteristicValue(pRemoteCharacteristic);
    // returns 1277055, same as hex = 00 13 7C 7F
    //Serial.printf("Time in seconds since boost: %" PRIu32 "\n", deviceBootTime);
    // Swap bytes because its encoded in little endian
    // returning 2138837760, sam as previous hex, but bytes are swapped: 7F 7C 13 00
    deviceBootTime = swapBytes(deviceBootTime);
    Serial.printf("Time in seconds since boost: %" PRIu32 "\n", deviceBootTime);

     // Historical sensor data
//    Serial.println("- History sensor data");
//    uint8_t historicalTimeHandle = 0x3e;
//    readRequest(service, &historicalTimeHandle);
//    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_historical_sensor_data); // ERROR: failed to get sensor data characteristic. if service does not have this characteristic
//    std::string value = readCharacteristicValue(pRemoteCharacteristic);
    
    // Data service #1
    Serial.println("# Service: Firmware version, battery level, real-time sensor data");
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
      
      std::string value = readCharacteristicValue(pRemoteCharacteristic);
      Serial.println("- Realtime Data");
      const char *val = value.c_str();
      printHex(val, value.length());

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
//      temperature = swap_uint16_Bytes(temperature);//Temperature after swap: 380
//      Serial.printf("Temperature after swap: %" PRIu16 "\n", temperature);
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


 
    /*
    if(serviceFound)
    {
      if(setServiceInDataMode(&service)==true)
      {
        Serial.println("Getting sensor data: TODO");
      }
    }
    else
      disconnectOnError();/*
  }
/*
  if(connected == true)
  {
    Serial.println("Connected, trying to access dataService...");
    BLERemoteService* dataService = pClient->getService(dataServiceUUID);
    if (dataService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(dataServiceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");
  }*/
}
