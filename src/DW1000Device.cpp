#include "DW1000Device.h"
#include "DW1000.h"

DW1000Device::DW1000Device() {
    randomShortAddress();
}

DW1000Device::DW1000Device(byte deviceAddress[], boolean shortOne)
{
    noteActivity();
    _expectedMsgId = 0;  // or 0, depending on your protocol
    if (!shortOne) {
        setAddress(deviceAddress);
        randomShortAddress();
    } else {
        setShortAddress(deviceAddress);
    }
}

DW1000Device::DW1000Device(byte deviceAddress[], byte shortAddress[]) {
    setAddress(deviceAddress);
    setShortAddress(shortAddress);
	noteActivity();
	_expectedMsgId = 0;  // or 0, depending on your protocol
}

DW1000Device::~DW1000Device() {noteActivity();}

void DW1000Device::setAddress(char deviceAddress[]) {
    DW1000.convertToByte(deviceAddress, _ownAddress);
}

void DW1000Device::setAddress(byte* deviceAddress) {
    memcpy(_ownAddress, deviceAddress, 8);
}

void DW1000Device::setShortAddress(byte deviceAddress[]) {
    memcpy(_shortAddress, deviceAddress, 2);
}

byte* DW1000Device::getByteAddress() {
    return _ownAddress;
}

byte* DW1000Device::getByteShortAddress() {
    return _shortAddress;
}

uint16_t DW1000Device::getShortAddress() {
    return (_shortAddress[1] << 8) | _shortAddress[0];
}

bool DW1000Device::isAddressEqual(DW1000Device* device) {
    return memcmp(getByteAddress(), device->getByteAddress(), 8) == 0;
}

bool DW1000Device::isShortAddressEqual(DW1000Device* device) {
    return getShortAddress() == device->getShortAddress();
}


void DW1000Device::setRange(float range) {
    _range = round(range * 100);
}

void DW1000Device::setRXPower(float RXPower) {
    _RXPower = round(RXPower * 100);
}

void DW1000Device::setFPPower(float FPPower) {
    _FPPower = round(FPPower * 100);
}

void DW1000Device::setQuality(float quality) {
    _quality = round(quality * 100);
}

float DW1000Device::getRange() {
    return float(_range) / 100.0f;
}

float DW1000Device::getRXPower() {
    return float(_RXPower) / 100.0f;
}

float DW1000Device::getFPPower() {
    return float(_FPPower) / 100.0f;
}

float DW1000Device::getQuality() {
    return float(_quality) / 100.0f;
}

void DW1000Device::setReplyTime(uint16_t replyDelayTimeUs) {
    _replyDelayTimeUS = replyDelayTimeUs;
}

uint16_t DW1000Device::getReplyTime() {
    return _replyDelayTimeUS;
}

void DW1000Device::setReplyDelayTime(uint16_t time) {
    _replyDelayTimeUS = time;
}

void DW1000Device::setIndex(int8_t index) {
    _index = index;
}

int8_t DW1000Device::getIndex() {
    return _index;
}

void DW1000Device::randomShortAddress() {
    _shortAddress[0] = random(0, 256);
    _shortAddress[1] = random(0, 256);
}

void DW1000Device::noteActivity() {
    _activity = millis();
    _active = true;  // Wake up on any activity
}

bool DW1000Device::isInactive() {
    return (millis() - _activity > INACTIVITY_TIME);
}

void DW1000Device::setExpectedMsgId(uint8_t msgId) {
    _expectedMsgId = msgId;
}

uint8_t DW1000Device::getExpectedMsgId() const {
    return _expectedMsgId;
}

void DW1000Device::setTagState(TagState state) {
    _tagState = state;
    _lastStateChange = millis();
}

TagState DW1000Device::getTagState() const {
    return _tagState;
}

void DW1000Device::setActive() {
    _active = true;
    noteActivity(); // also refresh timestamp
}

void DW1000Device::setInactive() {
    _active = false;
}

bool DW1000Device::isActive() const {
    return _active;
}

unsigned long DW1000Device::getLastActivity() const {
    return _activity;
}

