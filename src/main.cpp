#include <Arduino.h>
#include <ChronosESP32.h>
#include <NimBLEDevice.h>
#include <Ticker.h>

//  ------------------------------ PINS START  -------------------------
#define BUZZER 4
#define PHN_LED 5
#define MSG_LED 19
#define DOORBELL_LED 21
#define BAND_LED 23
#define CTRL_BUTTON 25
//  ------------------------------- PINS END -------------------------------

// --------------------------------- Timings START---------------------------
#define PHN_LED_TIME 7
#define MSG_LED_TIME 2
#define DOORBELL_LED_TIME 3
#define BAND_LED_TIME 3
#define BUZZER_TIME 1
// ---------------------------------- Timings ENDS -----------------------------

#define SubBandMac "28:05:a5:33:3b:d2"
#define DoorBellMac "00:00:00:00:00:00"


// BLE
static const NimBLEAdvertisedDevice* smartband;
static const NimBLEAdvertisedDevice* doorBell;
static bool doConnect  = false;
static bool doConnectDoorbell = false;
static uint32_t scanTimeMs = 5000;


static NimBLERemoteCharacteristic* pRemoteChar = nullptr;
static NimBLERemoteCharacteristic* pdoorChar = nullptr;

#define toBAND_SERVICE "5df0d625-9efa-458f-b41b-06b2fbb9eed7"
#define toBAND_CHAR "78033c90-2bf0-4a36-a99a-44732377d115"


#define toDOORBELL_SERVICE "5df0d000-9efa-458f-b000-06b2fbb9eed7" 
#define toDOORBELL_CHAR "78033c44-2111-4a36-a111-44732377d115"


ChronosESP32 band("Deaf And Mute Band");

// --------------------------------- Ticker Starts -----------------------------
Ticker PHNLED_T, MSGLED_T, BANDLED_T, DOORBELL_LED_T, BUZZER_T;

void PHNLED_OFF() {
  PHNLED_T.detach();
  digitalWrite(PHN_LED, LOW);
  Serial.println("Phone Call Ended....");
}

void MSGLED_OFF() {
  MSGLED_T.detach();
  digitalWrite(MSG_LED, LOW);
  Serial.println("Message Call Ended....");
}


void BANDLED_OFF() {
  BANDLED_T.detach();
  digitalWrite(BAND_LED, LOW);
  Serial.println("Band Call Ended....");
}

void DOORBELL_LED_OFF() {
  DOORBELL_LED_T.detach();
  digitalWrite(DOORBELL_LED, LOW);
  Serial.println("Doorbell Call Ended....");
}

void BUZZER_OFF() {
  BUZZER_T.detach();
  digitalWrite(BUZZER, LOW);
  Serial.println("Buzzer Call Ended....");
}
// --------------------------------- Ticker Ends -----------------------------

// ------------------------------- Controller Methods Start -------------------------------
void phoneCall() {
  Serial.println("Phone Call Started ....");
  PHNLED_T.detach(); // prevent stacking
  digitalWrite(PHN_LED, HIGH);
  PHNLED_T.once(PHN_LED_TIME, PHNLED_OFF);
}

void message() {
  Serial.println("Message Call Started ....");
  MSGLED_T.detach();
  digitalWrite(MSG_LED, HIGH);
  MSGLED_T.once(MSG_LED_TIME, MSGLED_OFF);
}

void bandCall() {
  Serial.println("Band Call Started ....");
  BANDLED_T.detach();
  digitalWrite(BAND_LED, HIGH);
  BANDLED_T.once(BAND_LED_TIME, BANDLED_OFF);
}

void doorBellCall() {
  Serial.println("Doorbell Call Started ....");
  DOORBELL_LED_T.detach();
  digitalWrite(DOORBELL_LED, HIGH);
  DOORBELL_LED_T.once(DOORBELL_LED_TIME, DOORBELL_LED_OFF);
}

void buzzer() {
  Serial.println("Buzzer Call Started ....");
  BUZZER_T.detach();
  digitalWrite(BUZZER, HIGH);
  BUZZER_T.once(BUZZER_TIME, BUZZER_OFF);
}

// ------------------------------- Controller Methods END --------------------------

// ---------------------------------------- Chronos --------------------------------
void connectionCallback(bool state) {
  Serial.print("Connection state: ");
  Serial.println(state ? "Connected" : "Disconnected");
  if (state) {
    message();
  }
}

void notificationCallback(Notification notification) { message(); }
void ringerCallback(String caller, bool state) {
  if (state) {
    Serial.print("Ringer: Incoming call from ");
    Serial.println(caller);
    phoneCall();
  } else {
    Serial.println("Ringer dismissed");
    message();
  }
}

