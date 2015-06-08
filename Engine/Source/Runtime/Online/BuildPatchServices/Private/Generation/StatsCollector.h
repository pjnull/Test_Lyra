// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace FBuildPatchServices
{
	enum EStatFormat
	{
		// Using the AccumulateTimeBegin and End functions, measured in cycles
		Timer,
		// Value measured in Bytes
		DataSize,
		// Value measured in Bytes per second
		DataSpeed,
		// Generic int64 output
		Value
	};

	class FStatsCollector
	{
	public:
		virtual volatile int64* CreateStat(const FString& Name, EStatFormat Type, int64 InitialValue = 0) = 0;
		virtual void LogStats(float TimeBetweenLogs = 0.0f) = 0;

	public:
		static uint64 GetCycles();
		static double CyclesToSeconds(const uint64 Cycles);
		static void AccumulateTimeBegin(uint64& TempValue);
		static void AccumulateTimeEnd(volatile int64* Stat, uint64& TempValue);
		static void Accumulate(volatile int64* Stat, int64 Amount);
		static void Set(volatile int64* Stat, int64 Value);
	};

	typedef TSharedRef<FStatsCollector, ESPMode::ThreadSafe> FStatsCollectorRef;
	typedef TSharedPtr<FStatsCollector, ESPMode::ThreadSafe> FStatsCollectorPtr;

	class FStatsCollectorFactory
	{
	public:
		static FStatsCollectorRef Create();
	};
}
