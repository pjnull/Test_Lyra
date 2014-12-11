// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ConsoleManager.cpp: console command handling
=============================================================================*/

#include "CorePrivatePCH.h"
#include "ConsoleManager.h"
#include "ModuleManager.h"
#include "RemoteConfigIni.h"

DEFINE_LOG_CATEGORY(LogConsoleResponse);
DEFINE_LOG_CATEGORY_STATIC(LogConsoleManager, Log, All);

static inline bool IsWhiteSpace(TCHAR Value) { return Value == TCHAR(' '); }

// @param In must not be 0
bool IsGoodHelpString(const TCHAR* In)
{
	check(In);

	if(*In == 0)
	{
		return false;
	}

	bool bGoodEndChar = true;

	while(TCHAR c = *In++)
	{
		bGoodEndChar = true;

		if(c == L'\n' || c == L'\t' || c == L' ' || c == L'\r')
		{
			bGoodEndChar = false;
		}
	}

	return bGoodEndChar;
}

// Get human readable string
// @return never 0
static const TCHAR* GetSetByTCHAR(EConsoleVariableFlags InSetBy)
{
	EConsoleVariableFlags SetBy = (EConsoleVariableFlags)((uint32)InSetBy & ECVF_SetByMask);

	switch(SetBy)
	{
#define CASE(A) case ECVF_SetBy##A: return TEXT(#A);
		// Could also be done with enum reflection instead
		CASE(Constructor)
		CASE(Scalability)
		CASE(GameSetting)
		CASE(ProjectSetting)
		CASE(DeviceProfile)
		CASE(SystemSettingsIni)
		CASE(ConsoleVariablesIni)
		CASE(Commandline)
		CASE(Code)
		CASE(Console)
#undef CASE
	}
	return TEXT("<UNKNOWN>");
}

class FConsoleVariableBase : public IConsoleVariable
{
public:
	/**
	 * Constructor
	 * @param InHelp must not be 0, must not be empty
	 */
	FConsoleVariableBase(const TCHAR* InHelp, EConsoleVariableFlags InFlags)
		:Flags(InFlags), bWarnedAboutThreadSafety(false)
	{
		SetHelp(InHelp);
	}

	// interface IConsoleVariable -----------------------------------

	virtual const TCHAR* GetHelp() const
	{
		return *Help;
	}
	virtual void SetHelp(const TCHAR* Value)
	{
		check(Value);

		Help = Value;

		// for now disabled as there is no good callstack when we crash early during engine init
//		ensure(IsGoodHelpString(Value));
	}
	virtual EConsoleVariableFlags GetFlags() const
	{
		return Flags;
	}
	virtual void SetFlags(const EConsoleVariableFlags Value)
	{
		Flags = Value;
	}

	virtual class IConsoleVariable* AsVariable()
	{
		return this;
	}

	virtual void SetOnChangedCallback(const FConsoleVariableDelegate& Callback) { OnChangedCallback = Callback; }

	// ------

	bool CanChange(EConsoleVariableFlags SetBy) const
	{
		uint32 OldPri =	(uint32)Flags & ECVF_SetByMask;
		uint32 NewPri =	(uint32)SetBy & ECVF_SetByMask;

		bool bRet = NewPri >= OldPri;

		if(!bRet)
		{
			FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();
			FString CVarName = ConsoleManager.FindConsoleObjectName(this);
			UE_LOG(LogConsoleManager, Warning,
				TEXT("Console variable '%s' wasn't set (Priority %s < %s)"),
				CVarName.IsEmpty() ? TEXT("unknown?") : *CVarName,
				GetSetByTCHAR((EConsoleVariableFlags)OldPri),
				GetSetByTCHAR((EConsoleVariableFlags)NewPri)
				);
		}

		return bRet;
	}

	void OnChanged(EConsoleVariableFlags SetBy)
	{
		// you have to specify a SetBy e.g. ECVF_SetByCommandline
		check((uint32)SetBy & ECVF_SetByMask);

		// double check, if this fires we miss a if(CanChange(SetBy))
		check(CanChange(SetBy));

		// only change on main thread

		Flags = (EConsoleVariableFlags)((uint32)Flags | ECVF_Changed);

		Flags = (EConsoleVariableFlags)(((uint32)Flags & ~ECVF_SetByMask) | SetBy);

		OnChangedCallback.ExecuteIfBound(this);
	}

	
protected: // -----------------------------------------

	// not using TCHAR* to allow chars support reloading of modules (otherwise we would keep a pointer into the module)
	FString Help;
	//
	EConsoleVariableFlags Flags;
	/** User function to call when the console variable is changed */
	FConsoleVariableDelegate OnChangedCallback;

	/** True if this console variable has been used on the wrong thread and we have warned about it. */
	mutable bool bWarnedAboutThreadSafety;

	// @return 0:main thread, 1: render thread, later more
	uint32 GetShadowIndex() const
	{
		if((uint32)Flags & ECVF_RenderThreadSafe)
		{
			return IsInGameThread() ? 0 : 1;
		}
		else
		{
			FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();
			if(ConsoleManager.IsThreadPropagationThread() && FPlatformProcess::SupportsMultithreading())
			{
				if(!bWarnedAboutThreadSafety)
				{
					FString CVarName = ConsoleManager.FindConsoleObjectName(this);
					UE_LOG(LogConsoleManager, Warning,
						TEXT("Console variable '%s' used in the render thread. Rendering artifacts could happen. Use ECVF_RenderThreadSafe or don't use in render thread."),
						CVarName.IsEmpty() ? TEXT("unknown?") : *CVarName
						);
					bWarnedAboutThreadSafety = true;
				}
			}
			// other threads are not handled at the moment (e.g. sound)
		}
	
		return 0;
	}
};

class FConsoleCommandBase : public IConsoleCommand
{
public:
	/**
	 * Constructor
	 * @param InHelp must not be 0, must not be empty
	 */
	FConsoleCommandBase(const TCHAR* InHelp, EConsoleVariableFlags InFlags)
		: Help(InHelp), Flags(InFlags)
	{
		check(InHelp);
		//check(*Help != 0); for now disabled as there callstack when we crash early during engine init
	}

	// interface IConsoleVariable -----------------------------------

	virtual const TCHAR* GetHelp() const
	{
		return *Help;
	}
	virtual void SetHelp(const TCHAR* InValue)
	{
		check(InValue);
		check(*InValue != 0);

		Help = InValue;
	}
	virtual EConsoleVariableFlags GetFlags() const
	{
		return Flags;
	}
	virtual void SetFlags(const EConsoleVariableFlags Value)
	{
		Flags = Value;
	}

	virtual struct IConsoleCommand* AsCommand()
	{
		return this;
	}

private: // -----------------------------------------

	// not using TCHAR* to allow chars support reloading of modules (otherwise we would keep a pointer into the module)
	FString Help;

	EConsoleVariableFlags Flags;
};

template <class T>
void OnCVarChange(T& Dst, const T& Src, EConsoleVariableFlags Flags)
{
	FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();

	if(IsInGameThread())
	{
		if((Flags & ECVF_RenderThreadSafe) && ConsoleManager.GetThreadPropagationCallback())
		{
			// defer the change to be in order with other rendering commands
			ConsoleManager.GetThreadPropagationCallback()->OnCVarChange(Dst, Src);
		}
		else
		{
			// propagate the change right away
			Dst = Src;
		}
	}
	else
	{
		// CVar Changes can only be initiated from the main thread
		check(0);
	}

	ConsoleManager.OnCVarChanged();
}

