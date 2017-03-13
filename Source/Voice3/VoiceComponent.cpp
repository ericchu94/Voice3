// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice3.h"
#include "VoiceComponent.h"
#include "VoicePipe.h"
#include "Runtime/Online/Voice/Public/VoiceModule.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCapture.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCodec.h"
#include "Runtime/Online/Voice/Public/Voice.h"
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"

// Sets default values for this component's properties
UVoiceComponent::UVoiceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	// VoiceCapture must be created on game thread
	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture();
	if (!VoiceCapture.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceCapture is not valid"));
		return;
	}
	VoiceEncoder = FVoiceModule::Get().CreateVoiceEncoder();
	if (!VoiceEncoder.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceEncoder is not valid"));
		return;
	}
	VoiceDecoder = FVoiceModule::Get().CreateVoiceDecoder();
	if (!VoiceDecoder.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceDecoder is not valid"));
		return;
	}
	SoundWave = NewObject<USoundWaveProcedural>();
	SoundWave->SampleRate = VOICE_SAMPLE_RATE;
	SoundWave->NumChannels = NUM_VOICE_CHANNELS;
	SoundWave->Duration = TNumericLimits<float>::Max();

	VoiceCapture->Start();
}

void UVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	VoiceCapture->Stop();
	VoiceCapture->Shutdown();
	VoiceEncoder->Destroy();
	VoiceDecoder->Destroy();
}

void UVoiceComponent::AddVoiceData(bool Compressed, TArray<uint8> VoiceData) {
	if (VoiceData.Num() == 0)
		return;

	TArray<uint8> AudioData;
	if (Compressed) {
		AudioData.SetNumUninitialized(BUFFER_SIZE);
		uint32 AudioDataSize = BUFFER_SIZE;
		VoiceDecoder->Decode(VoiceData.GetData(), VoiceData.Num(), AudioData.GetData(), AudioDataSize);
		AudioData.SetNum(AudioDataSize);
	}
	else
	{
		AudioData = VoiceData;
	}
	SoundWave->QueueAudio(AudioData.GetData(), AudioData.Num());
}


TArray<uint8> UVoiceComponent::GetVoiceData(bool Compressed)
{
	// Check valid states
	if (!VoiceCapture.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceCapture is not valid"));
		return TArray<uint8>();
	}
	if (!VoiceEncoder.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceEncoder is not valid"));
		return TArray<uint8>();
	}
	if (!VoiceDecoder.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceDecoder is not valid"));
		return TArray<uint8>();
	}

	// Read from mic
	TArray<uint8> VoiceData;
	VoiceData.SetNumUninitialized(BUFFER_SIZE);
	uint32 AvailableVoiceData;
	if (VoiceCapture->GetVoiceData(VoiceData.GetData(), BUFFER_SIZE, AvailableVoiceData) == EVoiceCaptureState::BufferTooSmall) {
		UE_LOG(LogTemp, Warning, TEXT("Buffer too small"));
		return TArray<uint8>();
	}
	Buffer.Append(VoiceData.GetData(), AvailableVoiceData);

	// If nothing in Buffer, return empty array
	if (Buffer.Num() == 0) {
		return TArray<uint8>();
	}

	TArray<uint8> AudioData;
	if (Compressed) {
		if (AvailableVoiceData == 0) {
			CapturedLastTick = false;
			Buffer.AddZeroed(1024);
		}
		else {
			CapturedLastTick = true;
		}


		// Compress data
		int32 FragmentSize = FMath::Min<int>(FRAGMENT_SIZE, Buffer.Num());
		TArray<uint8> CompressedData;
		CompressedData.SetNumUninitialized(FragmentSize);
		uint32 CompressedDataSize = FragmentSize;
		int32 BytesSkipped = VoiceEncoder->Encode(Buffer.GetData(), FragmentSize, CompressedData.GetData(), CompressedDataSize);
		if (BytesSkipped != 0) {
			UE_LOG(LogTemp, Warning, TEXT("Skipped %d bytes"), BytesSkipped);
		}

		Buffer.RemoveAt(0, FragmentSize);

		CompressedData.SetNum(CompressedDataSize);
		AudioData = CompressedData;
	} else {
		AudioData.SetNum(FMath::Min<int>(FRAGMENT_SIZE, Buffer.Num()));
		FMemory::Memcpy(AudioData.GetData(), Buffer.GetData(), AudioData.Num());
		Buffer.RemoveAt(0, AudioData.Num());
	}
	
	return AudioData;
}