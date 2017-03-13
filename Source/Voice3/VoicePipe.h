// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class IVoiceCapture;
class USoundWaveProcedural;

/**
 * 
 */
class VOICE3_API VoicePipe : public FRunnable
{
	bool ShouldStop = false;
	TSharedPtr<IVoiceCapture> VoiceCapture;
	USoundWaveProcedural *SoundWave;
	bool MouthOpen = false;
	float Threshold;
	float Delay;

public:
	int FrameNumber = 0;
	float SStart = -1;
	float EEnd = -1;

	VoicePipe(float Threshold, float Delay, TSharedPtr<IVoiceCapture> VoiceCapture);
	~VoicePipe();

	USoundWaveProcedural* GetSoundWave();
	bool IsMouthOpen();
	
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	virtual void Exit();
};