// T: int32, float, FString
template <class T>
class FConsoleVariable : public FConsoleVariableBase
{
public:
	FConsoleVariable(T DefaultValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Data(DefaultValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy)
	{
		if(CanChange(SetBy))
		{
			TTypeFromString<T>::FromString(Data.ShadowedValue[0], InValue);
			OnChanged(SetBy);
		}
	}

	virtual int32 GetInt() const;
	virtual float GetFloat() const;
	virtual FString GetString() const;
	virtual class TConsoleVariableData<int32>* AsVariableInt() { return 0; }
	virtual class TConsoleVariableData<float>* AsVariableFloat() { return 0; }
	virtual class TConsoleVariableData<FString>* AsVariableString() { return 0; }

private: // ----------------------------------------------------

	TConsoleVariableData<T> Data;

	const T &Value() const
	{
		// remove const
		FConsoleVariable<T>* This = (FConsoleVariable<T>*)this;
		return This->Data.GetReferenceOnAnyThread();
	}

	void OnChanged(EConsoleVariableFlags SetBy)
	{
		// propagate from main thread to render thread
		OnCVarChange(Data.ShadowedValue[1], Data.ShadowedValue[0], Flags);
		FConsoleVariableBase::OnChanged(SetBy);
	}
};


// specialization for int32

template<> int32 FConsoleVariable<int32>::GetInt() const
{
	return Value();
}
template<> float FConsoleVariable<int32>::GetFloat() const
{
	return (float)Value();
}
template<> FString FConsoleVariable<int32>::GetString() const
{
	return FString::Printf(TEXT("%d"), Value());
}
template<> TConsoleVariableData<int32>* FConsoleVariable<int32>::AsVariableInt()
{
	return &Data; 
}

// specialization for float

template<> int32 FConsoleVariable<float>::GetInt() const
{
	return (int32)Value();
}
template<> float FConsoleVariable<float>::GetFloat() const
{
	return Value();
}
template<> FString FConsoleVariable<float>::GetString() const
{
	return FString::Printf(TEXT("%g"), Value());
}
template<> TConsoleVariableData<float>* FConsoleVariable<float>::AsVariableFloat()
{
	return &Data; 
}

// specialization for FString

template<> void FConsoleVariable<FString>::Set(const TCHAR* InValue, EConsoleVariableFlags SetBy)
{
	if(CanChange(SetBy))
	{
		Data.ShadowedValue[0] = InValue;
		OnChanged(SetBy);
	}
}
template<> int32 FConsoleVariable<FString>::GetInt() const
{
	return FCString::Atoi(*Value());
}
template<> float FConsoleVariable<FString>::GetFloat() const
{
	return FCString::Atof(*Value());
}
template<> FString FConsoleVariable<FString>::GetString() const
{
	return Value();
}

template<> TConsoleVariableData<FString>* FConsoleVariable<FString>::AsVariableString()
{
	return &Data;
}

// ----

// T: int32, float
template <class T>
class FConsoleVariableRef : public FConsoleVariableBase
{
public:
	FConsoleVariableRef(T& InRefValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), RefValue(InRefValue), MainValue(InRefValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy)
	{
		if(CanChange(SetBy))
		{
			TTypeFromString<T>::FromString(MainValue, InValue);
			OnChanged(SetBy);
		}
	}
	virtual int32 GetInt() const
	{
		return (int32)MainValue;
	}
	virtual float GetFloat() const
	{
		return (float)MainValue;
	}
	virtual FString GetString() const
	{
		return TTypeToString<T>::ToString(MainValue);
	}

private: // ----------------------------------------------------

	// reference the the value (should not be changed from outside), if ECVF_RenderThreadSafe this is the render thread version, otherwise same as MainValue
	T& RefValue;
	//  main thread version 
	T MainValue;
	
	const T &Value() const
	{
		uint32 Index = GetShadowIndex();
		checkSlow(Index < 2);
		return (Index == 0) ? MainValue : RefValue;
	}

	void OnChanged(EConsoleVariableFlags SetBy)
	{
		if(CanChange(SetBy))
		{
			// propagate from main thread to render thread or to reference
			OnCVarChange(RefValue, MainValue, Flags);
			FConsoleVariableBase::OnChanged(SetBy);
		}
	}
};

// specialization for float

template <>
FString FConsoleVariableRef<float>::GetString() const
{
	// otherwise we get 2.1f would become "2.100000"
	return FString::SanitizeFloat(RefValue);
}

// ----

class FConsoleVariableBitRef : public FConsoleVariableBase
{
public:
	FConsoleVariableBitRef(const TCHAR* FlagName, uint32 InBitNumber, uint8* InForce0MaskPtr, uint8* InForce1MaskPtr, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Force0MaskPtr(InForce0MaskPtr), Force1MaskPtr(InForce1MaskPtr), BitNumber(InBitNumber)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy)
	{
		if(CanChange(SetBy))
		{
			int32 Value = FCString::Atoi(InValue);

			check(IsInGameThread());

			FMath::SetBoolInBitField(Force0MaskPtr, BitNumber, Value == 0);
			FMath::SetBoolInBitField(Force1MaskPtr, BitNumber, Value == 1);

			OnChanged(SetBy);
		}
	}
	virtual int32 GetInt() const
	{
		// we apply the bitmask on game thread (showflags) so we don't have to do any special thread handling
		check(IsInGameThread());

		bool Force0 = FMath::ExtractBoolFromBitfield(Force0MaskPtr, BitNumber);
		bool Force1 = FMath::ExtractBoolFromBitfield(Force1MaskPtr, BitNumber);
		
		if(!Force0 && !Force1)
		{
			// not enforced to be 0 or 1
			return 2;
		}

		return Force1 ? 1 : 0;
	}
	virtual float GetFloat() const
	{
		return (float)GetInt();
	}
	virtual FString GetString() const
	{
		return FString::Printf(TEXT("%d"), GetInt());
	}

private: // ----------------------------------------------------

	uint8* Force0MaskPtr;
	uint8* Force1MaskPtr;
	uint32 BitNumber;
};

IConsoleVariable* FConsoleManager::RegisterConsoleVariableBitRef(const TCHAR* CVarName, const TCHAR* FlagName, uint32 BitNumber, uint8* Force0MaskPtr, uint8* Force1MaskPtr, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(CVarName, new FConsoleVariableBitRef(FlagName, BitNumber, Force0MaskPtr, Force1MaskPtr, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

void FConsoleManager::CallAllConsoleVariableSinks()
{
	if(bCallAllConsoleVariableSinks)
	{
		for(uint32 i = 0; i < (uint32)ConsoleVariableChangeSinks.Num(); ++i)
		{
			ConsoleVariableChangeSinks[i].ExecuteIfBound();
		}

		bCallAllConsoleVariableSinks = false;
	}
}

void FConsoleManager::RegisterConsoleVariableSink(const FConsoleCommandDelegate& Command)
{
	ConsoleVariableChangeSinks.Add(Command);
}

void FConsoleManager::UnregisterConsoleVariableSink(const FConsoleCommandDelegate& Command)
{
	ConsoleVariableChangeSinks.Remove(Command);
}

class FConsoleCommand : public FConsoleCommandBase
{

public:
	FConsoleCommand( const FConsoleCommandDelegate& InitDelegate, const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags ),
		  Delegate( InitDelegate )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InWorld, FOutputDevice& OutputDevice ) override
	{
		// NOTE: Args are ignored for FConsoleCommand.  Use FConsoleCommandWithArgs if you need parameters.
		return Delegate.ExecuteIfBound();
	}

private:

	/** User function to call when the console command is executed */
	FConsoleCommandDelegate Delegate;

};


class FConsoleCommandWithArgs : public FConsoleCommandBase
{

public:
	FConsoleCommandWithArgs( const FConsoleCommandWithArgsDelegate& InitDelegate, const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags ),
		  Delegate( InitDelegate )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InWorld, FOutputDevice& OutputDevice ) override
	{
		return Delegate.ExecuteIfBound( Args );
	}

private:

	/** User function to call when the console command is executed */
	FConsoleCommandWithArgsDelegate Delegate;
};

/* Console command that can be given a world parameter. */
class FConsoleCommandWithWorld : public FConsoleCommandBase
{

public:
	FConsoleCommandWithWorld( const FConsoleCommandWithWorldDelegate& InitDelegate, const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags ),
		Delegate( InitDelegate )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InWorld, FOutputDevice& OutputDevice ) override
	{
		return Delegate.ExecuteIfBound( InWorld );
	}

private:

	/** User function to call when the console command is executed */
	FConsoleCommandWithWorldDelegate Delegate;
};

/* Console command that can be given a world parameter and args. */
class FConsoleCommandWithWorldAndArgs : public FConsoleCommandBase
{

public:
	FConsoleCommandWithWorldAndArgs( const FConsoleCommandWithWorldAndArgsDelegate& InitDelegate, const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags ),
		Delegate( InitDelegate )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InWorld, FOutputDevice& OutputDevice ) override
	{
		return Delegate.ExecuteIfBound( Args, InWorld );
	}

private:

	/** User function to call when the console command is executed */
	FConsoleCommandWithWorldAndArgsDelegate Delegate;
};

