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
		pocsag.SetAmplitude(8000);
		pocsag.SetDateTimePosition(POCSAG::DateTimePosition::Begin);
		pocsag.encode(message, 1234567, POCSAG::Type::Alphanuberic, "Hello World!", POCSAG::BPS::BPS_512);

		HackRF_PCMSource pcm(message);
		HackRFTransmitter tx;
		tx.PushSamples(pcm);
		tx.SetSubChunkSizeSamples(4096);
		tx.SetFrequency(141,225);
		tx.SetFMDeviationKHz(25.0);
		tx.SetGainRF(40);
		tx.SetAMP(true);
		tx.StartTX();

		while (!tx.IsIdle())
		{
			if ((1 << 15) & GetAsyncKeyState(VK_ESCAPE))
				break;
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