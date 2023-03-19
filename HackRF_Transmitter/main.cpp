#include <Windows.h>
#include <iostream>
#include "HackRF_PCMSource.h"
#include "HackRFTransmitter.h"
#include "POCSAG.h"

int main()
{
	try
	{
		POCSAG::Encoder pocsag;
		std::vector<uint8_t> message;
		pocsag.SetAmplitude(8000); //PCM Sample amplitude. POCSAG PCMs always 16bit.
		pocsag.SetDateTimePosition(POCSAG::DateTimePosition::Begin); //Append date/time to begin of the message

		//Make alphanumeric message for pager with RIC 1234567, 512 bitrate and encode text as latin
		pocsag.encode(message, 1234567, POCSAG::Type::Alphanumeric, "Test message. Hello world!", POCSAG::BPS::BPS_512, POCSAG::Charset::Latin);

		//Prepare message PCM data for TX
		//You can make HackRF_PCMSource() from any PCM data. It supports only PCM raw format. 8, 16, 24 and 32 bits.
		//Mono or stereo. If you use stereo it will be re-sampled to mono.
		HackRF_PCMSource pcm(message);
		HackRFTransmitter tx;
		tx.PushSamples(pcm); //Push new pack of samples. This pack is called "chunk"
		tx.SetSubChunkSizeSamples(4096); //Each chunk is splitted on subchunks, 4096 samples each
		tx.SetFrequency(141,300); //141.300 MHz
		tx.SetFMDeviationKHz(25.0); //Width of signal
		tx.SetAMP(true); //Enable amplifier
		tx.SetGainRF(40); //Also amplifies signal
		tx.SetTurnOffTXWhenIdle(true); //Turn off transmitter every time when we finished all TX queue
		tx.StartTX(); //TX is handling in internal thread

		bool pushed = false; //just for demonstration convenience
		while (true)
		{
			//Break the loop and stop the program on Escape key
			if ((1 << 15) & GetAsyncKeyState(VK_ESCAPE))
				break;

			//Press left Ctrl to push generated message into TX queue again
			if ((1 << 15) & GetAsyncKeyState(VK_LCONTROL) && !pushed)
			{
				tx.PushSamples(pcm);
				pushed = true; 
			}

			if (tx.IsIdle()) //Idle state is when TX is on, but nothing to transmit
				pushed = false;
		}

		tx.StopTX();
	}
	catch (const std::exception& ex)
	{
		std::cout << "Error: " << ex.what() << std::endl;
	}
	
	system("pause");
	return 0;
}