/* Console command that can be given an output device. */
class FConsoleCommandWithOutputDevice : public FConsoleCommandBase
{

public:
	FConsoleCommandWithOutputDevice( const FConsoleCommandWithOutputDeviceDelegate& InitDelegate, const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags ),
		Delegate( InitDelegate )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InWorld, FOutputDevice& OutputDevice ) override
	{
		return Delegate.ExecuteIfBound( OutputDevice );
	}

private:

	/** User function to call when the console command is executed */
	FConsoleCommandWithOutputDeviceDelegate Delegate;
};

// only needed for auto completion of Exec commands
class FConsoleCommandExec : public FConsoleCommandBase
{

public:
	FConsoleCommandExec( const TCHAR* InitHelp, const EConsoleVariableFlags InitFlags )
		: FConsoleCommandBase( InitHelp, InitFlags )
	{
	}

	// interface IConsoleCommand -----------------------------------

	virtual void Release() override
	{
		delete this; 
	} 

	virtual bool Execute( const TArray< FString > Args, UWorld* InCmdWorld, FOutputDevice& OutputDevice ) override
	{
		return false;
	}
};

IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, int32 DefaultValue, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleVariable<int32>(DefaultValue, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, float DefaultValue, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleVariable<float>(DefaultValue, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, const FString& DefaultValue, const TCHAR* Help, uint32 Flags)
{
	// not supported
	check((Flags & (uint32)ECVF_RenderThreadSafe) == 0);
	return AddConsoleObject(Name, new FConsoleVariable<FString>(DefaultValue, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

IConsoleVariable* FConsoleManager::RegisterConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleVariableRef<int32>(RefValue, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

IConsoleVariable* FConsoleManager::RegisterConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleVariableRef<float>(RefValue, Help, (EConsoleVariableFlags)Flags))->AsVariable();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommand(Command, Help, (EConsoleVariableFlags)Flags))->AsCommand();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommandExec(Help, (EConsoleVariableFlags)Flags))->AsCommand();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommandWithArgs(Command, Help, (EConsoleVariableFlags)Flags))->AsCommand();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommandWithWorld(Command, Help, (EConsoleVariableFlags)Flags))->AsCommand();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommandWithWorldAndArgs(Command, Help, (EConsoleVariableFlags)Flags))->AsCommand();
}

IConsoleCommand* FConsoleManager::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithOutputDeviceDelegate& Command, uint32 Flags)
{
	return AddConsoleObject(Name, new FConsoleCommandWithOutputDevice(Command, Help, (EConsoleVariableFlags)Flags))->AsCommand();
}


IConsoleVariable* FConsoleManager::FindConsoleVariable(const TCHAR* Name) const
{
	IConsoleObject* Obj = FindConsoleObject(Name);

	if(Obj)
	{
		if(Obj->TestFlags(ECVF_Unregistered))
		{
			return 0;
		}

		return Obj->AsVariable();
	}

	return 0;
}

IConsoleObject* FConsoleManager::FindConsoleObject(const TCHAR* Name) const
{
	IConsoleObject* CVar = FindConsoleObjectUnfiltered(Name);

	if(CVar && CVar->TestFlags(ECVF_CreatedFromIni))
	{
		return 0;
	}

	return CVar;
}

IConsoleObject* FConsoleManager::FindConsoleObjectUnfiltered(const TCHAR* Name) const
{
	FScopeLock ScopeLock( &ConsoleObjectsSynchronizationObject );
	IConsoleObject* Var = ConsoleObjects.FindRef(Name);
	return Var;
}

void FConsoleManager::UnregisterConsoleObject(IConsoleObject* CVar, bool bKeepState)
{
	if(!CVar)
	{
		return;
	}

	// Slow search for console object
	const FString ObjName = FindConsoleObjectName( CVar );
	if( !ObjName.IsEmpty() )
	{
		UnregisterConsoleObject(*ObjName, bKeepState);
	}
}


void FConsoleManager::UnregisterConsoleObject(const TCHAR* Name, bool bKeepState)
{
	IConsoleObject* Object = FindConsoleObject(Name);

	if(Object)
	{
		FScopeLock ScopeLock(&ConsoleObjectsSynchronizationObject);

		IConsoleVariable* CVar = Object->AsVariable();

		if(CVar && bKeepState)
		{
			// to be able to restore the value if we just recompile a module
			CVar->SetFlags(ECVF_Unregistered);
		}
		else
		{
			ConsoleObjects.Remove(Name);
			Object->Release();
		}
	}
}

void FConsoleManager::LoadHistoryIfNeeded()
{
	if(bHistoryWasLoaded)
	{
		return;
	}

	bHistoryWasLoaded = true;

	HistoryEntries.Empty();

	FConfigFile Ini;

	FString ConfigPath = FPaths::GeneratedConfigDir() + TEXT("ConsoleHistory.ini");
	ProcessIniContents(*ConfigPath, *ConfigPath, &Ini, false, false, false);

	const FString History = TEXT("History");

	FConfigSection* Section = Ini.Find(TEXT("ConsoleHistory"));

	if(Section)
	{
		for (auto It : *Section)
		{
			const FString& Key = It.Key.ToString();

			if(Key == History)
			{
				HistoryEntries.Add(It.Value);
			}
		}
	}
}

void FConsoleManager::SaveHistory()
{
	const FName History = TEXT("History");

	FConfigFile Ini;

	FString ConfigPath = FPaths::GeneratedConfigDir() + TEXT("ConsoleHistory.ini");

	FConfigSection& Section = Ini.Add(TEXT("ConsoleHistory"));

	for(auto It : HistoryEntries)
	{
		Section.Add(History, It);
	}

	Ini.Dirty = true;
	Ini.Write(ConfigPath);
}


void FConsoleManager::ForEachConsoleObject(const FConsoleObjectVisitor& Visitor, const TCHAR* ThatStartsWith) const
{
	check(Visitor.IsBound());
	check(ThatStartsWith);

	//@caution, potential deadlock if the visitor tries to call back into the cvar system. Best not to do this, but we could capture and array of them, then release the lock, then dispatch the visitor.
	FScopeLock ScopeLock( &ConsoleObjectsSynchronizationObject );
	for(TMap<FString, IConsoleObject*>::TConstIterator PairIt(ConsoleObjects); PairIt; ++PairIt)
	{
		const FString& Name = PairIt.Key();
		IConsoleObject* CVar = PairIt.Value();

		if(MatchPartialName(*Name, ThatStartsWith))
		{
			Visitor.Execute(*Name, CVar);
		}
	}
}

bool FConsoleManager::ProcessUserConsoleInput(const TCHAR* InInput, FOutputDevice& Ar, UWorld* InWorld)
{
	check(InInput);

	const TCHAR* It = InInput;

	FString Param1 = GetTextSection(It);
	if(Param1.IsEmpty())
	{
		return false;
	}

	IConsoleObject* CObj = FindConsoleObject(*Param1);
	if(!CObj)
	{
		return false;
	}

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(CObj->TestFlags(ECVF_Cheat))
	{
		return false;
	}
#endif // (UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if(CObj->TestFlags(ECVF_Unregistered))
	{
		return false;
	}

	// fix case for nicer printout
	Param1 = FindConsoleObjectName(CObj);

	IConsoleCommand* CCmd = CObj->AsCommand();
	IConsoleVariable* CVar = CObj->AsVariable();

	if( CCmd )
	{
		// Process command
		// Build up argument list
		TArray< FString > Args;
		FString( It ).ParseIntoArrayWS( &Args );

		const bool bShowHelp = Args.Num() == 1 && Args[0] == TEXT("?");
		if( bShowHelp )
		{
			// get help
			Ar.Logf(TEXT("HELP for '%s':\n%s"), *Param1, CCmd->GetHelp());
		}
		else
		{
			// if a delegate was bound, we execute it and it should return true,
			// otherwise it was a Exec console command and this returns FASLE
			return CCmd->Execute( Args, InWorld, Ar );
		}
	}
	else if( CVar )
	{
		// Process variable

		bool bShowCurrentState = false;

		if(*It == 0)
		{
			bShowCurrentState = true;
		}
		else
		{
			FString Param2 = FString(It).Trim().TrimTrailing();

			if(Param2.Len() >= 2)
			{
				if(Param2[0] == (TCHAR)'\"' && Param2[Param2.Len() - 1] == (TCHAR)'\"')
				{
					Param2 = Param2.Mid(1, Param2.Len() - 2);
				}
			}

			bool bReadOnly = CVar->TestFlags(ECVF_ReadOnly);

			if(Param2 == TEXT("?"))
			{
				// get help
				Ar.Logf(TEXT("HELP for '%s'%s:\n%s"), *Param1, bReadOnly ? TEXT("(ReadOnly)") : TEXT(""), CVar->GetHelp());
				bShowCurrentState = true;
			}
			else
			{
				if(bReadOnly)
				{
					Ar.Logf(TEXT("Error: %s is read only!"), *Param1, *CVar->GetString());
				}
				else
				{
					// set value
					CVar->Set(*Param2, ECVF_SetByConsole);

					Ar.Logf(TEXT("%s = \"%s\""), *Param1, *CVar->GetString());

					CallAllConsoleVariableSinks();
				}
			}
		}

		if(bShowCurrentState)
		{
			Ar.Logf(TEXT("%s = \"%s\"      LastSetBy: %s"), *Param1, *CVar->GetString(), GetSetByTCHAR(CVar->GetFlags()));
		}
	}

	return true;
}

