
#include "LyraClone_ControllerComponent_CharacterParts.h"
#include "LyraClone_PawnComponent_CharacterParts.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_ControllerComponent_CharacterParts)

ULyraClone_ControllerComponent_CharacterParts::ULyraClone_ControllerComponent_CharacterParts(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
}

PRAGMA_DISABLE_OPTIMIZATION
ULyraClone_PawnComponent_CharacterParts* ULyraClone_ControllerComponent_CharacterParts::GetPawnCustomizer() const
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		return ControlledPawn->FindComponentByClass<ULyraClone_PawnComponent_CharacterParts>();
	}
	return nullptr;
}

void ULyraClone_ControllerComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			OwningController->OnPossessedPawnChanged.AddDynamic(this,&ThisClass::OnPossessedPawnChanged);
		}
	}
}
void ULyraClone_ControllerComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllCharacterParts();
	Super::EndPlay(EndPlayReason);
}



void ULyraClone_ControllerComponent_CharacterParts::AddCharacterPartInternal(const FLyraClone_CharacterPart& NewPart)
{
	FLyraClone_Controller_CharacterPartEntry& NewEntry = CharacterParts.AddDefaulted_GetRef();
	NewEntry.Part = NewPart;

	if (ULyraClone_PawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		NewEntry.Handle = PawnCustomizer->AddCharacterPart(NewPart);
	}
}

void ULyraClone_ControllerComponent_CharacterParts::RemoveAllCharacterParts()
{
	if (ULyraClone_PawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		for (FLyraClone_Controller_CharacterPartEntry& Entry : CharacterParts)
		{
			PawnCustomizer->RemoveCharacterParts(Entry.Handle);
		}
	}
	CharacterParts.Reset();
}

PRAGMA_ENABLE_OPTIMIZATION

void ULyraClone_ControllerComponent_CharacterParts::AddCharacterPart(const FLyraClone_CharacterPart& NewPart)
{
	AddCharacterPartInternal(NewPart);
}

void ULyraClone_ControllerComponent_CharacterParts::OnPossessedPawnChanged(APawn* Oldpawn, APawn* Newpawn)
{
	if (ULyraClone_PawnComponent_CharacterParts* OldCustomizer = Oldpawn ? Oldpawn->FindComponentByClass<ULyraClone_PawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraClone_Controller_CharacterPartEntry& Entry : CharacterParts)
		{
			OldCustomizer->RemoveCharacterParts(Entry.Handle);
			Entry.Handle.Reset();
		}

	}

	if (ULyraClone_PawnComponent_CharacterParts* NewCustomizer = Newpawn ? Newpawn->FindComponentByClass<ULyraClone_PawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraClone_Controller_CharacterPartEntry& Entry : CharacterParts)
		{
			check(!Entry.Handle.IsValid());
			Entry.Handle = NewCustomizer->AddCharacterPart(Entry.Part);
		}
	}

}