void configCallback(Config config, uint32_t a, uint32_t b) {
  if (config == CF_ALARM) {
    Serial.print("Alarm: Index ");
    Serial.print(a);
    Serial.print("\tTime: ");
    Serial.print(uint8_t(b >> 24));
    Serial.print(":");
    Serial.print(uint8_t(b >> 16));
    Serial.print("\tRepeat: ");
    Serial.print(uint8_t(b >> 8));
    Serial.print("\tEnabled: ");
    Serial.println((uint8_t(b)) ? "ON" : "OFF");
  }
}



void dataCallback(uint8_t *data, int length) {
  Serial.println("Received Data");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
}

// ---------------------------------------- Chronos -------------------------------------



// BLE 
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override { 
      // Band
        std::string peerAddr = pClient->getPeerAddress().toString().c_str();
        //   if (pClient->getPeerAddress().toString().c_str() == "00....00"){
        if(peerAddr == SubBandMac){
            bandCall();
        }
        // Doorbell
        //   if (pClient->getPeerAddress().toString().c_str() == "28:05:a5:33:3b:d2"){
        if(peerAddr == DoorBellMac){
            doorBellCall();
            
        }
        
        Serial.println(peerAddr.c_str());
    }



    void onDisconnect(NimBLEClient* pClient, int reason) override {
        bool m = true;
        // Band 
        std::string peerAddr = pClient->getPeerAddress().toString().c_str();
        if (peerAddr == SubBandMac){
            bandCall();
            m = true;
        }
        // Doorbell
        if (peerAddr == DoorBellMac){
            m = true;
            doorBellCall();
        }

        if(m){
            Serial.println("Device Not Found");
        }
    }
} clientCallbacks;




/** Define a class to handle the callbacks when scan events are received */
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(toBAND_SERVICE))) {
            Serial.printf("Found Band Our Service\n");
            NimBLEDevice::getScan()->stop();
            smartband = advertisedDevice;
            doConnect = true;
        }
        
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(toDOORBELL_SERVICE))) {
            Serial.printf("Found Doorbell Our Service\n");
            NimBLEDevice::getScan()->stop();
            doorBell = advertisedDevice;
            doConnectDoorbell = true;
        }
    }

    /** Callback to process the results of the completed scan or restart it */
    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    std::string peerAddr = pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    bool flag = true;
    Serial.print("Notification Received From: ");
    Serial.println(peerAddr.c_str());
    // Band 
    if (peerAddr == SubBandMac) {
        Serial.println("Band Server Sent Notification");
        flag = false;
    }
    // Doorbell
    if (peerAddr == DoorBellMac) {
        doorBellCall();
        // Serial.println("Doorbell Band Server Sent Notification");
        flag = false;
    }
    if(flag) {
        Serial.println("Unknown Server Notification");
        Serial.println(peerAddr.c_str());
    }
}

bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    if (NimBLEDevice::getCreatedClientCount()) {
        pClient = NimBLEDevice::getClientByPeerAddress(smartband->getAddress());
        if (pClient) {
            if (!pClient->connect(smartband, false)) {
                Serial.printf("Reconnect failed\n");
                return false;
            }
            Serial.printf("Reconnected client\n");
        } else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
            Serial.printf("Max clients reached - no more connections available\n");
            return false;
        }
        pClient = NimBLEDevice::createClient();
        Serial.printf("New client created\n");
        pClient->setClientCallbacks(&clientCallbacks, false);
        pClient->setConnectionParams(12, 12, 0, 150);
        pClient->setConnectTimeout(5 * 1000);
        if (!pClient->connect(smartband)) {
            NimBLEDevice::deleteClient(pClient);
            Serial.printf("Failed to connect, deleted client\n");
            return false;
        }
    }
    if (!pClient->isConnected()) {
        if (!pClient->connect(smartband)) {
            Serial.printf("Failed to connect\n");
            return false;
        }
    }
    /** Now we can read/write/subscribe the characteristics of the services we are interested in */
    NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor*     pDsc = nullptr;

    pSvc = pClient->getService(toBAND_SERVICE);
    if (pSvc) {
        pRemoteChar = pSvc->getCharacteristic(toBAND_CHAR);
        pChr = pRemoteChar;
    }

    if (pChr) {
        if (pChr->canRead()) {
            Serial.printf("%s Value: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
        }

        if (pChr->canNotify()) {
            if (!pChr->subscribe(true, notifyCB)) {
                pClient->disconnect();
                return false;
          }
        } else if (pChr->canIndicate()) {
            /** Send false as first argument to subscribe to indications instead of notifications */
            if (!pChr->subscribe(false, notifyCB)) {
                pClient->disconnect();
                return false;
            }
        }
    } else {
        Serial.printf("Band service not found.\n");
    }
    return true;
}





