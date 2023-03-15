#ifndef _FMMODULATOR_H
#define _FMMODULATOR_H

#include <mutex>
#include "IHackRFData.h"
#include "HackRFDevice.h"
#include "WavSource.h"
#include <atomic>
#include <thread>
#include <queue>
#include <future>

class FMModulator : public IHackRFData
{
private:
	using WorkerBuf_t = std::vector<std::vector<int8_t>>;
	using PCMChunk_t = std::vector<float>;
	using PCMQueue_t = std::queue<PCMChunk_t>;

	HackRFDevice m_device;
	std::mutex m_mutex;
	int m_leftToSend;
	WorkerBuf_t m_workerBuf;
	int m_tail;
	int m_head;
	float m_localGain;
	std::vector<float> m_interpolatedBuf;
	std::vector<float> m_IQ_buf;
	uint32_t m_sample_rate;
	size_t m_sample_count;
	uint32_t m_hackrf_sample;
	float m_last_in_samples[4];
	uint32_t m_subchunkSizeSamples;
	PCMQueue_t m_waveQueue;
	PCMChunk_t m_currentChunk;
	WavSource::PCMHeader m_wavInfo;
	size_t m_subchunkOffset;
	double m_FMdeviationKHz;
	bool m_AM;
	std::atomic<bool> m_ready;
	double m_FM_phase;
	std::thread* m_queueThread;
	std::promise<bool> m_stopped;
	std::promise<bool> m_started;
	std::atomic<bool> m_stop;
	std::atomic<bool> m_emptyQueue;

	void _interpolation();
	void _modulation();
	void _work(size_t offset);
	bool _prepareNext();
	void _workerThread();
	void _nextSubChunk();

protected:
	int onData(int8_t* buffer, uint32_t length);

public:
	FMModulator(float localGain);
	~FMModulator();

	void SetSubChunkSizeSamples(size_t count);
	void SetupFormat(WavSource::PCMHeader waveMetadata);
	void PushSamples(const std::vector<float>& samples);
	

	uint32_t GetDeviceSampleRate();
	uint32_t GetChunkSizeSamples();

	void SetFrequency(uint64_t mhz, uint64_t khz, uint64_t hz = 0);
	void SetGainRF(float gain);
	void SetLocalGain(float gain);
	void SetAMP(bool enableamp);
	bool StartTX();
	bool StopTX();
	void SetAM(bool set);
	void SetFMDeviationKHz(double value);
	bool IsIdle();
};

#endif

