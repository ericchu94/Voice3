// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice3.h"
#include "VoicePipe.h"
#include "Runtime/Online/Voice/Public/VoiceModule.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCapture.h"
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"
#include "Runtime/Online/Voice/Public/Voice.h"

VoicePipe::VoicePipe(float Threshold, float Delay, TSharedPtr<IVoiceCapture> VoiceCapture) : Threshold(Threshold), Delay(Delay), VoiceCapture(VoiceCapture)
{
}

VoicePipe::~VoicePipe()
{
}

USoundWaveProcedural* VoicePipe::GetSoundWave() {
	return SoundWave;
}

bool VoicePipe::Init()
{
	// Initialize SoundWave
	SoundWave = NewObject<USoundWaveProcedural>();
	SoundWave->SampleRate = VOICE_SAMPLE_RATE;
	SoundWave->NumChannels = NUM_VOICE_CHANNELS;
	SoundWave->Duration = TNumericLimits<float>::Max();
	return true;
}

uint32 VoicePipe::Run()
{
	VoiceCapture->Start();
	// 32k seemed to be average, double for margin
	uint32 BufferSize = 64 * 1024;
	uint8 *Buffer = new uint8[BufferSize];
	double LastCaptureTime = 0;
	while (!ShouldStop) {
		uint32 AvailableVoiceData;
		if (VoiceCapture->GetVoiceData(Buffer, BufferSize, AvailableVoiceData) == EVoiceCaptureState::BufferTooSmall) {
			UE_LOG(LogTemp, Warning, TEXT("Buffer too small"));
		}

		if (AvailableVoiceData > 0) {
			SoundWave->QueueAudio(Buffer, AvailableVoiceData);
			LastCaptureTime = FPlatformTime::Seconds();

			uint32 Samples = AvailableVoiceData / 2;
			int16* SamplePtr = reinterpret_cast<int16*>(Buffer);
			int64 Sum = 0;
			for (uint32 i = 0; i < Samples; i++) {
				Sum += SamplePtr[i];
			}
			int64 Average = Sum / Samples;
			Sum = 0;
			for (uint32 i = 0; i < Samples; i++) {
				int64 Diff = SamplePtr[i] - Average;
				Sum += Diff * Diff;
			}
			float Amplitude = FMath::Sqrt(Sum / Samples);
			//UE_LOG(LogTemp, Log, TEXT("Amplitude: %f"), Amplitude);
			if (Amplitude > Threshold) {
				MouthOpen = true;
				UE_LOG(LogTemp, Log, TEXT("Open"));
			}
			else {
				MouthOpen = false;
				UE_LOG(LogTemp, Log, TEXT("Low Amp Close"));
			}
		}
		else if (MouthOpen && FPlatformTime::Seconds() - LastCaptureTime > Delay) {
			MouthOpen = false;
			UE_LOG(LogTemp, Log, TEXT("Silence Close"));
		}
	}
	delete Buffer;
	Buffer = NULL;
	VoiceCapture->Stop();
	return 0;
}

void VoicePipe::Stop()
{
	ShouldStop = true;
}

void VoicePipe::Exit()
{
	VoiceCapture->Shutdown();
}

bool VoicePipe::IsMouthOpen() {
	return MouthOpen;
}