IConsoleObject* FConsoleManager::AddConsoleObject(const TCHAR* Name, IConsoleObject* Obj)
{
	check(Name);
	check(*Name != 0);
	check(Obj);

	FScopeLock ScopeLock( &ConsoleObjectsSynchronizationObject ); // we will lock on the entire add process
	IConsoleObject* ExistingObj = ConsoleObjects.FindRef(Name);

	if(Obj->GetFlags() & ECVF_Scalability)
	{
		// Scalability options cannot be cheats - otherwise using the options menu would mean cheating
		check(!(Obj->GetFlags() & ECVF_Cheat));
		// Scalability options cannot be read only - otherwise the options menu cannot work 
		check(!(Obj->GetFlags() & ECVF_ReadOnly));
	}

	if(Obj->GetFlags() & ECVF_RenderThreadSafe)
	{
		if(Obj->AsCommand())
		{
			// This feature is not supported for console commands
			check(0);
		}
	}

	if(ExistingObj)
	{
		// An existing console object was found that has the same name as the object being registered.
		// In most cases this is not allowed, but if there is a variable with the same name and is
		// in an 'unregistered' state or we're hot reloading dlls, we may be able to replace or update that variable.
#if WITH_HOT_RELOAD
		const bool bCanUpdateOrReplaceObj = (ExistingObj->AsVariable()||ExistingObj->AsCommand()) && (GIsHotReload || ExistingObj->TestFlags(ECVF_Unregistered));
#else
		const bool bCanUpdateOrReplaceObj = ExistingObj->AsVariable() && ExistingObj->TestFlags(ECVF_Unregistered);
#endif
		if( !bCanUpdateOrReplaceObj )
		{
			// NOTE: The reason we don't assert here is because when using HotReload, locally-initialized static console variables will be
			//       re-registered, and it's desirable for the new variables to clobber the old ones.  Because this happen outside of the
			//       hot reload stack frame (GIsHotReload=true), we can't detect and handle only those cases, so we opt to warn instead.
			UE_LOG(LogConsoleManager, Warning, TEXT( "Console object named '%s' already exists but is being registered again, but we weren't expected it to be! (FConsoleManager::AddConsoleObject)"), Name );
		}

		IConsoleVariable* ExistingVar = ExistingObj->AsVariable();
		IConsoleCommand* ExistingCmd = ExistingObj->AsCommand();
		const int ExistingType = ExistingVar ? ExistingCmd ? 3 : 2 : 1;

		IConsoleVariable* Var = Obj->AsVariable();
		IConsoleCommand* Cmd = Obj->AsCommand();
		const int NewType = Var ? Cmd ? 3 : 2 : 1;

		// Validate that we have the same type for the existing console object and for the new one, because
		// never allowed to replace a command with a variable or vice-versa
		if( ExistingType != NewType )
		{
			UE_LOG(LogConsoleManager, Fatal, TEXT( "Console object named '%s' can't be replaced with the new one of different type!"), Name );
		}

		if( ExistingVar && Var )
		{
			if(ExistingVar->TestFlags(ECVF_CreatedFromIni))
			{
				// The existing one came from the ini, get the value and destroy the existing one (no need to call sink because that will happen after all ini setting have been loaded)
				Var->Set(*ExistingVar->GetString(), (EConsoleVariableFlags)((uint32)ExistingVar->GetFlags() & ECVF_SetByMask));
				ExistingVar->Release();

				ConsoleObjects.Add(Name, Var);
				return Var;
			}
			else
			{
				// Copy data over from the new variable,
				// but keep the value from the existing one.
				// This way references to the old variable are preserved (no crash).
				// Changing the type of a variable however is not possible with this.
				ExistingVar->SetFlags(Var->GetFlags());
				ExistingVar->SetHelp(Var->GetHelp());

				// Name was already registered but got unregistered
				Var->Release();

				return ExistingVar;
			}
		}
		else if( ExistingCmd )
		{
			// Replace console command with the new one and release the existing one.
			// This should be safe, because we don't have FindConsoleVariable equivalent for commands.
			ConsoleObjects.Add( Name, Cmd );
			ExistingCmd->Release();

			return Cmd;
		}

		// Should never happen
		return nullptr;
	}
	else
	{
		ConsoleObjects.Add(Name, Obj);
		return Obj;
	}
}

FString FConsoleManager::GetTextSection(const TCHAR* &It)
{
	FString ret;

	while(*It)
	{
		if(IsWhiteSpace(*It))
		{
			break;
		}

		ret += *It++;
	}

	while(IsWhiteSpace(*It))
	{
		++It;
	}

	return ret;
}

FString FConsoleManager::FindConsoleObjectName(const IConsoleObject* InVar) const
{
	check(InVar);

	FScopeLock ScopeLock( &ConsoleObjectsSynchronizationObject );
	for(TMap<FString, IConsoleObject*>::TConstIterator PairIt(ConsoleObjects); PairIt; ++PairIt)
	{
		IConsoleObject* Var = PairIt.Value();

		if(Var == InVar)
		{
			const FString& Name = PairIt.Key();

			return Name;
		}
	}

	return FString();
}

bool FConsoleManager::MatchPartialName(const TCHAR* Stream, const TCHAR* Pattern)
{
	while(*Pattern)
	{
		if(FChar::ToLower(*Stream) != FChar::ToLower(*Pattern))
		{
			return false;
		}

		++Stream;
		++Pattern;
	}

	return true;
}

void CreateConsoleVariables();

IConsoleManager* IConsoleManager::Singleton;

void IConsoleManager::SetupSingleton()
{
	check(!Singleton);
	if (!Singleton)
	{
		Singleton = new FConsoleManager; // we will leak this
		CreateConsoleVariables();
	}
	check(Singleton);
}

void FConsoleManager::AddConsoleHistoryEntry(const TCHAR* Input)
{
	LoadHistoryIfNeeded();

	// limit size to avoid a ever growing file
	while(HistoryEntries.Num() > 64)
	{
		HistoryEntries.RemoveAt(0);
	}

	HistoryEntries.Add(FString(Input));

	SaveHistory();
}

void FConsoleManager::GetConsoleHistory(TArray<FString>& Out)
{
	LoadHistoryIfNeeded();

	Out = HistoryEntries;
}

bool FConsoleManager::IsNameRegistered(const TCHAR* Name) const
{
	FScopeLock ScopeLock(&ConsoleObjectsSynchronizationObject);
	return ConsoleObjects.Contains(Name);
}

void FConsoleManager::RegisterThreadPropagation(uint32 ThreadId, IConsoleThreadPropagation* InCallback)
{
	if(InCallback)
	{
		// at the moment we only support one thread besides the main thread
		check(!ThreadPropagationCallback);
	}
	else
	{
		// bad input parameters
		check(!ThreadId);
	}

	ThreadPropagationCallback = InCallback;
	ThreadPropagationThreadId = ThreadId;
}

IConsoleThreadPropagation* FConsoleManager::GetThreadPropagationCallback()
{
	return ThreadPropagationCallback;
}

bool FConsoleManager::IsThreadPropagationThread()
{
	return FPlatformTLS::GetCurrentThreadId() == ThreadPropagationThreadId;
}

