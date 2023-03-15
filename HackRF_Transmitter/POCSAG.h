#pragma once

#include <vector>
#include <string>

namespace POCSAG
{
	using RIC = unsigned long;

	enum class Type
	{
		Numeric,
		Alphanuberic,
		Tone
	};

	enum class Function
	{
		A = 0b00,
		B = 0b01,
		C = 0b10,
		D = 0b11
	};

	enum class DateFormat
	{
		None,    //No date and time
		Begin,   //Adds date and time at the begining.
		End      //Adds date and time at the end.
	};

	enum class BPS : uint16_t
	{
		BPS_512 = 512,
		BPS_1200 = 1200,
		BPS_2400 = 2400
	};

	class Encoder
	{
	public:
		using PCMSample_t = int16_t;	//16bit
		using WaveBuffer_t = std::vector<uint8_t>;

	private:
		uint32_t m_sampleRate;
		PCMSample_t m_amplitude;
		size_t m_maxBatches;
		DateFormat m_dateFormat;

		Encoder(const Encoder&) = delete;
		Encoder& operator=(const Encoder&) = delete;

		void _modulatePOCSAG(std::vector<PCMSample_t>& output, const std::vector<uint8_t>& data, uint16_t bps);

	public:
		Encoder(size_t maxBatches = 8, uint32_t sampleRate = 44100); //This sampling rate is pretty much OK
		~Encoder();

		//Sampling rate of modulated signal
		uint32_t GetSampleRate() const;
		void SetSampleRate(uint32_t sampleRate);

		//Amplitude (or volume) of modulated signal
		PCMSample_t GetAmplitude() const;
		void SetAmplitude(uint32_t sampleRate);

		//Specify how date and time should be added to the message
		void SetDateFormat(DateFormat format);

		// Encodes your message to POCSAG paging signal
		// Takes:
		//  output    [out]          - You must specify your output buffer.
		//  address   [in]           - RIC number of receiver (User ID of pager). Value between 0 and 2097151.
		//  msg       [in, optional] - Message content. Supports 7bit ASCII for text messages, digits (and special characters) for numeric messages.
		//                             Can be empty.
		//  bps       [in]           - Bits per second for POCSAG transmission. Can be 512, 1200 or 2400. Makes no difference if rawPOCSAG is true.
		//                             Can be empty.
		//  msgType   [in]           - Type of message. Alphanumeric for text, Numeric for numbers, Tone for empty messages.
		//  func      [in, optional] - Type of notification. Depends on reciever-side implementation. Function::A by default, always works fine.
		//  rawPOCSAG [in, optional] - If false returns modulated PCM (wave) that is ready to use with your RF TX. If true returns raw POCSAG buffer.
		//							   This buffer is just raw buffer of encoded message. This argument is false by default.
		// Returns: If rawPOCSAG true returns size in bits of encoded pocsag message. If rawPOCSAG false return total count of PCM samples.
		size_t encode(std::vector<uint8_t>& output, RIC address, Type msgType, std::string msg, BPS bps, Function func = Function::A, bool rawPOCSAG = false);
	};
}