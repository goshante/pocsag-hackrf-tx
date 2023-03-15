#include <Windows.h>
#include <iostream>
#include "WavSource.h"
#include "FMModulator.h"
#include "POCSAG.h"

#define HACKRF_SAMPLE 2000000

int main(int argc, char* argv[])
{
	try
	{
		POCSAG::Encoder pocsag;
		pocsag.SetAmplitude(8000);
		std::vector<uint8_t> result;
		pocsag.SetDateFormat(POCSAG::DateFormat::Begin);
		pocsag.encode(result, 403361, POCSAG::Type::Alphanuberic, "pRIWET vOPA", POCSAG::BPS::BPS_512);
		WavSource wav(result);
		FMModulator mod(90);

		mod.SetupFormat(wav.pcmInfo());
		mod.PushSamples(wav.getData());
		mod.SetSubChunkSizeSamples(4096);
		mod.SetFrequency(161, 125);
		mod.SetFMDeviationKHz(25.0);
		mod.SetGainRF(40);
		mod.SetAMP(true);
		mod.StartTX();

		while (((1 << 15) & GetAsyncKeyState(VK_ESCAPE)) == false && mod.IsIdle())
		{
			Sleep(5);
		}

		mod.StopTX();
	}
	catch (const std::exception& ex)
	{
		std::cout << "Error: " << ex.what() << std::endl;
	}
	
	system("pause");
	return 0;
}