void FConsoleManager::OnCVarChanged()
{
	bCallAllConsoleVariableSinks = true;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
uint32 GConsoleManagerSinkTestCounter = 0;
void TestSinkCallback()
{
	++GConsoleManagerSinkTestCounter;
}
uint32 GConsoleVariableCallbackTestCounter = 0;
void TestConsoleVariableCallback(IConsoleVariable* Var)
{
	check(Var);

	float Value = Var->GetFloat();
	check(FMath::IsNearlyEqual(Value, 3.1f, KINDA_SMALL_NUMBER));

	++GConsoleVariableCallbackTestCounter;
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void FConsoleManager::Test()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(IsInGameThread());

	// at this time we don't want to test with threading
	check(GetThreadPropagationCallback() == 0);

	// init ---------

	GConsoleManagerSinkTestCounter = 0;
	IConsoleManager::Get().CallAllConsoleVariableSinks();

	// setup ---------

	RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&TestSinkCallback));

	// start tests ---------

	// no change should be triggered
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	check(GConsoleManagerSinkTestCounter == 0);

	for(uint32 Pass = 0; Pass < 2; ++Pass)
	{
		// we only test the main thread side of ECVF_RenderThreadSafe so we expect the same results
		uint32 Flags = Pass ? ECVF_Default : ECVF_RenderThreadSafe;

		int32 RefD = 2;
		float RefE = 2.1f;

		IConsoleVariable* VarA = IConsoleManager::Get().RegisterConsoleVariable(TEXT("TestNameA"), 1, TEXT("TestHelpA"), Flags);
		IConsoleVariable* VarB = IConsoleManager::Get().RegisterConsoleVariable(TEXT("TestNameB"), 1.2f, TEXT("TestHelpB"), Flags);
		IConsoleVariable* VarD = IConsoleManager::Get().RegisterConsoleVariableRef(TEXT("TestNameD"), RefD, TEXT("TestHelpD"), Flags);
		IConsoleVariable* VarE = IConsoleManager::Get().RegisterConsoleVariableRef(TEXT("TestNameE"), RefE, TEXT("TestHelpE"), Flags);

		// at the moment ECVF_SetByConstructor has to be 0 or we set ECVF_Default to ECVF_SetByConstructor
		check((VarA->GetFlags() & ECVF_SetByMask) == ECVF_SetByConstructor);

		GConsoleVariableCallbackTestCounter = 0;
		VarB->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&TestConsoleVariableCallback));
		check(GConsoleVariableCallbackTestCounter == 0);

		// make sure the vars are there

		check(VarA == IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameA")));
		check(VarB == IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameB")));
		check(VarD == IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameD")));
		check(VarE == IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameE")));

		// test Get()

		check(VarA->GetInt() == 1);
		check(VarA->GetFloat() == 1);
		check(VarA->GetString() == FString(TEXT("1")));

		check(VarB->GetInt() == 1);
		check(FMath::IsNearlyEqual(VarB->GetFloat(), 1.2f, KINDA_SMALL_NUMBER));
		check(VarB->GetString() == FString(TEXT("1.2")));

		check(RefD == 2);
		check(VarD->GetInt() == 2);
		check(VarD->GetFloat() == (float)2);
		check(VarD->GetString() == FString(TEXT("2")));

		check(FMath::IsNearlyEqual(RefE, 2.1f, KINDA_SMALL_NUMBER));
		check(VarE->GetInt() == (int32)RefE);
		check(VarE->GetFloat() == RefE);
		check(VarE->GetString() == FString(TEXT("2.1")));

		// test changed

		check(!VarA->TestFlags(ECVF_Changed));
		check(!VarB->TestFlags(ECVF_Changed));
		check(!VarD->TestFlags(ECVF_Changed));
		check(!VarE->TestFlags(ECVF_Changed));

		// call Set(string)

		VarA->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);
		VarB->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);
		VarD->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);
		VarE->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);

		check(GConsoleVariableCallbackTestCounter == 1);

		// test changed

		check(VarA->TestFlags(ECVF_Changed));
		check(VarB->TestFlags(ECVF_Changed));
		check(VarD->TestFlags(ECVF_Changed));
		check(VarE->TestFlags(ECVF_Changed));

		// verify Set()

		check(VarA->GetString() == FString(TEXT("3")));
		check(VarB->GetString() == FString(TEXT("3.1")));
		check(VarD->GetString() == FString(TEXT("3")));
		check(VarE->GetString() == FString(TEXT("3.1")));
		check(RefD == 3);
		check(RefE == 3.1f);

		VarB->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);
		check(GConsoleVariableCallbackTestCounter == 2);

		// unregister

		IConsoleManager::Get().UnregisterConsoleObject(VarA);
		IConsoleManager::Get().UnregisterConsoleObject(VarB, false);
		UnregisterConsoleObject(TEXT("TestNameD"), false);
		UnregisterConsoleObject(TEXT("TestNameE"), false);

		check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameA")));
		check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameB")));
		check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameD")));
		check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameE")));

		// re-register but maintain state
		IConsoleVariable* SecondVarA = IConsoleManager::Get().RegisterConsoleVariable(TEXT("TestNameA"), 1234, TEXT("TestHelpSecondA"), Flags);
		check(SecondVarA == VarA);
		check(SecondVarA->GetInt() == 3);
		check(IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameA")));

		UnregisterConsoleObject(TEXT("TestNameA"), false);
		check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameA")));

		if((Flags & ECVF_RenderThreadSafe) == 0)
		{
			// string is not supported with the flag ECVF_RenderThreadSafe
			IConsoleVariable* VarC = IConsoleManager::Get().RegisterConsoleVariable(TEXT("TestNameC"), TEXT("1.23"), TEXT("TestHelpC"), Flags);
			check(VarC == IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameC")));
			check(VarC->GetInt() == 1);
			// note: exact comparison fails in Win32 release
			check(FMath::IsNearlyEqual(VarC->GetFloat(), 1.23f, KINDA_SMALL_NUMBER));
			check(VarC->GetString() == FString(TEXT("1.23")));
			check(!VarC->TestFlags(ECVF_Changed));
			VarC->Set(TEXT("3.1"), ECVF_SetByConsole);
			check(VarC->TestFlags(ECVF_Changed));
			check(VarC->GetString() == FString(TEXT("3.1")));
			UnregisterConsoleObject(TEXT("TestNameC"), false);
			check(!IConsoleManager::Get().FindConsoleVariable(TEXT("TestNameC")));
		}

		// verify priority
		{
			IConsoleVariable* VarX = IConsoleManager::Get().RegisterConsoleVariable(TEXT("TestNameX"), 1, TEXT("TestHelpX"), Flags);
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByConstructor);

			VarX->Set(TEXT("3.1"), ECVF_SetByConsoleVariablesIni);
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsoleVariablesIni);

			// lower should fail
			VarX->Set(TEXT("111"), ECVF_SetByScalability);
			check(VarX->GetString() == FString(TEXT("3")));
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsoleVariablesIni);

			// higher should work
			VarX->Set(TEXT("222"), ECVF_SetByCommandline);
			check(VarX->GetString() == FString(TEXT("222")));
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByCommandline);

			// lower should fail
			VarX->Set(TEXT("333"), ECVF_SetByConsoleVariablesIni);
			check(VarX->GetString() == FString(TEXT("222")));
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByCommandline);

			// higher should work
			VarX->Set(TEXT("444"), ECVF_SetByConsole);
			check(VarX->GetString() == FString(TEXT("444")));
			check(((uint32)VarX->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsole);

			IConsoleManager::Get().UnregisterConsoleObject(VarX, false);
		}
	}

	// this should trigger the callback
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	check(GConsoleManagerSinkTestCounter == 1);

	// this should not trigger the callback
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	check(GConsoleManagerSinkTestCounter == 1);

	// this should also trigger the callback
	TestSinkCallback();
	check(GConsoleManagerSinkTestCounter == 2);

	UnregisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&TestSinkCallback));

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

