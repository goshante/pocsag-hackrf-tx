#ifndef _WAVSOURCE_H
#define _WAVSOURCE_H

#include <vector>
#include <string>

class WavSource
{
public:
	struct PCMHeader
	{
		uint16_t channels;
		uint16_t bitrate;
		uint16_t byterate;
		uint32_t samplingRate;
		bool isFloat;
	};

private:
	PCMHeader m_pcmHeader;
	std::vector<float> m_buf;

	void _makeBuffer(const std::vector<uint8_t>& buf);

public:
	WavSource(const std::string& fileName);
	WavSource(const std::vector<uint8_t>& buf);
	~WavSource();

	PCMHeader pcmInfo() const { return m_pcmHeader; }
	std::vector<float> getData() const { return m_buf; }
	size_t getSampleCount() { return m_buf.size(); }
};

#endif // !_WAVSOURCE_H

