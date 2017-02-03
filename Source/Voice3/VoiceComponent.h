// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VoiceComponent.generated.h"

class VoicePipe;
class USoundWaveProcedural;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOICE3_API UVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

	VoicePipe *Pipe;
	FRunnableThread* Thread;

public:	
	// Sets default values for this component's properties
	UVoiceComponent();

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere)
		float Threshold = 400;

	UPROPERTY(VisibleAnywhere)
		double Delay = 0.15;

	UFUNCTION(BlueprintCallable, Category = "Voice")
		USoundWaveProcedural* GetSoundWave();

	UFUNCTION(BlueprintCallable, Category = "Voice")
		bool IsMouthOpen();
	
};
