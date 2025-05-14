/*
 * Copyright (c) 2015 by Thomas Trojer <thomas@trojer.net> and Leopold Sayous <leosayous@gmail.com>
 * Decawave DW1000 library for Arduino.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file DW1000Ranging.h
 * Arduino global library (header file) working with the DW1000 library
 * for the Decawave DW1000 UWB transceiver IC.
 */

#ifndef _DW1000_RANGING_H_
#define _DW1000_RANGING_H_

#include "DW1000.h"
#include "DW1000Time.h"
#include "DW1000Device.h"
#include "DW1000Mac.h"
#include "DeviceManager.h"

// Ranging protocol messages
enum : uint8_t {
    POLL = 0,
    POLL_ACK = 1,
    RANGE = 2,
    RANGE_REPORT = 3,
    RANGE_FAILED = 255,
    BLINK = 4,
    RANGING_INIT = 5
};

#define LEN_DATA 35

// Default pins
#define DEFAULT_RST_PIN      9
#define DEFAULT_SPI_SS_PIN  10
#define DEFAULT_RESET_PERIOD 1000    // ms
#define DEFAULT_REPLY_DELAY_TIME 10000 // µs
#define DEFAULT_TIMER_DELAY   60    // ms

// Device roles
enum Role : uint8_t { TAG = 0, ANCHOR = 1 };

#ifndef DEBUG
  #define DEBUG false
#endif

class DW1000RangingClass {
public:
    // Global data buffer
    static byte data[LEN_DATA];

    // Initialization & network configuration
    static void initCommunication(uint8_t rst = DEFAULT_RST_PIN,
                                  uint8_t ss  = DEFAULT_SPI_SS_PIN,
                                  uint8_t irq = 2);
    static void configureNetwork(uint16_t deviceAddress,
                                 uint16_t networkId,
                                 const byte mode[]);
    static void generalStart();
    static void startAsAnchor(const char address[], const byte mode[], bool randomShort = true);
    static void startAsTag   (const char address[], const byte mode[], bool randomShort = true);

    // Ranging control
    static void loop();
    static int16_t detectMessageType(const byte frame[]);

    // Settings
    static void setReplyTime(uint16_t us);
    static void setResetPeriod(uint32_t ms);

    // Address & device lookup
    static const byte*    getCurrentAddress();
    static const byte*    getCurrentShortAddress();
    static DW1000Device*  getDistantDevice(int16_t index);
	static DW1000Device* getDistantDevice(const byte shortAddr[]);
    static DW1000Device*  searchDistantDevice(const byte shortAddr[]);

	static void useRangeFilter(bool enabled);
static void setRangeFilterValue(uint16_t value);


	//Handlers:
	static void attachNewRange(void (* handleNewRange)(DW1000Device*)) { _handleNewRange = handleNewRange; };
	
	static void attachBlinkDevice(void (* handleBlinkDevice)(DW1000Device*)) { _handleBlinkDevice = handleBlinkDevice; };
	
	static void attachNewDevice(void (* handleNewDevice)(DW1000Device*)) { _handleNewDevice = handleNewDevice; };
	
	static void attachInactiveDevice(void (* handleInactiveDevice)(DW1000Device*)) { _handleInactiveDevice = handleInactiveDevice; };
	

    // Debug
    static void visualizeDatas(const byte frame[]);
	// Type detection by short address prefix
static bool isLikelyTag(uint16_t shortAddress);
static bool isLikelyAnchor(uint16_t shortAddress);


private:
    // Manager for storing anchors/tags
    static DeviceManager _deviceManager;

    // Internal state
    static byte    _currentAddress[8];
    static byte    _currentShortAddress[2];
    static byte    _lastSentToShortAddress[2];
    static DW1000Mac _globalMac;

    // Handlers
    static void (*_handleNewRange)(DW1000Device*);
    static void (*_handleBlinkDevice)(DW1000Device*);
    static void (*_handleNewDevice)(DW1000Device*);
    static void (*_handleInactiveDevice)(DW1000Device*);

    // Protocol state
    static Role     _type;
    static volatile bool _sentAck;
    static volatile bool _receivedAck;
    static bool     _protocolFailed;

    // Timing
    static uint8_t  _RST;
    static uint8_t  _SS;
    static uint32_t _lastActivity;
    static uint32_t _resetPeriod;
    static uint16_t _replyDelayTimeUS;
    static uint16_t _timerDelay;
    static int32_t  timer;
    static int16_t  counterForBlink;
    static uint16_t _rangeFilterValue;
    static volatile bool _useRangeFilter;

    // Transmit/receive callbacks
    static void handleSent();
    static void handleReceived();
    static void noteActivity();
    static void resetInactive();

    // Checks & utilities
    static void checkForReset();
    static void checkForInactiveDevices();
    static void copyShortAddress(byte dst[], const byte src[]);

    // Sending frames
    static void transmitInit();
    static void transmit(const byte frame[]);
    static void transmit(const byte frame[], const DW1000Time& time);
    static void transmitBlink();
    static void transmitRangingInit(DW1000Device*);
    static void transmitPoll(DW1000Device*);
    static void transmitPollAck(DW1000Device*);
    static void transmitRange(DW1000Device*);
    static void transmitRangeReport(DW1000Device*);
    static void transmitRangeFailed(DW1000Device*);
    static void receiver();

    // Range computation
    static void computeRangeAsymmetric(DW1000Device*, DW1000Time* tof);
    static void timerTick();
    static float filterValue(float current, float previous, uint16_t elements);
};

// Global instance
extern DW1000RangingClass DW1000Ranging;

#endif // _DW1000_RANGING_H_
