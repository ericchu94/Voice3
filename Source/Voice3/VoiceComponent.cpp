// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice3.h"
#include "VoiceComponent.h"
#include "VoicePipe.h"
#include "Runtime/Online/Voice/Public/VoiceModule.h"
#include "Runtime/Online/Voice/Public/Interfaces/VoiceCapture.h"

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
	TSharedPtr<IVoiceCapture> VoiceCapture = FVoiceModule::Get().CreateVoiceCapture();
	if (!VoiceCapture.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("VoiceCapture is not valid"));
		return;
	}
	Pipe = new VoicePipe(Threshold, Delay, VoiceCapture);
	this->Thread = FRunnableThread::Create(Pipe, TEXT("VoicePipe"));
}

void UVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	this->Thread->Kill();
	delete this->Thread;
	this->Thread = NULL;
	delete Pipe;
	Pipe = NULL;
}

USoundWaveProcedural* UVoiceComponent::GetSoundWave() {
	return Pipe->GetSoundWave();
}

bool UVoiceComponent::IsMouthOpen() {
	return Pipe->IsMouthOpen();
}