// These don't belong here, but they belong here more than they belong in launch engine loop.
void CreateConsoleVariables()
{

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// the following commands are common exec commands that should be added to auto completion (todo: read UnConsole list in ini, discover all exec commands)
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("VisualizeTexture"),	TEXT("To visualize internal textures"), ECVF_Cheat);
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("Vis"),	TEXT("short version of visualizetexture"), ECVF_Cheat);
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("VisRT"),	TEXT("GUI for visualizetexture"), ECVF_Cheat);
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("HighResShot"),	TEXT("High resolution screenshots [Magnification = 2..]"), ECVF_Cheat);
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("DumpConsoleCommands"),	TEXT("Dumps all console vaiables and commands and all exec that can be discovered to the log/console"), ECVF_Default);
	IConsoleManager::Get().RegisterConsoleCommand(TEXT("DumpUnbuiltLightInteractions"),	TEXT("Logs all lights and primitives that have an unbuilt interaction."), ECVF_Cheat);

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// testing code
	{
		FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();

		ConsoleManager.Test();
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void DumpHelp(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Console Help:"));
	Ar.Logf(TEXT("============="));
	Ar.Logf(TEXT(" "));
	Ar.Logf(TEXT("A console variable is a engine wide key value pair. The key is a string usually starting with the subsystem prefix followed"));
	Ar.Logf(TEXT("by '.' e.g. r.BloomQuality. The value can be of different tpe (e.g. float, int, string). A console command has no state associated with"));
	Ar.Logf(TEXT("and gets executed immediately."));
	Ar.Logf(TEXT(" "));
	Ar.Logf(TEXT("Console variables can be put into ini files (e.g. ConsoleVariables.ini or BaseEngine.ini) with this syntax:"));
	Ar.Logf(TEXT("<Console variable> = <value>"));
	Ar.Logf(TEXT(" "));
	Ar.Logf(TEXT("DumpConsoleCommands         Lists all console variables and commands that are registered (Some are not registered)"));
	Ar.Logf(TEXT("<Console variable>          Get the console variable state"));
	Ar.Logf(TEXT("<Console variable> ?        Get the console variable help text"));
	Ar.Logf(TEXT("<Console variable> <value>  Set the console variable value"));
	Ar.Logf(TEXT("<Console command> [Params]  Execute the console command with optional parameters"));
}
static FAutoConsoleCommandWithOutputDevice GConsoleCommandHelp(
	TEXT("help"),
	TEXT("Outputs some helptext to the console and the log"),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic(DumpHelp)
	);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

// Naming conventions:
//
// Console variable should start with (suggestion):
//
// r.      Renderer / 3D Engine / graphical feature
// RHI.    Low level RHI (rendering platform) specific
// a.	   Animation
// s. 	   Sound / Music
// n.      Network
// ai.     Artificial intelligence
// i.      Input e.g. mouse/keyboard
// p.      Physics
// t.      Timer
// g.      Game
// Compat.
// FX.     Particle effects
// sg.     scalability group (used by scalability system, ini load/save or using SCALABILITY console command)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarPreViewTranslation(
	TEXT("r.PreViewTranslation"),
	1,
	TEXT("To limit issues with float world space positions we offset the world by the\n")
	TEXT("PreViewTranslation vector. This command allows to disable updating this vector.\n")
	TEXT(" 0: disable update\n")
	TEXT(" 1: update the offset is each frame (default)"),
	ECVF_Cheat);

