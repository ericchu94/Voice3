// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VoiceComponent.generated.h"

class IVoiceCapture;
class IVoiceEncoder;
class IVoiceDecoder;
class USoundWaveProcedural;

#define BUFFER_SIZE (32 * 1024)

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOICE3_API UVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

	TSharedPtr<IVoiceCapture> VoiceCapture;
	TSharedPtr<IVoiceEncoder> VoiceEncoder;
	TSharedPtr<IVoiceDecoder> VoiceDecoder;
	TArray<uint8> Buffer;
	bool CapturedLastTick = false;

public:	
	UPROPERTY(BlueprintReadOnly)
	USoundWaveProcedural* SoundWave;

	// Sets default values for this component's properties
	UVoiceComponent();

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere)
		float Threshold = 400;

	UPROPERTY(EditAnywhere)
		float Delay = 0.15;

	UFUNCTION(BlueprintCallable, Category = "Voice")
		TArray<uint8> GetVoiceData(bool Compressed);

	UFUNCTION(BlueprintCallable, Category = "Voice")
		void AddVoiceData(bool Compressed, TArray<uint8> Data);
};
