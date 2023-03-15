#ifndef _IHACKRFDATA_H
#define _IHACKRFDATA_H

#include <stdint.h>

class IHackRFData
{
public:
	IHackRFData() {};
	~IHackRFData() {};

protected:
	virtual int onData(int8_t* buffer, uint32_t length) = 0;

	friend class HackRFDevice;
};

#endif // !_IHACKRFDATA_H