static TAutoConsoleVariable<FString> CVarFreezeAtPosition(
	TEXT("FreezeAtPosition"),
	TEXT(""),	// default value is empty
	TEXT("This console variable stores the position and rotation for the FreezeAt command which allows\n")
	TEXT("to lock the camera in order to provide more deterministic render profiling.\n")
	TEXT("The FreezeAtPosition can be set in the ConsoleVariables.ini (start the map with MAPNAME?bTourist=1).\n")
	TEXT("Also see the FreezeAt command console command.\n")
	TEXT("The number syntax if the same as the one used by the BugIt command:\n")
	TEXT(" The first three values define the position, the next three define the rotation.\n")
	TEXT("Example:\n")
	TEXT(" FreezeAtPosition 2819.5520 416.2633 75.1500 65378 -25879 0"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarLimitRenderingFeatures(
	TEXT("r.LimitRenderingFeatures"),
	0,
	TEXT("Allows to quickly reduce render feature to increase render performance.\n")
	TEXT("This is just a quick way to alter multiple show flags and console variables in the game\n")
	TEXT("Disabled more feature the higher the number\n")
	TEXT(" <=0:off, order is defined in code (can be documented here when we settled on an order)"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDumpingMovie(
	TEXT("r.DumpingMovie"),
	GIsDumpingMovie,
	TEXT("Allows to dump each rendered frame to disk (slow fps, names MovieFrame..).\n")
	TEXT("<=0:off (default), <0:remains on, >0:remains on for n frames (n is the number specified)"),
	ECVF_Cheat);

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static TAutoConsoleVariable<int32> CVarUniformBufferPooling(
	TEXT("r.UniformBufferPooling"),
	1,
	TEXT("If we pool object in RHICreateUniformBuffer to have less real API calls to creat buffers\n")
	TEXT(" 0: off (for debugging)\n")
	TEXT(" 1: on (optimization)"),
	ECVF_RenderThreadSafe);

// The following console variable should never be compiled out ------------------
static TAutoConsoleVariable<int32> CVarClearWithExcludeRects(
	TEXT("r.ClearWithExcludeRects"),
	2,
	TEXT("Control the use of exclude rects when using RHIClear\n")
	TEXT(" 0: Force off (can be faster on hardware that has fast clears)\n")
	TEXT(" 1: Use exclude rect if supplied\n")
	TEXT(" 2: Auto (default is 2, pick what is considered best on this hardware)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSimpleDynamicLighting(
	TEXT("r.SimpleDynamicLighting"),
	0,
	TEXT("Whether to use simple dynamic lighting, which just renders an unshadowed dynamic directional light and a skylight.\n")
	TEXT("All other lighting features are disabled when true.  This is useful for supporting very low end hardware.\n")
	TEXT("0:off, 1:on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTranslucentSortPolicy(
	TEXT("r.TranslucentSortPolicy"),
	0,
	TEXT("0: Sort based on distance from camera centerpoint to bounding sphere centerpoint. (default, best for 3D games)\n")
	TEXT("1: Sort based on projected distance to camera.")
	TEXT("2: Sort based on the projection onto a fixed axis. (best for 2D games)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileHDR(
	TEXT("r.MobileHDR"),
	1,
	TEXT("0: Mobile renders in LDR gamma space. (suggested for unlit games targeting low-end phones)\n")
	TEXT("1: Mobile renders in HDR linear space. (default)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarMobileHDR32bpp(
	TEXT("r.MobileHDR32bpp"),
	0,
	TEXT("0: Mobile HDR renders to an FP16 render target.\n")
	TEXT("1: Mobile HDR renders to an RGBA8 target."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSetClearSceneMethod(
	TEXT("r.ClearSceneMethod"),
	1,
	TEXT("Select how scene rendertarget clears are handled\n")
	TEXT(" 0: No clear\n")
	TEXT(" 1: RHIClear (default)\n")
	TEXT(" 2: Quad at max z"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLensFlareQuality(
	TEXT("r.LensFlareQuality"),
	2,
	TEXT(" 0: off but best for performance\n")
	TEXT(" 1: low quality with good performance\n")
	TEXT(" 2: good quality (default)\n")
	TEXT(" 3: very good quality but bad performance"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarBloomQuality(
	TEXT("r.BloomQuality"),
	5,
	TEXT(" 0: off, no performance impact.\n")
	TEXT(" 1: average quality, least performance impact.\n")
	TEXT(" 2: average quality, least performance impact.\n")
	TEXT(" 3: good quality.\n")
	TEXT(" 4: good quality.\n")
	TEXT(" 5: Best quality, most significant performance impact. (default)\n")
	TEXT(">5: force experimental higher quality on mobile (can be quite slow on some hardware)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSceneColorFringeQuality(
	TEXT("r.SceneColorFringeQuality"),
	1,
	TEXT(" 0: off but best for performance\n")
	TEXT(" 1: 3 texture samples (default)n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);


// ---------------------------------------

static TAutoConsoleVariable<int32> CVarAmbientOcclusionLevels(
	TEXT("r.AmbientOcclusionLevels"),
	3,
	TEXT("Defines how many mip levels are using during the ambient occlusion calculation. This is useful when tweaking the algorithm.\n")
	TEXT(" 0:none, 1:one, 2:two, 3:three(default), 4:four(larger at little cost but can flicker)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarAmbientOcclusionRadiusScale(
	TEXT("r.AmbientOcclusionRadiusScale"),
	1.0f,
	TEXT("Allows to scale the ambient occlusion radius (SSAO).\n")
	TEXT(" 0:off, 1.0:normal, <1:smaller, >1:larger"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarAmbientOcclusionStaticFraction(TEXT("r.AmbientOcclusionStaticFraction"),
	-1.0f,
	TEXT("Allows to override the Ambient Occlusion Static Fraction (see post process volume). Fractions are between 0 and 1.\n")
	TEXT("<0: use default setting (default -1)\n")
	TEXT(" 0: no effect on static lighting, 0 is free meaning no extra rendering pass\n")
	TEXT(" 1: AO affects the stat lighting"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShadowQuality(
	TEXT("r.ShadowQuality"),
	5,
	TEXT("Defines the shadow method which allows to adjust for quality or performance.\n")
	TEXT(" 0:off, 1:low(unfiltered), 2:low .. 5:max (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMotionBlurQuality(
	TEXT("r.MotionBlurQuality"),
	4,
	TEXT("Defines the motion blur method which allows to adjust for quality or performance.\n")
	TEXT(" 0:off, 1:low, 2:medium, 3:high (default), 4: very high"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPostProcessAAQuality(
	TEXT("r.PostProcessAAQuality"),
	4,
	TEXT("Defines the postprocess anti aliasing method which allows to adjust for quality or performance.\n")
	TEXT(" 0:off, 1:very low (faster FXAA), 2:low (FXAA), 3:medium (faster TemporalAA), 4:high (default TemporalAA), 5:very high, 6:max"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarFullscreenMode(
	TEXT("r.FullScreenMode"),
	1,
	TEXT("Defines how we do full screen when requested (e.g. command line option -fullscreen or in ini [SystemSettings] fullscreen=true)\n")
	TEXT(" 0: normal full screen (renders faster, more control over vsync, less GPU memory, 10bit color if possible)\n")
	TEXT(" 1: windowed full screen (quick switch between applications and window mode, slight performance loss)\n")
	TEXT(" any other number behaves like 0"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarSceneColorFormat(
	TEXT("r.SceneColorFormat"),
	4,
	TEXT("Defines the memory layout (RGBA) used for the scene color\n")
	TEXT("(affects performance, mostly through bandwidth, quality especially with translucency).\n")
	TEXT(" 0: PF_B8G8R8A8 32Bit (mostly for testing, likely to unusable with HDR)\n")
	TEXT(" 1: PF_A2B10G10R10 32Bit\n")
	TEXT(" 2: PF_FloatR11G11B10 32Bit\n")
	TEXT(" 3: PF_FloatRGB 32Bit\n")
	TEXT(" 4: PF_FloatRGBA 64Bit (default, might be overkill, especially if translucency is mostly using SeparateTranslucency)\n")
	TEXT(" 5: PF_A32B32G32R32F 128Bit (unreasonable but good for testing)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDepthOfFieldQuality(
	TEXT("r.DepthOfFieldQuality"),
	2,
	TEXT("Allows to adjust the depth of field quality. Currently only fully affects BokehDOF. GaussianDOF is either 0 for off, otherwise on.\n")
	TEXT(" 0: Off\n")
	TEXT(" 1: Low\n")
	TEXT(" 2: high quality (default, adaptive, can be 4x slower)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarScreenPercentage(
	TEXT("r.ScreenPercentage"),
	-1.0f,
	TEXT("To render in lower resolution and upscale for better performance.\n")
	TEXT("70 is a good value for low aliasing and performance, can be verified with 'show TestImage'\n")
	TEXT("in percent, >0 and <=100, <0 means the post process volume settings are used"),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<int32> CVarMaterialQualityLevel(
	TEXT("r.MaterialQualityLevel"),
	1,
	TEXT("0 corresponds to low quality materials, as defined by quality switches in materials, 1 corresponds to high."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarUseDXT5NormalMaps(
	TEXT("Compat.UseDXT5NormalMaps"),
	0,
	TEXT("Whether to use DXT5 for normal maps, otherwise BC5 will be used, which is not supported on all hardware.\n")
	TEXT("Both formats require the same amount of memory (if driver doesn't emulate the format).\n")
	TEXT("Changing this will cause normal maps to be recompressed on next load (or when using recompile shaders)\n")
	TEXT(" 0: Use BC5 texture format (default)\n")
	TEXT(" 1: Use DXT5 texture format (lower quality)"),
	// 
	// Changing this causes a full shader recompile
	ECVF_ReadOnly);

// Changing this is currently unsupported after content has been chunked with the previous setting
// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarMaxGPUSkinBones(
	TEXT("Compat.MAX_GPUSKIN_BONES"),
	256,
	TEXT("Max number of bones that can be skinned on the GPU in a single draw call. Cannot be changed at runtime."),
	ECVF_ReadOnly);

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarAllowStaticLighting(
	TEXT("r.AllowStaticLighting"),
	1,
	TEXT("Whether to allow any static lighting to be generated and used, like lightmaps and shadowmaps.\n")
	TEXT("Games that only use dynamic lighting should set this to 0 to save some static lighting overhead."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNormalMaps(
	TEXT("r.NormalMapsForStaticLighting"),
	0,
	TEXT("Whether to allow any static lighting to use normal maps for lighting computations."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNumBufferedOcclusionQueries(
	TEXT("r.NumBufferedOcclusionQueries"),
	1,
	TEXT("Number of frames to buffer occlusion queries (including the current renderthread frame).\n")
	TEXT("More frames reduces the chance of stalling the CPU waiting for results, but increases out of date query artifacts."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDistField(
	TEXT("r.GenerateMeshDistanceFields"),
	0,	
	TEXT("Whether to build distance fields of static meshes, needed for distance field AO, which is used to implement Movable SkyLight shadows.\n")
	TEXT("Enabling will increase mesh build times and memory usage.  Changing this value will cause a rebuild of all static meshes."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarMinLogVerbosity(
	TEXT("con.MinLogVerbosity"),
	0,
	TEXT("Allows to see the log in the in game console (by default deactivated to avoid spam and minor performance loss).\n")
	TEXT(" 0: no logging other than console response (default)\n")
	TEXT(" 1: Only fatal errors (no that useful)\n")
	TEXT(" 2: additionally errors\n")
	TEXT(" 3: additionally warnings\n")
	TEXT(" 4: additionally display\n")
	TEXT(" 5: additionally log\n")
	TEXT("..\n")
	TEXT(">=7: all"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarMSAACompositingSampleCount(
	TEXT("r.MSAA.CompositingSampleCount"),
	4,
	TEXT("Affects the render quality of the editor 3d objects.\n")
	TEXT(" 1: no MSAA, lowest quality\n")
	TEXT(" 2: 2x MSAA, medium quality (medium GPU memory consumption)\n")
	TEXT(" 4: 4x MSAA, high quality (high GPU memory consumption)\n")
	TEXT(" 8: 8x MSAA, very high quality (insane GPU memory consumption)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<FString> CVarNetPackageMapDebugObject(
	TEXT("net.PackageMap.DebugObject"),
	TEXT(""),
	TEXT("Debugs PackageMap serialization of object")
	TEXT("Partial name of object to debug"),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarNetReplicationDebugProperty(
	TEXT("net.Replication.DebugProperty"),
	TEXT(""),
	TEXT("Debugs Replication of property by name")
	TEXT("Partial name of property to debug"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetRPCDebug(
	TEXT("net.RPC.Debug"),
	0,
	TEXT("Print all RPC bunches sent over the network\n")
	TEXT(" 0: no print.\n")
	TEXT(" 1: Print bunches as they are sent."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetMontageDebug(
	TEXT("net.Montage.Debug"),
	0,
	TEXT("Prints Replication information about AnimMontages\n")
	TEXT(" 0: no print.\n")
	TEXT(" 1: Print AnimMontage info on client side as they are played."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarRenderTargetPoolMin(
	TEXT("r.RenderTargetPoolMin"),
	400,
	TEXT("If the render target pool size (in MB) is below this number there is no deallocation of rendertargets")
	TEXT("Default is 200 MB."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowDistanceFieldPenumbraSize(
	TEXT("r.Shadow.DistanceFieldPenumbraSize"),
	0.05f,
	TEXT("Controls the size of the uniform penumbra produced by static shadowing.\n")
	TEXT("This is a fraction of MaxTransitionDistanceWorldSpace in BaseLightmass.ini."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarIdleWhenNotForeground(
	TEXT("t.IdleWhenNotForeground"), 0,
	TEXT("Prevents the engine from taking any CPU or GPU time while not the foreground app."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarSetVSyncEnabled(
	TEXT("r.VSync"),
	0,
	TEXT("0: VSync is disabled.(default)\n")
	TEXT("1: VSync is enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarFinishCurrentFrame(
	TEXT("r.FinishCurrentFrame"),
	0,
	TEXT("If on, the current frame will be forced to finish and render to the screen instead of being buffered.  This will improve latency, but slow down overall performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMaxAnistropy(
	TEXT("r.MaxAnisotropy"),
	4,
	TEXT("MaxAnisotropy should range from 1 to 16. Higher values mean better texure quality when using anisotropic filtering but at a cost to performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowMaxResolution(
	TEXT("r.Shadow.MaxResolution"),
	2048,
	TEXT("Max square dimensions (in texels) allowed for rendering shadow depths. Range 4 to hardware limit. Higher = better quality shadows but at a performance cost."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowCSMTransitionScale(
	TEXT("r.Shadow.CSM.TransitionScale"),
	1.0f,
	TEXT("Allows to scale the cascaded shadow map transition region. Clamped within 0..2.\n")
	TEXT("0: no transition (fastest)\n")
	TEXT("1: as specific in the light settings (default)\n")
	TEXT("2: 2x larger than what was specified in the light"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMobileContentScaleFactor(
	TEXT("r.MobileContentScaleFactor"),
	1.0f,
	TEXT("Content scale multiplier (equates to iOS's contentScaleFactor to support Retina displays"),
	ECVF_Default);

// this cvar can be removed in shipping to not compile shaders for development (faster)
static TAutoConsoleVariable<int32> CVarCompileShadersForDevelopment(
	TEXT("r.CompileShadersForDevelopment"),
	1,
	TEXT("Setting this to 0 allows to ship a game with more optimized shaders as some\n")
	TEXT("editor and development features are not longer compiled into the shaders.\n")
	TEXT(" Note: This should be done when shipping but it's not done automatically yet (feature need to mature\n")
	TEXT("       and shaders will compile slower as shader caching from development isn't shared).\n")
	TEXT("Cannot be changed at runtime - can be put into BaseEngine.ini\n")
	TEXT(" 0: off, shader can run a bit faster\n")
	TEXT(" 1: on (Default)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarPS4MixeedModeShaderDebugInfo(
	TEXT("r.PS4MixedModeShaderDebugInfo"),
	0,
	TEXT("Whether to compile shaders to allow mixed mode shader debugging. This will currently generate slower code.\n")
	TEXT(" 0: Normal mode\n")
	TEXT(" 1: Mixed mode)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarPS4DumpShaderSDB(
	TEXT("r.PS4DumpShaderSDB"),
	0,
	TEXT("Whether to dump shader sdb files used for shader association.\n")
	TEXT(" 0: Disabled\n")
	TEXT(" 1: Enabled)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDontLimitOnBattery(
	TEXT("r.DontLimitOnBattery"),
	0,
	TEXT("0: Limit performance on devices with a battery.(default)\n")
	TEXT("1: Do not limit performance due to device having a battery."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarViewDistanceScale(
	TEXT("r.ViewDistanceScale"),
	1.0f,
	TEXT("Controls the view distance scale. A primitive's MaxDrawDistance is scaled by this value.\n")
	TEXT("Higher values will increase view distance but at a performance cost.\n")
	TEXT("Default = 1. Value should be in the range [0.0f, 1.0f]."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLightFunctionQuality(
	TEXT("r.LightFunctionQuality"),
	2,
	TEXT("Defines the light function quality which allows to adjust for quality or performance.\n")
	TEXT("<=0: off (fastest)\n")
	TEXT("  1: low quality (e.g. half res with blurring, not yet implemented)\n")
	TEXT("  2: normal quality (default)\n")
	TEXT("  3: high quality (e.g. super-sampled or colored, not yet implemented)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarEyeAdaptationQuality(
	TEXT("r.EyeAdaptationQuality"),
	2,
	TEXT("Defines the eye adaptation quality which allows to adjust for quality or performance.\n")
	TEXT("<=0: off (fastest)\n")
	TEXT("  1: low quality (e.g. non histogram based, not yet implemented)\n")
	TEXT("  2: normal quality (default)\n")
	TEXT("  3: high quality (e.g. screen position localized, not yet implemented)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowDistanceScale(
	TEXT("r.Shadow.DistanceScale"),
	1.0f,
	TEXT("Scalability option to trade shadow distance versus performance for directional lights (clamped within a reasonable range).\n")
	TEXT("<1: shorter distance\n")
	TEXT(" 1: normal (default)\n")
	TEXT(">1: larger distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarFreeSkeletalMeshBuffers(
	TEXT("r.FreeSkeletalMeshBuffers"),
	0,
	TEXT("Controls whether skeletal mesh buffers are kept in CPU memory to support merging of skeletal meshes.\n")
	TEXT("0: Keep buffers(default)\n")
	TEXT("1: Free buffers"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTonemapperQuality(
	TEXT("r.TonemapperQuality"),
	1,
	TEXT("0: low (minor performance benefit)\n")
	TEXT("1: high (default, with high frequency pixel pattern to fight 8 bit color quantization)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDetailMode(
	TEXT("r.DetailMode"),
	2,
	TEXT("Current detail mode; determines whether components of actors should be updated/ ticked.\n")
	TEXT(" 0: low, show only object with DetailMode low or higher\n")
	TEXT(" 1: medium, show all object with DetailMode medium or higher\n")
	TEXT(" 2: high, show all objects (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRefractionQuality(
	TEXT("r.RefractionQuality"),
	2,
	TEXT("Defines the distorion/refraction quality which allows to adjust for quality or performance.\n")
	TEXT("<=0: off (fastest)\n")
	TEXT("  1: low quality (not yet implemented)\n")
	TEXT("  2: normal quality (default)\n")
	TEXT("  3: high quality (e.g. color fringe, not yet implemented)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDBuffer(
	TEXT("r.DBuffer"),
	0,
	TEXT("Experimental DBuffer feature: Generating deferred decals before the BasePass.\n")
	TEXT("Allows decals to be correctly lit by baked lighting. Receivers need to be rendered in the early zpass.\n")
	TEXT("At the moment only can be ensures by full enablng this pass: r.EarlyZPassMovable=1 r.EarlyZPass=2\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on (needs early pass rendering on all decal receivers and base pass lookups into the DBuffer, costs GPU memory, allows GBuffer compression)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSkeletalMeshLODRadiusScale(
	TEXT("r.SkeletalMeshLODRadiusScale"),
	1.0f,
	TEXT("Scale factor for the screen radius used in computing discrete LOD for skeletal meshes. (0.25-1)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);



static TAutoConsoleVariable<int32> CVarOnlyStreamInTextures(
	TEXT("r.OnlyStreamInTextures"),
	0,
	TEXT("If set to 1, texture will only be streamed in, not out"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPreTileTextures(
	TEXT("r.PreTileTextures"),
	1,
	TEXT("If set to 1, textures will be tiled during cook and are expected to be cooked at runtime"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVirtualTextureReducedMemoryEnabled(
	TEXT("r.VirtualTextureReducedMemory"),
	0,
	TEXT("If set to 1, the cost of virtual textures will be reduced by using a more packed layout."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPrecomputedVisibilityWarning(
	TEXT("r.PrecomputedVisibilityWarning"),
	0,
	TEXT("If set to 1, a warning will be displayed when rendering a scene from a view point without precomputed visibility."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarFeatureLevelPreview(
	TEXT("r.FeatureLevelPreview"),
	0,
	TEXT("If 1 the quick settings menu will contain an option to enable feature level preview modes"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarGBuffer(
	TEXT("r.GBuffer"),
	1,
	TEXT("0=Do not use GBuffer (fast minimal rendering path), 1=Use GBuffer [default]."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVerifyPeer(
	TEXT("n.VerifyPeer"),
	1,
	TEXT("Sets libcurl's CURL_OPT_SSL_VERIFYPEER option to verify authenticity of the peer's certificate.\n")
	TEXT("  0 = disable (allows self-signed certificates)\n")
	TEXT("  1 = enable [default]"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarEmitterSpawnRateScale(
	TEXT("r.EmitterSpawnRateScale"),
	1.0,
	TEXT("A global scale upon the spawn rate of emitters. Emitters can choose to apply or ignore it via their bApplyGlobalSpawnRateScale property."),
	ECVF_Scalability | ECVF_RenderThreadSafe);