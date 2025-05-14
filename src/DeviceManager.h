#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "DW1000Device.h"

#define MAX_DEVICES 4  // You can increase this depending on memory

class DeviceManager {
public:
    DeviceManager();

    bool addDevice(DW1000Device* device, bool checkShortAddress = false);
    void removeDevice(int16_t index);

    DW1000Device* getDevice(int16_t index);
    DW1000Device* getDeviceByShortAddress(byte shortAddress[]);

    void checkForInactiveDevices(void (*handleInactive)(DW1000Device*));
    void reactivateDevice(byte shortAddress[]);
    uint8_t getDeviceCount();

private:
    DW1000Device _devices[MAX_DEVICES];
    uint8_t _deviceCount;
};

#endif
