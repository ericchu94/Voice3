// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice3.h"
#include "VoiceComponent.h"
#include "VoicePipe.h"
#include "Runtime/Online/Voice/Public/VoiceModule.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCapture.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCodec.h"
#include "Runtime/Online/Voice/Public/Voice.h"
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"

// Sets default values for this component's properties
UVoiceComponent::UVoiceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

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

	if (VoiceCapture.IsValid()) {
		VoiceCapture->Stop();
		VoiceCapture->Shutdown();
	}
	if (VoiceEncoder.IsValid()) {
		VoiceEncoder->Destroy();
	}
	if (VoiceDecoder.IsValid()) {
		VoiceDecoder->Destroy();
	}
}

/*
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
*/

// Called every frame
void UVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// ...
	if (Listener && !Socket) {
		bool Pending;
		if (Listener->HasPendingConnection(Pending) && Pending) {
			TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
			FSocket* Sock = Listener->Accept(*RemoteAddress, TEXT("SocketComponent Client"));

			if (Sock) {
				Socket = Sock;
				UE_LOG(LogTemp, Log, TEXT("Connection from %s"), *RemoteAddress->ToString(true));
			}
		}
	}

	if (Socket) {
		TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->CreateInternetAddr();
		Socket->GetAddress(*LocalAddress);

		if (!LocallyControlled) {
			uint32 PendingDataSize;
			if (Socket->HasPendingData(PendingDataSize)) {
				int32 CompressedDataSize;
				Socket->Recv(CompressedBuffer, PendingDataSize, CompressedDataSize, ESocketReceiveFlags::Type::None);
				UE_LOG(LogTemp, Log, TEXT("%s Received %d bytes"), *LocalAddress->ToString(true), CompressedDataSize);
				//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s Received %d bytes"), *LocalAddress->ToString(true), BytesRead));

				uint8* Data;
				int32 BytesRead;

				if (Compress) {
					uint32 AvailableVoiceData = BUFFER_SIZE;
					VoiceDecoder->Decode(CompressedBuffer, CompressedDataSize, VoiceBuffer, AvailableVoiceData);
					Data = VoiceBuffer;
					BytesRead = AvailableVoiceData;
				}
				else {
					Data = CompressedBuffer;
					BytesRead = CompressedDataSize;
				}

				SoundWave->QueueAudio(Data, BytesRead);

				// Peak detection
				uint32 SamplesCount = BytesRead / 2;
				int16* Samples = reinterpret_cast<int16*>(Data);
				int64 Sum = 0;
				for (uint32 i = 0; i < SamplesCount; i++) {
					Sum += Samples[i];
				}
				int64 Average = Sum / SamplesCount;
				UE_LOG(LogTemp, Log, TEXT("Average: %d"), Average);
				int64 SquaredSum = 0;
				for (uint32 i = 0; i < SamplesCount; i++) {
					int64 Diff = Samples[i] - Average;
					SquaredSum += Diff * Diff;
				}
				int64 SquaredAverage = SquaredSum / SamplesCount;
				Peak = SquaredAverage > Threshold;
			}
		}
		else {
			uint32 AvailableVoiceData;
			if (VoiceCapture->GetVoiceData(VoiceBuffer, BUFFER_SIZE, AvailableVoiceData) == EVoiceCaptureState::BufferTooSmall) {
				UE_LOG(LogTemp, Warning, TEXT("Buffer too small"));
			}
			uint8 *Data;
			int32 Count = 0;
			if (Compress) {
				if (AvailableVoiceData > 0) {
					Buffer.Append(VoiceBuffer, AvailableVoiceData);
				}
				else if (Flush) {
					Buffer.AddZeroed(1000);
				}

				if (Buffer.Num() > 0) {
					uint32 CompressedDataSize = BUFFER_SIZE;
					int32 BytesLeft = VoiceEncoder->Encode(Buffer.GetData(), Buffer.Num(), CompressedBuffer, CompressedDataSize);
					Buffer.RemoveAt(0, Buffer.Num() - BytesLeft);
					if (BytesLeft == 0) {
						Flush = false;
					}
					else if (Flush) {
						Flush = false;
						Buffer.SetNumUninitialized(0);
					}
					else {
						Flush = true;
					}
					Data = CompressedBuffer;
					Count = CompressedDataSize;
				}
				else {
					Data = NULL;
					Count = 0;
				}
			}
			else {
				Data = VoiceBuffer;
				Count = AvailableVoiceData;
			}
			if (Count > 0) {
				int32 BytesSent;
				Socket->Send(Data, Count, BytesSent);
				TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
				Socket->GetPeerAddress(*RemoteAddress);
				UE_LOG(LogTemp, Log, TEXT("Sent %d bytes to %s"), BytesSent, *RemoteAddress->ToString(true));
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Sent %d bytes to %s"), BytesSent, *RemoteAddress->ToString(true)));
			}
		}
	}
}

void UVoiceComponent::Listen(int32& Addr, int32& Port) {
	// TODO: Assert called once
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	bool CanBindAll;
	TSharedPtr<FInternetAddr> LocalAddress = SocketSubsystem->GetLocalHostAddr(*GLog, CanBindAll);

	Listener = SocketSubsystem->CreateSocket(NAME_Stream, "VoiceComponent Server");
	Listener->Bind(*LocalAddress);
	Listener->Listen(4);
	Listener->GetAddress(*LocalAddress); // Populate port field
	UE_LOG(LogTemp, Log, TEXT("Listening on %s"), *LocalAddress->ToString(true));
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Listening on %s"), *LocalAddress->ToString(true)));


	LocalAddress->GetIp((uint32&)Addr);
	LocalAddress->GetPort(Port);
}

void UVoiceComponent::Connect(int32 Addr, int32 Port) {
	// TODO: Assert called once
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
	RemoteAddress->SetIp(Addr);
	RemoteAddress->SetPort(Port);

	FSocket* Sock = SocketSubsystem->CreateSocket(NAME_Stream, "VoiceComponent Client");
	if (!Sock->Connect(*RemoteAddress)) {
		UE_LOG(LogTemp, Error, TEXT("Failed to connect to %s"), *RemoteAddress->ToString(true));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Failed to connect to %s"), *RemoteAddress->ToString(true)));
		return;
	}

	Socket = Sock;
	UE_LOG(LogTemp, Log, TEXT("Connected to %s"), *RemoteAddress->ToString(true));
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connected to %s"), *RemoteAddress->ToString(true)));
}