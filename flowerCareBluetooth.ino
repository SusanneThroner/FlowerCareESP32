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
// Real-time sensor data with properties: READ, WRITE, NOTIFY
static BLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
// Firmware version and battery level with properties: READ
static BLEUUID uuid_version_battery("00001a02-0000-1000-8000-00805f9b34fb");
// The characteristic of the remote service we are interested in.
// battery and firmware
static BLEUUID charUUID("00000002-0000-1000-8000-00805f9b34fb");

// HANDLES
//static *uint8_t[2] _CMD_REAL_TIME_READ_INIT = {0xa0, 0x1f};//bytes([0xa0, 0x1f]);//uint8_t buf[2] = {0xA0, 0x1F};
static auto _HANDLE_FIRMWARE_AND_BATTERY = 0x38;
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
/*
    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      */

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
   Serial.print("Hex: ");
    for (int i = 0; i < len; i++) {
    Serial.print((int)value[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
}

void readFloraDataCharacteristic(BLERemoteService* service)
{
  BLERemoteCharacteristic* characteristic = service->getCharacteristic(uuid_sensor_data);
  if(characteristic == nullptr)
    Serial.println("ERROR: failed to get sensor data characteristic.");
  else
  {
    Serial.println("Found sensor data characteristic with value:");
    std::string value;
    characteristic->readValue();
      const char *val = value.c_str();

  Serial.print("Hex: ");
  for (int i = 0; i < 16; i++) {
    Serial.print((int)val[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");

  int16_t* temp_raw = (int16_t*)val;
  float temperature = (*temp_raw) / ((float)10.0);
  Serial.print("-- Temperature: ");
  Serial.println(temperature);

  float temp = ((float)val[0] + val[1] * 256) / 10;
  Serial.print("Temperature (in 0.1 Celsius): ");
  Serial.println(temp);

    int moisture = val[7];
  Serial.print("-- Moisture (in %): ");
  Serial.println(moisture);

  int light = val[3] + val[4] * 256;
  Serial.print("-- Light intensity (in lux): ");
  Serial.println(light);
 
  int conductivity = val[8] + val[9] * 256;
  Serial.print("-- Conductivity (in µS/cm): ");
  Serial.println(conductivity);
  }
}

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

void printBatteryAndFirmwareVersion(BLERemoteCharacteristic* characteristic)
{
  std::string value;
  try{
      value = characteristic->readValue();
      const char *printedValue = value.c_str();
      Serial.println(printedValue); // returns d+3.2.2
      printHex(printedValue, value.length()); //  Hex: 64 2B 33 2E 32 2E 32 
      int battery = printedValue[0]; // first value, here 64, with value integer is battery level represnting 100 % battery level
      Serial.println("Battery level is:");
      Serial.println(battery);
      const char* firmwareVersion = &printedValue[2];
      Serial.println("Firmware version is:");
      Serial.println(firmwareVersion); // Here it's second (the one after battery level is unknown) with value char array, so here 3.2.2
    }
    catch(...)
    {
      Serial.println("ERROR: failed reading battery level.");
    }
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
    // Data service
    service = getService(dataServiceUUID);
    // Characteristic with battery level and firmware version
    pRemoteCharacteristic = getCharacteristicOfService(service, uuid_version_battery);
    printBatteryAndFirmwareVersion(pRemoteCharacteristic);
    // Real-time sensor data characteristic
    if(setServiceInDataMode(service)==true)
    {
      pRemoteCharacteristic = getCharacteristicOfService(service, uuid_sensor_data);
      std::string value = readCharacteristicValue(pRemoteCharacteristic);
    }    
    
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
