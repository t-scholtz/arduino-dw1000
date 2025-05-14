#include "DW1000Ranging.h"
#include "DW1000Device.h"

DW1000RangingClass DW1000Ranging;

// Initialize static variables
// Initialize static variables (match header exactly)
DeviceManager DW1000RangingClass::_deviceManager;
byte DW1000RangingClass::_currentAddress[8];
byte DW1000RangingClass::_currentShortAddress[2];
byte DW1000RangingClass::_lastSentToShortAddress[2];
DW1000Mac DW1000RangingClass::_globalMac;
Role DW1000RangingClass::_type;
volatile bool DW1000RangingClass::_sentAck = false;
volatile bool DW1000RangingClass::_receivedAck = false;
bool DW1000RangingClass::_protocolFailed = false;
int32_t DW1000RangingClass::timer = 0;
int16_t DW1000RangingClass::counterForBlink = 0;
byte DW1000RangingClass::data[LEN_DATA];
uint8_t DW1000RangingClass::_RST;
uint8_t DW1000RangingClass::_SS;
uint32_t DW1000RangingClass::_lastActivity;
uint32_t DW1000RangingClass::_resetPeriod;
uint16_t DW1000RangingClass::_replyDelayTimeUS;
uint16_t DW1000RangingClass::_timerDelay;
uint16_t DW1000RangingClass::_rangeFilterValue;
volatile bool DW1000RangingClass::_useRangeFilter = false;

// Here our handlers
void (*DW1000RangingClass::_handleNewRange)(DW1000Device *) = nullptr;
void (*DW1000RangingClass::_handleBlinkDevice)(DW1000Device *) = nullptr;
void (*DW1000RangingClass::_handleNewDevice)(DW1000Device *) = nullptr;
void (*DW1000RangingClass::_handleInactiveDevice)(DW1000Device *) = nullptr;

void DW1000RangingClass::initCommunication(uint8_t myRST, uint8_t mySS, uint8_t myIRQ)
{
	_RST = myRST;
	_SS = mySS;
	_resetPeriod = DEFAULT_RESET_PERIOD;
	_replyDelayTimeUS = DEFAULT_REPLY_DELAY_TIME;
	_timerDelay = DEFAULT_TIMER_DELAY;

	DW1000.begin(myIRQ, myRST);
	DW1000.select(mySS);
}

void DW1000RangingClass::configureNetwork(uint16_t deviceAddress, uint16_t networkId, const byte mode[])
{
	// general configuration
	DW1000.newConfiguration();
	DW1000.setDefaults();
	DW1000.setDeviceAddress(deviceAddress);
	DW1000.setNetworkId(networkId);
	DW1000.enableMode(mode);
	DW1000.commitConfiguration();
}

void DW1000RangingClass::generalStart()
{
	// attach callback for (successfully) sent and received messages
	DW1000.attachSentHandler(handleSent);
	DW1000.attachReceivedHandler(handleReceived);
	// anchor starts in receiving mode, awaiting a ranging poll message

	if (DEBUG)
	{
		// DEBUG monitoring
		Serial.println("DW1000-arduino");
		// initialize the driver

		Serial.println("configuration..");
		// DEBUG chip info and registers pretty printed
		char msg[90];
		DW1000.getPrintableDeviceIdentifier(msg);
		Serial.print("Device ID: ");
		Serial.println(msg);
		DW1000.getPrintableExtendedUniqueIdentifier(msg);
		Serial.print("Unique ID: ");
		Serial.print(msg);
		char string[6];
		sprintf(string, "%02X:%02X", _currentShortAddress[0], _currentShortAddress[1]);
		Serial.print(" short: ");
		Serial.println(string);

		DW1000.getPrintableNetworkIdAndShortAddress(msg);
		Serial.print("Network ID & Device Address: ");
		Serial.println(msg);
		DW1000.getPrintableDeviceMode(msg);
		Serial.print("Device mode: ");
		Serial.println(msg);
	}

	// anchor starts in receiving mode, awaiting a ranging poll message
	receiver();
	// align our looping timer
	timer = millis();
}

