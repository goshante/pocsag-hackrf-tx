#include "FMModulator.h"

constexpr const uint32_t BUF_NUM			= 256;
constexpr const uint32_t BYTES_PER_SAMPLE	= 2;
constexpr const double M_PI					= 3.14159265358979323846;
constexpr const uint32_t BUF_LEN			= 262144;         //hackrf tx buf


FMModulator::FMModulator(float localGain)
	: m_localGain(localGain / (float)100.0)
	, m_ready(false)
	, m_FM_phase(0)
{
	memset(m_last_in_samples, 0, sizeof(m_last_in_samples));
	
	m_leftToSend = m_tail = m_head = 0;

	m_workerBuf.resize(BUF_NUM);
	for (auto& buf : m_workerBuf)
		buf.resize(BUF_LEN);

	m_sample_count = 0;
	m_interpolatedBuf.resize(BUF_LEN);
	m_IQ_buf.resize(BUF_LEN * BYTES_PER_SAMPLE);

	m_chunkSizeSamples = 2048;
	m_chunkOffset = 0;
	m_FMdeviationKHz = 75.0e3;
	m_AM = false;
}

FMModulator::~FMModulator()
{
	m_mutex.lock();
	m_device.Close();
	m_mutex.unlock();
}

void FMModulator::SetFMDeviationKHz(double value)
{
	m_FMdeviationKHz = value;
}

bool FMModulator::Open()
{
	return m_device.Open(this);
}

void FMModulator::Close()
{
	m_device.Close();
}

void FMModulator::SetFrequency(uint64_t freg)
{
	m_device.SetFrequency(freg);
}

void FMModulator::SetGainRF(float gain)
{
	m_device.SetGain(gain);
}

void FMModulator::SetLocalGain(float gain)
{
	m_localGain = gain;
}

void FMModulator::SetAMP(bool enableamp)
{
	m_device.SetAMP(enableamp);
}

bool FMModulator::StartTX()
{
	return m_device.StartTx();
}

bool FMModulator::StopTX()
{
	return m_device.StopTx();
}

void FMModulator::SetAM(bool set)
{
	m_AM = set;
}

void FMModulator::SetChunkSizeSamples(size_t count)
{
	m_chunkSizeSamples = count;
}

uint32_t FMModulator::GetDeviceSampleRate()
{
	return m_hackrf_sample;
}

void FMModulator::SetupFormat(WavSource::PCMHeader waveMetadata)
{
	m_wavInfo = waveMetadata;
}

void FMModulator::PushSamples(const std::vector<float>& samples)
{
	m_waveBuffer = src.getData();
	m_wavInfo = src.pcmInfo();
	m_chunkOffset = 0;
	m_hackrf_sample = uint32_t((m_wavInfo.samplingRate * 1.0 / m_chunkSizeSamples) * BUF_LEN);
	m_device.SetSampleRate(m_hackrf_sample);

	m_FM_phase = 0;
	prepareNext();
	m_ready = true;

	//Make mono
	if (m_wavInfo.channels == 2) //todo
	{
		return;

		for (uint32_t i = 0; i < m_waveBuffer.size(); i+=2)
			m_waveBuffer[i] = (m_waveBuffer[i * 2] + m_waveBuffer[i * 2 + 1]) / (float)2.0;
		m_wavInfo.channels = 1;
		//m_wavInfo.sampleCount = m_wavInfo.sampleCount / 2;
	}
}

uint32_t FMModulator::GetChunkSizeSamples()
{
	return m_chunkSizeSamples;
}

void FMModulator::interpolation() 
{
	uint32_t i;		/* Input buffer index + 1. */
	uint32_t j = 0;	/* Output buffer index. */
	float pos;		/* Position relative to the input buffer
					* + 1.0. */

					/* We always "stay one sample behind", so what would be our first sample
					* should be the last one wrote by the previous call. */
	float* in_buf = &m_waveBuffer[m_chunkOffset];
	pos = (float)m_sample_count / (float)BUF_LEN;
	while (pos < 1.0)
	{
		m_interpolatedBuf[j] = m_last_in_samples[3] + (in_buf[0] - m_last_in_samples[3]) * pos;
		j++;
		pos = (float)(j + 1) * (float)m_sample_count / (float)BUF_LEN;
	}

	/* Interpolation cycle. */
	i = (uint32_t)pos;
	while (j < (BUF_LEN - 1))
	{

		m_interpolatedBuf[j] = in_buf[i - 1] + (in_buf[i] - in_buf[i - 1]) * (pos - (float)i);
		j++;
		pos = (float)(j + 1) * (float)m_sample_count / (float)BUF_LEN;
		i = (uint32_t)pos;
	}

	/* The last sample is always the same in input and output buffers. */
	m_interpolatedBuf[j] = in_buf[m_sample_count - 1];

	/* Copy last samples to m_last_in_samples (reusing i and j). */
	for (i = m_sample_count - 4, j = 0; j < 4; i++, j++)
		m_last_in_samples[j] = in_buf[i];
}

