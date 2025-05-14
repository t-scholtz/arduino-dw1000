#include "DeviceManager.h"
#include <string.h>

DeviceManager::DeviceManager()
{
    _deviceCount = 0;
}

bool DeviceManager::addDevice(DW1000Device *device, bool checkShortAddress)
{
    if (!device)
        return false;

    uint16_t newAddr = (device->getByteShortAddress()[1] << 8) | device->getByteShortAddress()[0];

    // Check for duplicates or reactivations
    for (uint8_t i = 0; i < _deviceCount; i++)
    {
        uint16_t existingAddr = (_devices[i].getByteShortAddress()[1] << 8) | _devices[i].getByteShortAddress()[0];

        if (checkShortAddress)
        {
            if (newAddr == existingAddr)
            {
                if (!_devices[i].isActive())
                {
                    _devices[i].setActive();
                    _devices[i].noteActivity();
                    Serial.print("[DeviceManager] Reactivated existing device: ");
                    Serial.println(newAddr, HEX);
                    return true;
                }
                Serial.print("[DeviceManager] Device already active: ");
                Serial.println(newAddr, HEX);
                return false; // Already active
            }
        }
        else
        {
            if (_devices[i].isAddressEqual(device))
            {
                Serial.println("[DeviceManager] Full address match, already in list.");
                return false;
            }
        }
    }

    if (_deviceCount >= MAX_DEVICES)
    {
        Serial.println("[ERROR] Over Max Devices");
        return false;
    }

    device->setRange(0);
    device->setIndex(_deviceCount);
    device->setActive();
    _devices[_deviceCount] = *device;
    _deviceCount++;

    Serial.print("[DeviceManager] Added new device: ");
    Serial.println(newAddr, HEX);

    return true;
}


void DeviceManager::removeDevice(int16_t index)
{
    if (index < 0 || index >= _deviceCount)
        return;

    for (int16_t i = index; i < _deviceCount - 1; i++)
    {
        memcpy(&_devices[i], &_devices[i + 1], sizeof(DW1000Device));
        _devices[i].setIndex(i);
    }
    _deviceCount--;
}

DW1000Device *DeviceManager::getDevice(int16_t index)
{
    if (index < 0 || index >= _deviceCount)
        return nullptr;
    return &_devices[index];
}

DW1000Device *DeviceManager::getDeviceByShortAddress(byte shortAddress[])
{
    uint16_t a = (shortAddress[1] << 8) | shortAddress[0];
    for (uint8_t i = 0; i < _deviceCount; i++)
    {
        uint16_t b = (_devices[i].getByteShortAddress()[1] << 8) | _devices[i].getByteShortAddress()[0];
        if (a == b)
        {
            return &_devices[i];
        }
    }
    return nullptr;
}


void DeviceManager::checkForInactiveDevices(void (*handleInactive)(DW1000Device *))
{
    for (int i = 0; i < _deviceCount; i++)
    {
        DW1000Device* dev = &_devices[i];

        // Mark inactive if no activity and still active
        if (dev->isInactive() && dev->isActive())
        {
            if (handleInactive)
            {
                handleInactive(dev);
            }
            Serial.print("[INFO] Marking device inactive: ");
            Serial.println(dev->getShortAddress(), HEX);
            dev->setInactive(); // don't remove
        }

        // Extra: reset stuck RANGING devices
        if (dev->getTagState() == TAG_STATE_RANGING && (millis() - dev->getLastActivity() > 500))
        {
            dev->setTagState(TAG_STATE_IDLE);
            Serial.print("[TIMEOUT] Forcing IDLE on device: ");
            Serial.println(dev->getShortAddress(), HEX);
        }
    }
}


uint8_t DeviceManager::getDeviceCount()
{
    return _deviceCount;
}

void DeviceManager::reactivateDevice(byte shortAddress[])
{
    DW1000Device *dev = getDeviceByShortAddress(shortAddress);
    if (dev)
        dev->setActive();
}
