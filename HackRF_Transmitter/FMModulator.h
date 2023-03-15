#ifndef _FMMODULATOR_H
#define _FMMODULATOR_H

#include <mutex>
#include "IHackRFData.h"
#include "HackRFDevice.h"
#include "WavSource.h"
#include <atomic>
#include <thread>
#include <queue>

class FMModulator : public IHackRFData
{
private:
	using WorkerBuf_t = std::vector<std::vector<int8_t>>;
	using PCMQueue_t = std::queue<std::vector<float>>;

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
	uint32_t m_chunkSizeSamples;
	PCMQueue_t m_waveBuffer;
	WavSource::PCMHeader m_wavInfo;
	size_t m_chunkOffset;
	double m_FMdeviationKHz;
	bool m_AM;
	std::atomic<bool> m_ready;
	double m_FM_phase;

	void interpolation();
	void modulation();
	void work(size_t offset);
	void prepareNext();

protected:
	int onData(int8_t* buffer, uint32_t length);

public:
	FMModulator(float localGain);
	~FMModulator();

	void SetChunkSizeSamples(size_t count);
	void SetupFormat(WavSource::PCMHeader waveMetadata);
	void PushSamples(const std::vector<float>& samples);
	
	void NextChunk();
	uint32_t GetDeviceSampleRate();
	uint32_t GetChunkSizeSamples();

	bool Open();
	void Close();
	void SetFrequency(uint64_t freg);
	void SetGainRF(float gain);
	void SetLocalGain(float gain);
	void SetAMP(bool enableamp);
	bool StartTX();
	bool StopTX();
	void SetAM(bool set);
	void SetFMDeviationKHz(double value);
	bool ReadyNext();
};

#endif

