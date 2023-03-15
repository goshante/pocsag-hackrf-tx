#include <Windows.h>
#include <iostream>
#include "WavSource.h"
#include "FMModulator.h"
#include "POCSAG.h"

#define HACKRF_SAMPLE 2000000

int main(int argc, char* argv[])
{
	POCSAG::Encoder pocsag(44100);
	pocsag.SetAmplitude(8000);
	std::vector<uint8_t> result;
	pocsag.SetDateFormat(POCSAG::DateFormat::Begin);
	pocsag.encode(result, 403361, POCSAG::Type::Alphanuberic, "Bibos Drulis!", POCSAG::BPS::BPS_512);

	//WavSource* wav = new WavSource("D:\\NOLF2\\Game\\SND\\AMB\\RADIOCHATTER_1.WAV", SAMPLE_COUNT);
	WavSource* wav = new WavSource("D:\\dsd\\test2.wav");
	FMModulator* mod = new FMModulator(90);

	if (!mod->Open())
		return 0;

	mod->SetupFormat(wav->pcmInfo());
	mod->PushSamples(wav->getData());
	mod->SetChunkSizeSamples(4096);
	mod->SetFrequency(161'125'000);
	mod->SetFMDeviationKHz(25.0e3);
	mod->SetGainRF(40);
	mod->SetAMP(true);
	mod->StartTX();

	while (((1 << 15) & GetAsyncKeyState(VK_ESCAPE)) == false)
	{
		Sleep(5);
	}

	mod->StopTX();
	delete(mod);
	return 0;
}