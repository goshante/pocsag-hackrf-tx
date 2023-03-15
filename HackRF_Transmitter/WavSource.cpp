#include "WavSource.h"
#include <Windows.h>
#include <stdexcept>
#include <sstream>

#define WAVE_FORMAT_PCM		1
#define WAVE_FORMAT_FLOAT	3

static void wavRead32FromMemory(WavSource::PCMHeader header, const std::vector<uint8_t>& in, std::vector<float>& out, size_t start, size_t sampleCount)
{
    unsigned char bufi[8];
    size_t off = start;
    auto readNext = [&in, &bufi, &off](size_t itemSize, size_t count)
    {
        size_t i = 0;
        for (i = 0; i < count * itemSize && i + off < in.size(); i++)
            bufi[i] = in[i + off];
        off += i;

        return i / itemSize;
    };

    if ((header.byterate > 8) || (header.byterate < 1))
        return;

    auto num = sampleCount;
    out.clear();
    
    size_t i = 0;
     for  (i = 0; i < num; i++)
    {
        if (readNext(header.byterate, 1) != 1)
            break;

        int s = 0;
        if (header.isFloat)
        {
            switch (header.byterate)
            {
            case 4:
                out.push_back((*(float*)&bufi) * (float)32768);
                break;

            case 8:
                out.push_back((float)((*(double*)&bufi) * (float)32768));
                break;

            default:
                return;
            }
        }
        else
        {
            // convert to 32 bit float
            // fix endianness
            switch (header.byterate)
            {
            case 1:
                /* this is endian clean */
                out.push_back(((float)bufi[0] - 128) * (float)256);
                break;

            case 2:
                /*if (sndf->bigendian)
                {
                    // swap bytes
                    int16_t s = ((int16_t*)bufi)[0];
                    s = SWAP16(s);
                    buf[i] = (float)s / 65530;
                }
                else
                {*/
                    // no swap
                    s = ((int16_t*)bufi)[0];
                    out.push_back((float)s / 65530);
                //}
                break;

            case 3:
                /*if (!sndf->bigendian)
                {*/
                    s = bufi[0] | (bufi[1] << 8) | (bufi[2] << 16);

                    // fix sign
                    if (s & 0x800000)
                        s |= 0xff000000;

                    out.push_back((float)s / 256);
                /*}
                else // big endian input
                {
                    int s = (bufi[0] << 16) | (bufi[1] << 8) | bufi[2];

                    // fix sign
                    if (s & 0x800000)
                        s |= 0xff000000;

                    buf[i] = (float)s / 256;
                }*/
                break;

            case 4:
                /*if (sndf->bigendian)
                {
                    // swap bytes
                    int s = *(int*)&bufi;
                    buf[i] = (float)SWAP32(s) / 65536;
                }
                else
                {*/
                    s = *(int*)&bufi;
                    out.push_back((float)s / 65536);
                //}
                break;

            default:
                return;
            }
        }
    }
}

WavSource::WavSource(const std::string& fileName)
{
    FILE* file;
    if (!(file = fopen(fileName.c_str(), "rb")))
        throw std::runtime_error("Cannot open wav file");

	std::vector<uint8_t> buf;
    fseek(file, 0L, SEEK_END);
    DWORD sz = ftell(file);
    buf.resize(sz);

    rewind(file);
    if (fread(&buf[0], sz, 1, file) != 1)
        throw std::runtime_error("Cannot read wav file");
	
    fclose(file);
	_makeBuffer(buf);
}

WavSource::WavSource(const std::vector<uint8_t>& buf)
{
	_makeBuffer(buf);
}

void WavSource::_makeBuffer(const std::vector<uint8_t>& buf)
{
	if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F' || buf.size() < 44)
		throw std::runtime_error("This is not a WAVE file or buffer.");

	uint16_t pcm = *reinterpret_cast<const uint16_t*>(&buf[20]);
	
	if (pcm != WAVE_FORMAT_PCM && pcm != WAVE_FORMAT_FLOAT)
		throw std::runtime_error("This is not PCM or float wave.");

	m_pcmHeader.channels = *reinterpret_cast<const uint16_t*>(&buf[22]);
	m_pcmHeader.samplingRate = *reinterpret_cast<const uint32_t*>(&buf[24]);
	//m_pcmHeader.sampleBytePerSec = *reinterpret_cast<const uint32_t*>(&buf[28]);
	m_pcmHeader.bitrate = *reinterpret_cast<const uint16_t*>(&buf[34]);
    m_pcmHeader.byterate = m_pcmHeader.bitrate / 8;
    m_pcmHeader.isFloat = pcm == WAVE_FORMAT_FLOAT;
    size_t sampleSize = m_pcmHeader.byterate;
	size_t sampleCount = (buf.size() - 44) / sampleSize;

    wavRead32FromMemory(m_pcmHeader, buf, m_buf, 44, sampleCount);

	if (m_buf.size() != sampleCount)
		throw std::runtime_error("PCM data is corrupted or incomplete.");
}


WavSource::~WavSource()
{
}

