#ifndef A3A894A8_7557_423C_AF30_DF2AD1257EE6
#define A3A894A8_7557_423C_AF30_DF2AD1257EE6

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <sstream>
#include "data_packet.h"

#define RSSI_CONNECT -80
#define RSSI_DISCONNECT -90
#define SCAN_DURATION 1
#define SCAN_WINDOW 100
#define SCAN_INTERVAL 500

bool clientConnected = false; // indicates whether repeater is connected
boolean doServerConnect = false; // flag to enable trying to connect to a server
BLEAdvertisedDevice *peripheral_device;

// Callbacks for server's main characteristic between aggregator and repeater
class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  private:
    TransmissionData_t _data;

  public:
    CharacteristicCallbacks()
    {
    }

    void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param)
    {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0)
        {
            Serial.println("*********");
            Serial.print("New value: ");
            for (int i = 0; i < value.length(); i++)
                Serial.print(value[i]);

            Serial.println();
            Serial.println("*********");
        }
    }

    void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param)
    {
        std::stringstream str_data("");
        str_data << "TEMP: " << _data.temp_data << " LAT: " << _data.latitude << " LONG: " << _data.longitude
                 << " ALT: " << _data.altitude;

        Serial.println(str_data.str().c_str());

        pCharacteristic->setValue(str_data.str());
    }


    void onNotify(BLECharacteristic *pCharacteristic)
    {
        printf("NOTIFED\n");
    }

    void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code)
    {
        printf("STATUS\n");
    }

    TransmissionData_t getData()
    {
        return _data;
    }

    void setData(TransmissionData_t data)
    {
        _data = data;
    }
};

// Callbacks for overall server between aggregator and repeater
class ServerCallbacks : public BLEServerCallbacks
{
  public:
    void onConnect(BLEServer *pServer)
    {
        clientConnected = true;
        Serial.println("Repeater connected.\n");
        BLEDevice::stopAdvertising();
    }

    void onDisconnect(BLEServer *pServer)
    {
        clientConnected = false;
        Serial.println("Repeater disconnected.\n");
        BLEDevice::startAdvertising();
    }
};

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Only connect to the peripheral if:
        // - correct service is advertised
        // - RSSI is high enough
        // Match sensors based on name
        if (advertisedDevice.haveName() && advertisedDevice.getName() == "S0" && (advertisedDevice.getRSSI() >= RSSI_CONNECT))
        {
            peripheral_device = new BLEAdvertisedDevice(advertisedDevice);
            doServerConnect = true;
            Serial.println("Sensor found.");
        }
    }
};

class ClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        Serial.println("A sensor connected.");
    }

    void onDisconnect(BLEClient *pclient)
    {
        Serial.println("A sensor disconnected.");
    }
};

// main class for handling BLE 
template <typename _UUID_Generator_Type> class Bluetooth
{
  private:
    BLEServer *pServer;
    BLEService *pService;
    BLECharacteristic *pCharacteristic;
    BLEAdvertising *pAdvertising;
    _UUID_Generator_Type _uuid_gen_struct;
    BLEScan *pBLEScan;
    std::vector<BLEClient *> clients; // one BLEClient object per connected server

  public:
    CharacteristicCallbacks *callback_class;
    Bluetooth()
    {
    }

    Bluetooth(_UUID_Generator_Type uuid_gen_struct) : _uuid_gen_struct(uuid_gen_struct)
    {
        // Setup for server side (aggregator to repeater)
        callback_class = new CharacteristicCallbacks();
        BLEDevice::init("A0");
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new ServerCallbacks);
        pService = pServer->createService(_uuid_gen_struct.get_service_uuid());
        pCharacteristic = pService->createCharacteristic(
            _uuid_gen_struct.get_characteristic_uuid(),
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

        pCharacteristic->setCallbacks(callback_class);
        pCharacteristic->setValue("Agg Node");
        pService->start();
        pAdvertising = pServer->getAdvertising();
        pAdvertising->addServiceUUID(_uuid_gen_struct.get_service_uuid());
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0);
        pAdvertising->start();

        // Setup for client side (sensor to aggregator)
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
        pBLEScan->setInterval(SCAN_INTERVAL);
        pBLEScan->setWindow(SCAN_WINDOW);
        pBLEScan->setActiveScan(true);

    }

    // Run whenever a message is received from a sensor
    static void clientOnNotify(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length,
                           bool isNotify)
    {
        TransmissionData_t *data = new TransmissionData_t;
        *data = *(TransmissionData_t *)pData; // copy notified data to new location. Will be deleted before popped off queue.
        received_packets.push(data); // add to queue for upstream transmission
        Serial.print("Received data from sensor: ");
        // print entire recieved packet by byte
        for (int i = 0; i < length; i++)
        {
            Serial.print(*(pData + i), HEX);
        }
        Serial.print(". Length: ");
        Serial.println(length);

		delete data;
    }

    // Send a message using notify to the repeater
    void sendData()
    {
        if (clientConnected)
        {
            TransmissionDataConverter_t converter;
            converter.message = callback_class->getData();
            pCharacteristic->setValue(converter.bytes, sizeof(TransmissionData));
            pCharacteristic->notify();
        }
    }

    bool tryConnectToServer()
    {
        if (doServerConnect)
        {
            doServerConnect = false;
            Serial.print("Forming a connection to ");
            Serial.println(peripheral_device->getAddress().toString().c_str());

            BLEClient *pClient = BLEDevice::createClient();

            pClient->setClientCallbacks(new ClientCallback());

            // Connect to the remote BLE Server.
            pClient->connect(peripheral_device);
            Serial.println(" - Connected to server");
            pClient->setMTU(40); // arbitrary MTU. The max is supposedly 517

            // prep primary service object
            BLERemoteService *pRemoteService = pClient->getService(_uuid_gen_struct.get_service_uuid());
            if (pRemoteService == nullptr)
            {
                Serial.println("Incorrect service UUID");
                pClient->disconnect();
                return false;
            }

            // prep characteristic object (Just one for now. Later: add multiple for sharing other info (battery status etc.))
            BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(_uuid_gen_struct.get_characteristic_uuid());
            if (pRemoteCharacteristic == nullptr)
            {
                Serial.println("Incorrect characteristic UUID");
                pClient->disconnect();
                return false;
            }

            clients.push_back(pClient);
            // prep to receive notifications from peripheral
            pRemoteCharacteristic->registerForNotify(clientOnNotify);
            return true;
        }

        return false;
    }

    // Scan for available sensors
    void scan()
    {
        pBLEScan->start(SCAN_DURATION, false);
    }
    
    // Disconnect servers from the list that are no longer connected or are already disconnected
    void removeOldServers()
    {
        // one client object per connected server. Each BLEClient object represents a connection to a server.
        BLEClient *client;
        for (int i = 0; i < clients.size(); i++)
        {
            client = clients[i];
            if (client->getRssi() < RSSI_DISCONNECT) // remove clients with poor connection
            {
                Serial.println("RSSI fell below threshold. Disconnecting from server.");
                client->disconnect();
                clients.erase(clients.begin() + i);
                i--;
            } else if (!client->isConnected()) { // remove already disconnected clients
                clients.erase(clients.begin() + i);
                i--;
            }
        }
    }

    bool clientIsConnected()
    {
        return clientConnected;
    }

    bool hasSensors()
    {
        return clients.size() > 0;
    }
};

#endif /* A3A894A8_7557_423C_AF30_DF2AD1257EE6 */
