// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VoiceComponent.generated.h"

class IVoiceCapture;
class USoundWaveProcedural;
class FSocket;

#define BUFFER_SIZE (32 * 1024)

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOICE3_API UVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

	TSharedPtr<IVoiceCapture> VoiceCapture;
	TArray<uint8> Buffer;
	bool Flush = false;

	FSocket* Listener;
	FSocket* Socket;


	uint8 VoiceBuffer[BUFFER_SIZE];
	double LastReceive = 0;

public:
	UPROPERTY(BlueprintReadOnly)
		USoundWaveProcedural* SoundWave;

	UPROPERTY(BlueprintReadOnly)
		bool Peak;

	UPROPERTY(BlueprintReadWrite)
		bool LocallyControlled = false;

	// Sets default values for this component's properties
	UVoiceComponent();

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere)
		float Threshold = 160000;

	UPROPERTY(EditAnywhere)
		float Delay = 0.15;

	UFUNCTION(BlueprintCallable, Category = "Socket")
		void Listen(int32& Addr, int32& Port);

	UFUNCTION(BlueprintCallable, Category = "Socket")
		void Connect(int32 Addr, int32 Port);
};