bool connectToServerDoorbell() {
    NimBLEClient* dClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getCreatedClientCount()) {
        dClient = NimBLEDevice::getClientByPeerAddress(doorBell->getAddress());
        if (dClient) {
            if (!dClient->connect(doorBell, false)) {
                Serial.printf("Doorbell Reconnect failed\n");
                return false;
            }
            Serial.printf("Doorbell Reconnected client\n");
        } else {
            dClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!dClient) {
        if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
            Serial.printf("Max clients reached - no more connections available\n");
            return false;
        }

        dClient = NimBLEDevice::createClient();
        Serial.printf("Doorbell New client created\n");
        dClient->setClientCallbacks(&clientCallbacks, false);
        dClient->setConnectionParams(12, 12, 0, 150);
        dClient->setConnectTimeout(5 * 1000);
        if (!dClient->connect(doorBell)) {
            NimBLEDevice::deleteClient(dClient);
            Serial.printf("Failed to connect, Doorbell deleted client\n");
            return false;
        }
    }

    if (!dClient->isConnected()) {
        if (!dClient->connect(doorBell)) {
            Serial.printf("Doorbell Failed to connect\n");
            return false;
        }
    }
    Serial.println("Connected to Doorbell Server");

    NimBLERemoteService*        dSvc = nullptr;
    NimBLERemoteCharacteristic* dChr = nullptr;
    NimBLERemoteDescriptor*     dDsc = nullptr;

    dSvc = dClient->getService(toDOORBELL_SERVICE);
    if (dSvc) {
        pdoorChar = dSvc->getCharacteristic(toDOORBELL_CHAR);
        dChr = pdoorChar;
    }

    if (dChr) {
        if (dChr->canNotify()) {
            if (!dChr->subscribe(true, notifyCB)) {
                dClient->disconnect();
                Serial.println("Subscription Notification Failed");
                return false;
            }
        } else if (dChr->canIndicate()) {
            if (!dChr->subscribe(false, notifyCB)) {
                dClient->disconnect();
                Serial.println("Subscription Indication Failed");
                return false;
            }
        }
    } else {
        Serial.printf("DOORBELL service not found.\n");
    }

    
    return true;
}
// BLE









void setup() {
    Serial.begin(115200);

    band.setConnectionCallback(connectionCallback);
    band.setNotificationCallback(notificationCallback);
    band.setRingerCallback(ringerCallback);
    band.setDataCallback(dataCallback);
    band.begin();

    band.setBattery(80);
    



    // BLE
    NimBLEDevice::init("NimBLE-Client");
    NimBLEDevice::setPower(3); /** 3dbm */
    NimBLEScan* pScan = NimBLEDevice::getScan();
    /** Set the callbacks to call when scan events occur, no duplicates */
    pScan->setScanCallbacks(&scanCallbacks, false);
    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setActiveScan(true);
    pScan->start(scanTimeMs);
    Serial.printf("Scanning for peripherals\n");
  // BLE



  // ---------------------------------- PINS AND MODDES -----------------------------------
    pinMode(CTRL_BUTTON, INPUT_PULLUP);
    pinMode(PHN_LED, OUTPUT);
    pinMode(MSG_LED, OUTPUT);
    pinMode(BAND_LED, OUTPUT);
    pinMode(DOORBELL_LED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
  // ---------------------------------- PINS AND MODDES ---------------------------------

    Serial.println("STARTING");
}

void loop() {
    band.loop();
    static bool lastState = HIGH;
    bool currentState = digitalRead(CTRL_BUTTON);

    if (currentState == LOW && lastState == HIGH) {
        Serial.println("Button Pressed!");

        if (pRemoteChar && pRemoteChar->canRead()) {
            std::string value = pRemoteChar->readValue();
            Serial.print("Read Value: ");
            Serial.println(value.c_str());
            buzzer();
        } else {
            Serial.println("Characteristic not available");
        }
    }

    lastState = currentState;


  




    if (doConnect) {
        doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            Serial.printf("Band Success! we should now be getting notifications, scanning for more!\n");
        } else {
            Serial.printf("Band Failed to connect, starting scan\n");
        }

        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
    if (doConnectDoorbell) {
        doConnectDoorbell = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServerDoorbell()) {
            Serial.printf("Doorbell Success! we should now be getting notifications, scanning for more!\n");
        } else {
            Serial.printf("Doorbell Failed to connect, starting scan\n");
        }

        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }




    //  chronos 
    String time = band.getHourC() + band.getTime(":%M ") + band.getAmPmC();
    Serial.println(time);
    delay(500);
    digitalWrite(PHN_LED, band.isAnyAlarmActive());
    //  chronos 
}