void DW1000RangingClass::startAsAnchor(const char address[], const byte mode[], bool randomShort)
{
	// Save the EUI-64 address
	DW1000.convertToByte(const_cast<char *>(address), _currentAddress);
	DW1000.setEUI(const_cast<char *>(address));
	Serial.print("device address: ");
	Serial.println(address);

	if (randomShort)
	{
		randomSeed(analogRead(0));
		uint8_t suffix = random(0, 256); // low byte
		_currentShortAddress[0] = suffix;
		_currentShortAddress[1] = 0x12; // high byte = 0x12 for anchors
	}
	else
	{
		_currentShortAddress[0] = _currentAddress[0];
		_currentShortAddress[1] = _currentAddress[1];
	}

	uint16_t shortAddr = (_currentShortAddress[1] << 8) | _currentShortAddress[0];

	// Configure network (device short address, PAN ID, UWB config)
	DW1000Ranging.configureNetwork(shortAddr, 0xDECA, mode);

	generalStart();
	_type = ANCHOR;

	Serial.println("### ANCHOR ###");
	Serial.print("Short address: 0x");
	Serial.println(shortAddr, HEX);
}

void DW1000RangingClass::startAsTag(const char address[], const byte mode[], bool randomShort)
{
	// Save the EUI-64 address
	DW1000.convertToByte(const_cast<char *>(address), _currentAddress);
	DW1000.setEUI(const_cast<char *>(address));
	Serial.print("device address: ");
	Serial.println(address);

	if (randomShort)
	{
		randomSeed(analogRead(0));
		uint8_t suffix = random(0, 256); // low byte
		_currentShortAddress[0] = suffix;
		_currentShortAddress[1] = 0x98; // high byte = 0x98 for tags
	}
	else
	{
		_currentShortAddress[0] = _currentAddress[0];
		_currentShortAddress[1] = _currentAddress[1];
	}

	uint16_t shortAddr = (_currentShortAddress[1] << 8) | _currentShortAddress[0];

	// Configure network (device short address, PAN ID, UWB config)
	DW1000Ranging.configureNetwork(shortAddr, 0xDECA, mode);

	generalStart();
	_type = TAG;

	Serial.println("### TAG ###");
	Serial.print("Short address: 0x");
	Serial.println(shortAddr, HEX);
}

/* ###########################################################################
 * #### Setters and Getters ##################################################
 * ######################################################################### */

// setters
void DW1000RangingClass::setReplyTime(uint16_t replyDelayTimeUs) { _replyDelayTimeUS = replyDelayTimeUs; }

void DW1000RangingClass::setResetPeriod(uint32_t resetPeriod) { _resetPeriod = resetPeriod; }

DW1000Device *DW1000RangingClass::searchDistantDevice(const byte shortAddr[])
{
	return _deviceManager.getDeviceByShortAddress(const_cast<byte *>(shortAddr));
}

DW1000Device *DW1000RangingClass::getDistantDevice(int16_t index)
{
	return _deviceManager.getDevice(index);
}

DW1000Device *DW1000RangingClass::getDistantDevice(const byte shortAddr[])
{
	return _deviceManager.getDeviceByShortAddress(const_cast<byte *>(shortAddr));
}

/* ###########################################################################
 * #### Public methods #######################################################
 * ######################################################################### */

void DW1000RangingClass::checkForReset()
{
	if (!_sentAck && !_receivedAck)
	{
		if (millis() - _lastActivity > _resetPeriod)
		{
			resetInactive();
		}
	}
}

void DW1000RangingClass::checkForInactiveDevices()
{
	_deviceManager.checkForInactiveDevices(_handleInactiveDevice);
}

// TODO check return type
int16_t DW1000RangingClass::detectMessageType(const byte frame[])
{
	if (frame[0] == FC_1_BLINK)
	{
		return BLINK;
	}
	else if (frame[0] == FC_1 && frame[1] == FC_2)
	{
		return frame[LONG_MAC_LEN];
	}
	else if (frame[0] == FC_1 && frame[1] == FC_2_SHORT)
	{
		return frame[SHORT_MAC_LEN];
	}
	return -1;
}

