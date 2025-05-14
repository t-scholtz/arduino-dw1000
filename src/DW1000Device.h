#ifndef _DW1000Device_H_INCLUDED
#define _DW1000Device_H_INCLUDED

#include "DW1000Time.h"

// Inactivity timeout in ms
#define INACTIVITY_TIME 2000

enum TagState
{
	TAG_STATE_IDLE,
	TAG_STATE_RANGING
};

class DW1000Device
{
public:
	DW1000Device();
	DW1000Device(byte address[], boolean shortOne = false);
	DW1000Device(byte address[], byte shortAddress[]);
	~DW1000Device();

	// Setters
	void setReplyTime(uint16_t replyDelayTimeUs);
	void setAddress(char address[]);
	void setAddress(byte *address);
	void setShortAddress(byte address[]);
	void setRange(float range);
	void setRXPower(float power);
	void setFPPower(float power);
	void setQuality(float quality);
	void setReplyDelayTime(uint16_t time);
	void setIndex(int8_t index);
	void setExpectedMsgId(uint8_t msgId);
	void setTagState(TagState state);
	void noteActivity();

	// Getters
	uint16_t getReplyTime();
	byte *getByteAddress();
	byte *getByteShortAddress();
	uint16_t getShortAddress();
	int8_t getIndex();
	float getRange();
	float getRXPower();
	float getFPPower();
	float getQuality();
	bool isAddressEqual(DW1000Device *device);
	bool isShortAddressEqual(DW1000Device *device);
	bool isInactive();
	uint8_t getExpectedMsgId() const;
	TagState getTagState() const;

	// Timestamps
	DW1000Time timePollSent;
	DW1000Time timePollReceived;
	DW1000Time timePollAckSent;
	DW1000Time timePollAckReceived;
	DW1000Time timeRangeSent;
	DW1000Time timeRangeReceived;

	unsigned long getLastActivity() const;

	void setActive();
	void setInactive();
	bool isActive() const;

private:
	void randomShortAddress();
	bool _active;

	byte _ownAddress[8];
	byte _shortAddress[2];
	int32_t _activity;
	uint16_t _replyDelayTimeUS;
	int8_t _index;

	int16_t _range;
	int16_t _RXPower;
	int16_t _FPPower;
	int16_t _quality;

	uint8_t _expectedMsgId;
	TagState _tagState;
	uint32_t _lastStateChange;
};

#endif