void FMModulator::modulation() 
{
	double fm_deviation = 2.0 * M_PI * m_FMdeviationKHz / m_hackrf_sample;

	//AM mode
	if (m_AM) 
	{
		for (uint32_t i = 0; i < BUF_LEN; i++) 
		{
			double	audio_amp = m_interpolatedBuf[i] * m_localGain;

			if (fabs(audio_amp) > 1.0)
				audio_amp = (audio_amp > 0.0) ? 1.0 : -1.0;

			m_IQ_buf[i * BYTES_PER_SAMPLE] = (float)audio_amp;
			m_IQ_buf[i * BYTES_PER_SAMPLE + 1] = 0;
		}
	}
	//FM mode
	else 
	{
		for (uint32_t i = 0; i < BUF_LEN; i++) 
		{
			double audio_amp = m_interpolatedBuf[i] * m_localGain;

			if (fabs(audio_amp) > 1.0)
				audio_amp = (audio_amp > 0.0) ? 1.0 : -1.0;

			m_FM_phase += fm_deviation * audio_amp;
			while (m_FM_phase > (float)(M_PI))
				m_FM_phase -= (float)(2.0 * M_PI);
			while (m_FM_phase < (float)(-M_PI))
				m_FM_phase += (float)(2.0 * M_PI);

			m_IQ_buf[i * BYTES_PER_SAMPLE] = (float)sin(m_FM_phase);
			m_IQ_buf[i * BYTES_PER_SAMPLE + 1] = (float)cos(m_FM_phase);
		}
	}
}

void FMModulator::work(size_t offset) 
{
	m_mutex.lock();
	auto& buf = m_workerBuf[m_head];
	for (uint32_t i = 0; i < BUF_LEN; i++) 
		buf[i] = (int8_t)(m_IQ_buf[i + offset] * 127.0);
	m_head = (m_head + 1) % BUF_NUM;
	m_leftToSend++;
	m_mutex.unlock();
}

int FMModulator::onData(int8_t* buffer, uint32_t length)
{
	m_mutex.lock();

	if (m_leftToSend == 0)
		memset(buffer, 0, length);
	else 
	{
		int8_t* p = &(m_workerBuf[m_tail][0]);
		memcpy(buffer, p, length);
		m_tail = (m_tail + 1) % BUF_NUM;
		m_leftToSend--;
		if (m_leftToSend == 0)
			m_ready = true;
	}
	m_mutex.unlock();

	return 0;
}

void FMModulator::prepareNext()
{
	auto samples = m_waveBuffer.size();
	if (m_chunkOffset >= samples)
		return;

	if (m_chunkOffset + m_chunkSizeSamples > samples)
		m_sample_count = samples - m_chunkOffset;
	else
		m_sample_count = m_chunkSizeSamples;

	uint32_t newRFSampleRate = uint32_t((m_wavInfo.samplingRate * 1.0 / m_chunkSizeSamples) * BUF_LEN);
	if (m_hackrf_sample != newRFSampleRate)
	{
		m_hackrf_sample = newRFSampleRate;
		m_device.SetSampleRate(m_hackrf_sample);
	}

	interpolation();
	modulation();

	m_chunkOffset += m_sample_count;
}

void FMModulator::NextChunk()
{
	auto samples = m_waveBuffer.size();
	if (m_chunkOffset >= samples)
		return;

	m_ready = false;
	for (uint32_t i = 0; i < (BUF_LEN * BYTES_PER_SAMPLE); i += BUF_LEN)
		work(i);

	prepareNext();
}

bool FMModulator::ReadyNext()
{
	return m_ready;
}