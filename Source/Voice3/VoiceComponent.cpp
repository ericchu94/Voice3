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

	VoiceCapture->Stop();
	VoiceCapture->Shutdown();
	VoiceEncoder->Destroy();
	VoiceDecoder->Destroy();
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
		UE_LOG(LogTemp, Log, TEXT("%s: %d"), *LocalAddress->ToString(true), LocallyControlled ? 1 : 0);

		if (!LocallyControlled) {
			uint32 PendingDataSize;
			if (Socket->HasPendingData(PendingDataSize)) {
				uint8* Data = new uint8[PendingDataSize];
				int32 BytesRead;
				Socket->Recv(Data, PendingDataSize, BytesRead, ESocketReceiveFlags::Type::None);
				UE_LOG(LogTemp, Log, TEXT("Received %d bytes"), BytesRead);

				// TODO append to waveform
				SoundWave->QueueAudio(Data, BytesRead);

				delete Data;
			}
			else {
				TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->CreateInternetAddr();
				Socket->GetAddress(*LocalAddress);
				//UE_LOG(LogTemp, Log, TEXT("No data: %s"), *LocalAddress->ToString(true));
			}
		}
		else {
			uint32 AvailableVoiceData;
			if (VoiceCapture->GetVoiceData(VoiceBuffer, BUFFER_SIZE, AvailableVoiceData) == EVoiceCaptureState::BufferTooSmall) {
				UE_LOG(LogTemp, Warning, TEXT("Buffer too small"));
			}
			if (AvailableVoiceData > 0) {
				int32 BytesSent;
				Socket->Send(VoiceBuffer, AvailableVoiceData, BytesSent);
				TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
				Socket->GetPeerAddress(*RemoteAddress);
				UE_LOG(LogTemp, Log, TEXT("Sent %d bytes to %s"), BytesSent, *RemoteAddress->ToString(true));
			}
		}
	}
}

/*
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
*/

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