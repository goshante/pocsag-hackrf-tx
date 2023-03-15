#include "FMModulator.h"

constexpr const uint32_t BUF_NUM			= 256;
constexpr const uint32_t BYTES_PER_SAMPLE	= 2;
constexpr const double M_PI					= 3.14159265358979323846;
constexpr const uint32_t BUF_LEN			= 262144;         //hackrf tx buf

using namespace std::chrono_literals;

FMModulator::FMModulator(float localGain)
	: m_localGain(localGain / (float)100.0)
	, m_ready(false)
	, m_FM_phase(0)
	, m_queueThread(nullptr)
	, m_stop(true)
	, m_emptyQueue(true)
{
	memset(m_last_in_samples, 0, sizeof(m_last_in_samples));
	
	m_leftToSend = m_tail = m_head = 0;

	m_workerBuf.resize(BUF_NUM);
	for (auto& buf : m_workerBuf)
		buf.resize(BUF_LEN);

	m_sample_count = 0;
	m_interpolatedBuf.resize(BUF_LEN);
	m_IQ_buf.resize(BUF_LEN * BYTES_PER_SAMPLE);

	m_subchunkSizeSamples = 2048;
	m_subchunkOffset = 0;
	m_FMdeviationKHz = 75.0e3;
	m_AM = false;

	if (!m_device.Open(this))
		throw std::runtime_error("Failed to open HackRF device.");
}

FMModulator::~FMModulator()
{
	if (m_device.IsRunning())
		StopTX();
	m_mutex.lock();
	m_device.Close();
	m_mutex.unlock();
}

void FMModulator::SetFMDeviationKHz(double value)
{
	m_FMdeviationKHz = value * 1000;
}

void FMModulator::SetFrequency(uint64_t mhz, uint64_t khz, uint64_t hz)
{
	m_device.SetFrequency((mhz * 1000000) + (khz * 1000) + hz);
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
	if ((m_waveQueue.empty() && m_currentChunk.empty()) || m_device.IsRunning())
		return false;

	if (m_currentChunk.empty())
	{
		m_subchunkOffset = 0;
		m_hackrf_sample = uint32_t((m_wavInfo.samplingRate * 1.0 / m_subchunkSizeSamples) * BUF_LEN);
		m_device.SetSampleRate(m_hackrf_sample);
		m_FM_phase = 0;
	}
	m_stopped = {};
	m_started = {};
	m_stop = false;
	m_ready = true;

	if (m_queueThread)
		delete m_queueThread;

	m_queueThread = new std::thread(&FMModulator::_workerThread, this);
	auto fut = m_started.get_future();
	if (fut.wait_for(10s) != std::future_status::ready)
		return false;

	return fut.get();
}

void FMModulator::_workerThread()
{
	if (!m_device.StartTx())
	{
		m_started.set_value(false);
		return;
	}

	while (!m_stop)
	{
		if (!m_currentChunk.empty())
			goto continueSubchunk;

		if (m_waveQueue.empty())
			m_emptyQueue = true;
		else
		{
			if (m_subchunkOffset >= m_currentChunk.size())
			{
				m_currentChunk = std::move(m_waveQueue.front());
				m_waveQueue.pop();
				if (!_prepareNext())
				{
					m_currentChunk.clear();
					continue;
				}
			}

		continueSubchunk:
			while (!m_stop)
			{
				if (m_ready)
				{
					_nextSubChunk();
					if (!_prepareNext())
						break;
				}
			}

			if (!m_stop)
				m_currentChunk.clear();
		}
	}

	m_stopped.set_value(!m_device.StopTx());
}

bool FMModulator::StopTX()
{
	if (!m_device.IsRunning())
		return false;


	m_stop = true;
	auto fut = m_stopped.get_future();
	if (fut.wait_for(30s) != std::future_status::ready)
		throw std::runtime_error("Failed to stop TX. Timeout.");
	bool stopped = fut.get();

	m_queueThread->join();
	delete m_queueThread;
	m_queueThread = nullptr;

	return stopped;
}

void FMModulator::SetAM(bool set)
{
	m_AM = set;
}

void FMModulator::SetSubChunkSizeSamples(size_t count)
{
	m_subchunkSizeSamples = count;
}

uint32_t FMModulator::GetDeviceSampleRate()
{
	return m_hackrf_sample;
}

void FMModulator::SetupFormat(WavSource::PCMHeader waveMetadata)
{
	if (m_device.IsRunning())
		throw std::runtime_error("Trying to change format while TX is active.");

	m_wavInfo = waveMetadata;
}

void FMModulator::PushSamples(const std::vector<float>& samples)
{
	m_mutex.lock();
	m_waveQueue.push(samples);
	m_emptyQueue = false;
	m_mutex.unlock();
}

uint32_t FMModulator::GetChunkSizeSamples()
{
	return m_subchunkSizeSamples;
}

void FMModulator::_interpolation() 
{
	uint32_t i;		/* Input buffer index + 1. */
	uint32_t j = 0;	/* Output buffer index. */
	float pos;		/* Position relative to the input buffer
					* + 1.0. */

					/* We always "stay one sample behind", so what would be our first sample
					* should be the last one wrote by the previous call. */
	float* in_buf = &m_currentChunk[m_subchunkOffset];
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

void FMModulator::_modulation() 
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

void FMModulator::_work(size_t offset) 
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

bool FMModulator::_prepareNext()
{
	auto samples = m_currentChunk.size();
	if (m_subchunkOffset >= samples)
		return false;

	if (m_subchunkOffset + m_subchunkSizeSamples > samples)
		m_sample_count = samples - m_subchunkOffset;
	else
		m_sample_count = m_subchunkSizeSamples;

	uint32_t newRFSampleRate = uint32_t((m_wavInfo.samplingRate * 1.0 / m_subchunkSizeSamples) * BUF_LEN);
	if (m_hackrf_sample != newRFSampleRate)
	{
		m_hackrf_sample = newRFSampleRate;
		m_device.SetSampleRate(m_hackrf_sample);
	}

	_interpolation();
	_modulation();

	m_subchunkOffset += m_sample_count;
	return true;
}

void FMModulator::_nextSubChunk()
{
	auto samples = m_currentChunk.size();
	if (m_subchunkOffset >= samples)
		return;

	m_ready = false;
	for (uint32_t i = 0; i < (BUF_LEN * BYTES_PER_SAMPLE); i += BUF_LEN)
		_work(i);
}

bool FMModulator::IsIdle()
{
	return m_ready && !m_emptyQueue && m_device.IsRunning();
}