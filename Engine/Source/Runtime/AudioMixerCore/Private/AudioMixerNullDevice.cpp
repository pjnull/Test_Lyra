// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerNullDevice.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Event.h"
#include "AudioMixerLog.h"

namespace Audio
{
	uint32 FMixerNullCallback::Run()
	{
		//
		// To simulate an audio device requesting for more audio, we sleep between callbacks.
		// The problem with this is that OS/Kernel Sleep is not accurate. It will always be slightly higher than requested,
		// which means that audio will be generated slightly slower than the stated sample rate.
		// To correct this, we keep track of the real time passed, and adjust the sleep time accordingly so the audio clock
		// stays as close to the real time clock as possible.

		double AudioClock = FPlatformTime::Seconds();

		check(SleepEvent);

		float SleepTime = CallbackTime; 

		UE_LOG(LogAudioMixer, Display, TEXT("FMixerNullCallback: Simulating a h/w device callback at [%dms]"), (int32)(CallbackTime*1000.f));

		while (!bShouldShutdown)
		{
			Callback();

			// Clamp to Maximum of 200ms.
			float SleepTimeClampedMs = FMath::Clamp<float>(SleepTime * 1000.f, 0.f, 200.f);

			// Wait with a timeout of our sleep time. Triggering the event will leave the wait early.
			SleepEvent->Wait((int32)SleepTimeClampedMs);
			SleepEvent->Reset();

			AudioClock += CallbackTime;
			double RealClock = FPlatformTime::Seconds();
			double AudioVsReal = RealClock - AudioClock;
			
			// For the next sleep, we adjust the sleep duration to try and keep the audio clock as close
			// to the real time clock as possible
			SleepTime = CallbackTime - AudioVsReal;

#if !NO_LOGGING
			// Warn if there's any crazy deltas (limit to every 30s).
			static double LastLog = 0.f;
			if (RealClock - LastLog > 30.f)
			{
				if (FMath::Abs(SleepTime) > 0.2f)
				{
					UE_LOG(LogAudioMixer, Warning, TEXT("FMixerNullCallback: Large time delta between simulated audio clock and realtime [%dms]"), (int32)(SleepTime * 1000.f));					
					LastLog = RealClock;
				}
			}
#endif //!NO_LOGGING
		}
		return 0;
	}

	FMixerNullCallback::FMixerNullCallback(float InBufferDuration, TFunction<void()> InCallback, EThreadPriority ThreadPriority)
		: Callback(InCallback)
		, CallbackTime(InBufferDuration)
		, bShouldShutdown(false)
		, SleepEvent(FPlatformProcess::GetSynchEventFromPool(true))
	{
		check(SleepEvent);
		SleepEvent->Reset();
		CallbackThread.Reset(FRunnableThread::Create(this, TEXT("AudioMixerNullCallbackThread"), 0, ThreadPriority, FPlatformAffinity::GetAudioThreadMask()));
	}
		
	void FMixerNullCallback::Stop()
	{
		// Flag loop to exit 
		bShouldShutdown = true;

		if (SleepEvent)
		{
			// Exit any sleep we're inside.
			SleepEvent->Trigger();

			if (CallbackThread.IsValid())
			{
				CallbackThread->WaitForCompletion();
			}
		
			FPlatformProcess::ReturnSynchEventToPool(SleepEvent);
			SleepEvent = nullptr;
		}
	}
}