void DW1000RangingClass::loop()
{
	checkForReset();

	if (millis() - timer > _timerDelay)
	{
		timer = millis();
		if (DEBUG)
			Serial.println("[TICK] timerTick()");
		timerTick();
	}
	if (_sentAck)
	{
		_sentAck = false;
		int txType = detectMessageType(data);
		if (DEBUG)
		{
			Serial.print("[ACK SENT] Type: ");
			Serial.println(txType);
		}
		if (auto dev = searchDistantDevice(_lastSentToShortAddress))
		{
			switch (txType)
			{
			case POLL:
				DW1000.getTransmitTimestamp(dev->timePollSent);
				break;
			case RANGE:
				DW1000.getTransmitTimestamp(dev->timeRangeSent);
				break;
			case POLL_ACK:
				if (_type == ANCHOR)
					DW1000.getTransmitTimestamp(dev->timePollAckSent);
				break;
			}
		}
		noteActivity();
	}

	if (!_receivedAck)
		return;
	_receivedAck = false;

	DW1000.getData(data, LEN_DATA);
	if (DEBUG)
	{
		Serial.print("[DEBUG] RX raw (");
		Serial.print(LEN_DATA);
		Serial.println(" bytes):");
		for (int i = 0; i < LEN_DATA; ++i)
		{
			if (data[i] < 0x10)
				Serial.print('0');
			Serial.print(data[i], HEX);
			Serial.print(' ');
		}
		Serial.println();
	}
	int msgType = detectMessageType(data);
	if (msgType < 0)
	{
		if (DEBUG)
			Serial.println("[ERROR] bad frame control");
		return;
	}
	if (DEBUG)
	{
		Serial.print("[RECEIVED] Msg type: ");
		Serial.println(msgType);
	}

	byte recvAddr[2];
	_globalMac.getReceiverAddress(data, recvAddr);
	uint16_t rxShort = (recvAddr[1] << 8) | recvAddr[0];
	uint16_t myShort = (_currentShortAddress[1] << 8) | _currentShortAddress[0];
	bool isBroadcast = (rxShort == 0xFFFF) || (data[0] == FC_1_BLINK);

	if (DEBUG && !isBroadcast)
	{
		Serial.print("THIS MESSAGE IS FOR: 0x");
		Serial.println(rxShort, HEX);
	}
	// only for non-POLL, non-broadcast, explicitly-targeted frames…
	if (_type == ANCHOR)
	{
		if (!isBroadcast && rxShort != myShort)
		{
			// auto-add unknown tags on the fly
			// if (((rxShort & 0x00FF) == 0x0098 || (rxShort & 0xff00) == 0x9800))
			// {
			// 	Serial.println("[ANCHOR] Trying to auto-add unknown tag");
			// 	// pass nullptr for the EUI, then the two-byte short address:
			// 	 byte KNOWN_TAG_EUI[8] = {
			// 		0x98, 0x33, 0x33, 0x33,
			// 		0x33, 0x33, 0x33, 0x33};
			// 	DW1000Device *newTag = new DW1000Device(KNOWN_TAG_EUI, recvAddr);
			// 	// true==shortAddressOnly
			// 	if (_deviceManager.addDevice(newTag, /*shortAddressOnly=*/true))
			// 	{
			// 		Serial.print("[ANCHOR] Auto-added tag: 0x");
			// 		Serial.println(rxShort, HEX);
			// 	}
			// 	else
			// 	{
			// 		delete newTag;
			// 		Serial.println("[ANCHOR] Auto-add failed");
			// 	}
			// }

if(DEBUG){			Serial.print("NOT FOR US: 0x");
			Serial.println(rxShort, HEX);}
			return;
		}
	}

	if (msgType == BLINK && _type == ANCHOR)
	{
		byte addr[8], shortAddr[2];
		_globalMac.decodeBlinkFrame(data, addr, shortAddr);

		// Check if device already exists before creating a new one
		DW1000Device *existingDevice = getDistantDevice(shortAddr);
		if (!existingDevice)
		{
			DW1000Device *newTag = new DW1000Device(addr, shortAddr);
			if (_deviceManager.addDevice(newTag))
			{
				if (DEBUG)
				{
					Serial.print("[ANCHOR] New tag added: short:");
					Serial.println((shortAddr[0] << 8) | shortAddr[1], HEX);
				}
				if (_handleBlinkDevice)
					_handleBlinkDevice(newTag);
				transmitRangingInit(newTag);
				noteActivity();
			}
			else
			{
				delete newTag;
				if (DEBUG)
					Serial.println("[ERROR] Failed to add tag device");
			}
		}
		else if (DEBUG)
		{
			Serial.print("[ANCHOR] Tag already exists: ");
			Serial.println((shortAddr[0] << 8) | shortAddr[1], HEX);
		}
		return;
	}

	if (msgType == RANGING_INIT && _type == TAG)
	{
		byte addr[2];
		_globalMac.decodeLongMACFrame(data, addr);

		// Check if this anchor already exists
		DW1000Device *existingAnchor = getDistantDevice(addr);
		if (!existingAnchor)
		{
			DW1000Device *newAnchor = new DW1000Device(addr, true);
			if (_deviceManager.addDevice(newAnchor, true))
			{
				if (DEBUG)
				{
					Serial.print("[TAG] New anchor added: short:");
					Serial.println((addr[0] << 8) | addr[1], HEX);
				}

				DW1000Device *storedDev = _deviceManager.getDeviceByShortAddress(addr);
				if (storedDev)
				{
					storedDev->setTagState(TAG_STATE_IDLE);
				}

				if (_handleNewDevice)
					_handleNewDevice(storedDev);

				if (DEBUG)
				{
					Serial.print("[TAG] DeviceManager now has ");
					Serial.print(_deviceManager.getDeviceCount());
					Serial.println(" devices.");
				}
			}
			else
			{
				delete newAnchor;
				if (DEBUG)
					Serial.println("[ERROR] Failed to add anchor device");
			}
		}
		else if (DEBUG)
		{
			Serial.print("[TAG] Anchor already exists: ");
			Serial.println((addr[0] << 8) | addr[1], HEX);
			// Make sure the anchor is in idle state so we'll range with it
			existingAnchor->setTagState(TAG_STATE_IDLE);
		}
		noteActivity();
		return;
	}

	// Validate message format before decoding
	if (msgType != BLINK && (data[0] != FC_1 || (data[1] != FC_2 && data[1] != FC_2_SHORT)))
	{
		if (DEBUG)
			Serial.println("[ERROR] Invalid frame format");
		return;
	}

	byte addr[2];
	_globalMac.decodeShortMACFrame(data, addr);

	DW1000Device *dev = getDistantDevice(addr);
	if (!dev)
	{
		// In case device isn't found, create a temporary one to handle the message
		// but with a safeguard to prevent memory leaks
		if (DEBUG)
		{
			Serial.print("[WARNING] Message from unknown device: ");
			Serial.println((addr[0] << 8) | addr[1], HEX);
		}

		// Only create a device if it's a critical message type
		if (msgType == POLL || msgType == POLL_ACK || msgType == RANGE)
		{
			DW1000Device *newDevice = new DW1000Device(addr, _type == TAG);
			bool added = _deviceManager.addDevice(newDevice);
			if (!added)
			{
				delete newDevice;
				if (DEBUG)
					Serial.println("[ERROR] Failed to add device to manager");
				return;
			}
			dev = _deviceManager.getDeviceByShortAddress(addr);
			if (DEBUG && dev)
			{
				Serial.print("[DEBUG] Created device on the fly. Success? ");
				Serial.println(added);
			}
		}
		else
		{
			// Skip non-critical messages from unknown devices
			return;
		}
	}

	// Double-check device is valid before proceeding
	if (!dev)
	{
		if (DEBUG)
			Serial.println("[ERROR] Failed to get or create device");
		return;
	}

	// Update device activity timestamp
	dev->noteActivity();

	if (_type == TAG)
	{
		if (msgType == dev->getExpectedMsgId())
		{
			if (msgType == POLL_ACK)
			{
				DW1000.getReceiveTimestamp(dev->timePollAckReceived);
				if (DEBUG)
				{
					Serial.print("[TAG] Received POLL_ACK from ");
					Serial.println((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
					Serial.print("[DEBUG] timePollAckReceived: ");
					dev->timePollAckReceived.printTo(Serial);
				}

				dev->setExpectedMsgId(RANGE_REPORT);
				transmitRange(dev);
			}
			else if (msgType == RANGE_REPORT)
			{
				float range, power;
				// Check if we have enough data for these fields
				if (SHORT_MAC_LEN + 9 <= LEN_DATA)
				{
					memcpy(&range, data + 1 + SHORT_MAC_LEN, 4);
					memcpy(&power, data + 5 + SHORT_MAC_LEN, 4);
					dev->setRange(range);
					dev->setRXPower(power);
					dev->setTagState(TAG_STATE_IDLE);
					if (DEBUG)
					{
						Serial.print("[TAG] RANGE_REPORT from ");
						Serial.print((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
						Serial.print(": Range=");
						Serial.print(range);
						Serial.print("m RXPower=");
						Serial.println(power);
					}
					if (_handleNewRange)
						_handleNewRange(dev);
				}
				else
				{
					if (DEBUG)
						Serial.println("[ERROR] RANGE_REPORT message too short");
				}
			}
			else if (msgType == RANGE_FAILED)
			{
				dev->setExpectedMsgId(POLL_ACK);
				dev->setTagState(TAG_STATE_IDLE);
				if (DEBUG)
				{
					Serial.print("[TAG] RANGE_FAILED received from ");
					Serial.print((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
					Serial.println(", resetting state");
				}
			}
		}
		else if (DEBUG)
		{
			Serial.print("[TAG] Unexpected message type ");
			Serial.print(msgType);
			Serial.print(" from ");
			Serial.print((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
			Serial.print(", expected ");
			Serial.println(dev->getExpectedMsgId());
		}
	}
	else if (_type == ANCHOR)
	{
		if (msgType == POLL)
		{
			DW1000.getReceiveTimestamp(dev->timePollReceived);
			dev->setExpectedMsgId(RANGE);
			transmitPollAck(dev);
			if (DEBUG)
			{
				Serial.print("[ANCHOR] POLL received from ");
				Serial.print((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
				Serial.println(" and replied");
			}
			noteActivity();
		}
		else if (msgType == RANGE)
		{
			// Validate message has enough bytes for all timestamps
			if (SHORT_MAC_LEN + 16 > LEN_DATA)
			{
				if (DEBUG)
					Serial.println("[ERROR] RANGE message too short");
				return;
			}

			DW1000.getReceiveTimestamp(dev->timeRangeReceived);
			dev->setExpectedMsgId(POLL);

			dev->timePollSent.setTimestamp(data + 1 + SHORT_MAC_LEN);
			dev->timePollAckReceived.setTimestamp(data + 6 + SHORT_MAC_LEN);
			dev->timeRangeSent.setTimestamp(data + 11 + SHORT_MAC_LEN);

			DW1000Time tof;
			computeRangeAsymmetric(dev, &tof);
			float distance = tof.getAsMeters();

			// Add extra validation for reasonable distance values
			if (distance < 0 || distance > 300)
			{
				if (DEBUG)
				{
					Serial.print("[ANCHOR] Invalid distance calculated: ");
					Serial.print(distance);
					Serial.println("m - ignoring");
				}
				transmitRangeFailed(dev);
				return;
			}

			if (_useRangeFilter && dev->getRange() > 0.0f)
			{
				distance = filterValue(distance, dev->getRange(), _rangeFilterValue);
			}

			dev->setRange(distance);
			dev->setRXPower(DW1000.getReceivePower());
			dev->setFPPower(DW1000.getFirstPathPower());
			dev->setQuality(DW1000.getReceiveQuality());

			transmitRangeReport(dev);

			if (_handleNewRange)
				_handleNewRange(dev);

			if (DEBUG)
			{
				Serial.print("[ANCHOR] Computed range for ");
				Serial.print((dev->getByteShortAddress()[0] << 8) | dev->getByteShortAddress()[1], HEX);
				Serial.print(": ");
				Serial.print(distance);
				Serial.println(" m");
			}
		}
	}
}

/* ###########################################################################
 * #### Private methods and Handlers for transmit & Receive reply ############
 * ######################################################################### */

void DW1000RangingClass::handleSent()
{
	// status change on sent success
	_sentAck = true;
}

void DW1000RangingClass::handleReceived()
{
	// status change on received success
	_receivedAck = true;
}

void DW1000RangingClass::noteActivity()
{
	// update activity timestamp, so that we do not reach "resetPeriod"
	_lastActivity = millis();
}

void DW1000RangingClass::resetInactive()
{
	if (_type == ANCHOR)
	{
		receiver();
	}
	noteActivity();
}

void DW1000RangingClass::timerTick()
{
	if (_type == TAG)
	{
		uint8_t devCount = _deviceManager.getDeviceCount();

		if (DEBUG)
		{
			for (uint8_t i = 0; i < _deviceManager.getDeviceCount(); i++)
			{
				DW1000Device *d = _deviceManager.getDevice(i);
				if (d)
				{
					Serial.print("[DEBUG] Dev ");
					Serial.print(i);
					Serial.print(" short=");
					Serial.print(d->getShortAddress(), HEX);
					Serial.print(" state=");
					Serial.println(d->getTagState() == TAG_STATE_IDLE ? "IDLE" : "BUSY");
				}
			}
		}

		if (devCount > 1)
		{
			if (DEBUG)
			{
				Serial.print("[TIMER] Device count: ");
				Serial.println(devCount);
			}

			// Log all known device short addresses
			if (DEBUG)
			{
				for (uint8_t i = 0; i < devCount; i++)
				{
					DW1000Device *d = _deviceManager.getDevice(i);
					if (d)
					{
						Serial.print("  [TIMER] Device[");
						Serial.print(i);
						Serial.print("] short: ");
						Serial.print(d->getShortAddress(), HEX);
						Serial.print(" state: ");
						Serial.println(d->getTagState() == TAG_STATE_IDLE ? "IDLE" : "RANGING");
					}
				}
			}

			// Use a counter to cycle through devices for more fair scheduling
			static uint8_t deviceIndex = 0;
			bool rangedThisTick = false;

			// Try up to the number of devices to find one to range with
			for (uint8_t attempt = 0; attempt < devCount; attempt++)
			{
				deviceIndex = (deviceIndex + 1) % devCount;
				DW1000Device *dev = _deviceManager.getDevice(deviceIndex);

				if (DEBUG)
				{
					Serial.print("[TIMER] Checking device index ");
					Serial.print(deviceIndex);
					Serial.print(": ");

					if (dev)
					{
						Serial.print("short=");
						Serial.print(dev->getShortAddress(), HEX);
						Serial.print(", state=");
						Serial.println(dev->getTagState() == TAG_STATE_IDLE ? "IDLE" : "BUSY");
					}
					else
					{
						Serial.println("nullptr");
					}
				}

				// Skip null devices or non-idle ones
				if (dev == nullptr || dev->getTagState() != TAG_STATE_IDLE)
				{
					continue;
				}

				dev->setTagState(TAG_STATE_RANGING);
				dev->setExpectedMsgId(POLL_ACK);
				memcpy(_lastSentToShortAddress, dev->getByteShortAddress(), 2);

				if (DEBUG)
				{
					Serial.print("[TIMER] Sending POLL to device: ");
					Serial.println(dev->getShortAddress(), HEX);
				}

				transmitPoll(dev);
				rangedThisTick = true;
				break;
			}

			if (!rangedThisTick && DEBUG)
			{
				Serial.println("[TIMER] No idle devices found for ranging");
			}
		}
		else
		{
			if (DEBUG)
				Serial.println("[TIMER] No devices, sending BLINK");
			transmitBlink();
		}
	}

	checkForInactiveDevices();
	counterForBlink++;
	if (counterForBlink > 20)
		counterForBlink = 0;
}

void DW1000RangingClass::copyShortAddress(byte dst[], const byte src[])
{
	dst[0] = src[0];
	dst[1] = src[1];
}

/* ###########################################################################
 * #### Methods for ranging protocole   ######################################
 * ######################################################################### */

void DW1000RangingClass::transmitInit()
{
	DW1000.newTransmit();
	DW1000.setDefaults();
}

void DW1000RangingClass::transmit(const byte frame[])
{
	DW1000.setData(const_cast<byte *>(frame), LEN_DATA);
	DW1000.startTransmit();
}

void DW1000RangingClass::transmit(const byte frame[], const DW1000Time &time)
{
	DW1000.setDelay(time);
	DW1000.setData(const_cast<byte *>(frame), LEN_DATA);
	DW1000.startTransmit();
}

void DW1000RangingClass::transmitBlink()
{
	transmitInit();
	_globalMac.generateBlinkFrame(data, _currentAddress, _currentShortAddress);
	transmit(data);
}

void DW1000RangingClass::transmitRangingInit(DW1000Device *myDistantDevice)
{
	transmitInit();
	// we generate the mac frame for a ranging init message
	_globalMac.generateLongMACFrame(data, _currentShortAddress, myDistantDevice->getByteAddress());
	// we define the function code
	data[LONG_MAC_LEN] = RANGING_INIT;

	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	delayMicroseconds(random(0, DEFAULT_TIMER_DELAY * 10));
	transmit(data);
	noteActivity();
}

void DW1000RangingClass::transmitPoll(DW1000Device *myDistantDevice)
{
	transmitInit();
	if (myDistantDevice == nullptr)
	{
		_timerDelay = DEFAULT_TIMER_DELAY + (uint16_t)(_deviceManager.getDeviceCount() * 3 * DEFAULT_REPLY_DELAY_TIME / 1000);

		byte shortBroadcast[2] = {0xFF, 0xFF};
		_globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
		data[SHORT_MAC_LEN] = POLL;
		data[SHORT_MAC_LEN + 1] = _deviceManager.getDeviceCount();

		for (uint8_t i = 0; i < _deviceManager.getDeviceCount(); i++)
		{
			DW1000Device *dev = _deviceManager.getDevice(i);
			dev->setReplyTime((2 * i + 1) * DEFAULT_REPLY_DELAY_TIME);
			memcpy(data + SHORT_MAC_LEN + 2 + 4 * i, dev->getByteShortAddress(), 2);
			uint16_t replyTime = dev->getReplyTime();
			memcpy(data + SHORT_MAC_LEN + 2 + 2 + 4 * i, &replyTime, 2);
		}
		copyShortAddress(_lastSentToShortAddress, shortBroadcast);
		if (DEBUG)
			Serial.println("[TX] Broadcasting POLL");
	}
	else
	{
		_timerDelay = DEFAULT_TIMER_DELAY;
		_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
		data[SHORT_MAC_LEN] = POLL;
		data[SHORT_MAC_LEN + 1] = 1;
		uint16_t replyTime = myDistantDevice->getReplyTime();
		memcpy(data + SHORT_MAC_LEN + 2, &replyTime, sizeof(uint16_t));
		copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
		if (DEBUG)
			Serial.print("[TX] POLL to specific device: "), Serial.println((myDistantDevice->getByteShortAddress()[0] << 8) | myDistantDevice->getByteShortAddress()[1], HEX);
	}
	transmit(data);
	noteActivity();
}

void DW1000RangingClass::transmitPollAck(DW1000Device *myDistantDevice)
{
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = POLL_ACK;

	// Plan the future TX timestamp
	DW1000Time deltaTime = DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS);
	DW1000.setDelay(deltaTime);

	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());

	DW1000.setData(data, LEN_DATA);
	DW1000.startTransmit();

	DW1000.getTransmitTimestamp(myDistantDevice->timePollAckSent);
}

void DW1000RangingClass::transmitRange(DW1000Device *myDistantDevice)
{
	transmitInit();

	if (myDistantDevice == nullptr)
	{
		_timerDelay = DEFAULT_TIMER_DELAY + (uint16_t)(_deviceManager.getDeviceCount() * 3 * DEFAULT_REPLY_DELAY_TIME / 1000);

		byte shortBroadcast[2] = {0xFF, 0xFF};
		_globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
		data[SHORT_MAC_LEN] = RANGE;
		data[SHORT_MAC_LEN + 1] = _deviceManager.getDeviceCount();

		DW1000Time deltaTime = DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW1000Time::MICROSECONDS);
		DW1000Time futureTime = DW1000.setDelay(deltaTime);

		// Count valid devices first
		uint8_t validDevCount = 0;
		for (uint8_t i = 0; i < _deviceManager.getDeviceCount(); i++)
		{
			if (_deviceManager.getDevice(i) != nullptr)
			{
				validDevCount++;
			}
		}

		// Update device count in the message
		data[SHORT_MAC_LEN + 1] = validDevCount;

		uint8_t devIndex = 0;
		for (uint8_t i = 0; i < _deviceManager.getDeviceCount(); i++)
		{
			DW1000Device *dev = _deviceManager.getDevice(i);
			if (!dev)
				continue; // Skip null devices

			// Make sure we don't write beyond data buffer
			if (SHORT_MAC_LEN + 2 + 17 * devIndex >= LEN_DATA - 17)
			{
				if (DEBUG)
					Serial.println("[ERROR] Too many devices for data buffer");
				break;
			}

			memcpy(data + SHORT_MAC_LEN + 2 + 17 * devIndex, dev->getByteShortAddress(), 2);
			dev->timeRangeSent = futureTime;
			dev->timePollSent.getTimestamp(data + SHORT_MAC_LEN + 4 + 17 * devIndex);
			dev->timePollAckReceived.getTimestamp(data + SHORT_MAC_LEN + 9 + 17 * devIndex);
			dev->timeRangeSent.getTimestamp(data + SHORT_MAC_LEN + 14 + 17 * devIndex);
			devIndex++;
		}

		copyShortAddress(_lastSentToShortAddress, shortBroadcast);
		transmit(data);
		noteActivity();
		if (DEBUG)
			Serial.println("[TX] Broadcast RANGE sent");
	}
	else
	{
		_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
		data[SHORT_MAC_LEN] = RANGE;

		DW1000Time deltaTime = DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS);
		DW1000.setDelay(deltaTime);
		DW1000Time futureTime = DW1000.setDelay(deltaTime);

		myDistantDevice->timeRangeSent = futureTime;

		myDistantDevice->timePollSent.getTimestamp(data + 1 + SHORT_MAC_LEN);
		myDistantDevice->timePollAckReceived.getTimestamp(data + 6 + SHORT_MAC_LEN);
		myDistantDevice->timeRangeSent.getTimestamp(data + 11 + SHORT_MAC_LEN);

		copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());

		DW1000.setData(data, LEN_DATA);
		DW1000.startTransmit();
	}
}

void DW1000RangingClass::transmitRangeReport(DW1000Device *myDistantDevice)
{
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = RANGE_REPORT;
	// write final ranging result
	float curRange = myDistantDevice->getRange();
	float curRXPower = myDistantDevice->getRXPower();
	// We add the Range and then the RXPower
	memcpy(data + 1 + SHORT_MAC_LEN, &curRange, 4);
	memcpy(data + 5 + SHORT_MAC_LEN, &curRXPower, 4);
	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	transmit(data, DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS));
}

void DW1000RangingClass::transmitRangeFailed(DW1000Device *myDistantDevice)
{
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = RANGE_FAILED;

	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	transmit(data);
	noteActivity();
}

void DW1000RangingClass::receiver()
{
	DW1000.newReceive();
	DW1000.setDefaults();
	// so we don't need to restart the receiver manually
	DW1000.receivePermanently(true);
	DW1000.startReceive();
}

/* ###########################################################################
 * #### Methods for range computation and corrections  #######################
 * ######################################################################### */

void DW1000RangingClass::computeRangeAsymmetric(DW1000Device *myDistantDevice, DW1000Time *myTOF)
{
	if (!myDistantDevice)
	{
		if (DEBUG)
			Serial.println("[ERROR] computeRangeAsymmetric: null device");
		return;
	}

	DW1000Time round1 = (myDistantDevice->timePollAckReceived - myDistantDevice->timePollSent).wrap();
	DW1000Time reply1 = (myDistantDevice->timePollAckSent - myDistantDevice->timePollReceived).wrap();
	DW1000Time round2 = (myDistantDevice->timeRangeReceived - myDistantDevice->timePollAckSent).wrap();
	DW1000Time reply2 = (myDistantDevice->timeRangeSent - myDistantDevice->timePollAckReceived).wrap();

	if (DEBUG)
	{
		Serial.println("[DEBUG] TOF calculation timestamps:");
		Serial.print("  PollSent: ");
		myDistantDevice->timePollSent.printTo(Serial);
		Serial.print("  PollAckReceived: ");
		myDistantDevice->timePollAckReceived.printTo(Serial);
		Serial.print("  PollAckSent: ");
		myDistantDevice->timePollAckSent.printTo(Serial);
		Serial.print("  RangeReceived: ");
		myDistantDevice->timeRangeReceived.printTo(Serial);
		Serial.print("  RangeSent: ");
		myDistantDevice->timeRangeSent.printTo(Serial);
		Serial.print("  round1: ");
		Serial.println((long)round1.getTimestamp());
		Serial.print("  reply1: ");
		Serial.println((long)reply1.getTimestamp());
		Serial.print("  round2: ");
		Serial.println((long)round2.getTimestamp());
		Serial.print("  reply2: ");
		Serial.println((long)reply2.getTimestamp());
	}

	DW1000Time computedTOF = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2);
	myTOF->setTimestamp(computedTOF);

	if (DEBUG)
	{
		float distance = computedTOF.getAsMeters();
		Serial.print("[DEBUG] Raw computed distance: ");
		Serial.print(distance);
		Serial.println(" m");
	}
}

/* FOR DEBUGGING*/
void DW1000RangingClass::visualizeDatas(const byte frame[])
{
	char buf[60];
	int len = 2 + 2 + 16; // adjust as needed
	for (int i = 0; i < 16; i++)
	{
		sprintf(buf + i * 3, "%02X:", frame[i]);
	}
	buf[16 * 3 - 1] = '\0';
	Serial.println(buf);
}

void DW1000RangingClass::useRangeFilter(bool enabled)
{
	_useRangeFilter = enabled;
}

void DW1000RangingClass::setRangeFilterValue(uint16_t value)
{
	_rangeFilterValue = (value < 2) ? 2 : value;
}

/* ###########################################################################
 * #### Utils  ###############################################################
 * ######################################################################### */

float DW1000RangingClass::filterValue(float value, float previousValue, uint16_t numberOfElements)
{

	float k = 2.0f / ((float)numberOfElements + 1.0f);
	return (value * k) + previousValue * (1.0f - k);
}

bool DW1000RangingClass::isLikelyTag(uint16_t shortAddr)
{
	return (shortAddr & 0xFF00) == 0x9800;
}

bool DW1000RangingClass::isLikelyAnchor(uint16_t shortAddr)
{
	return (shortAddr & 0xFF00) == 0x1200;
}
