// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

FGameplayTagContainer::FGameplayTagContainer()
{}

FGameplayTagContainer::FGameplayTagContainer(FGameplayTagContainer const& Other)
{
	*this = Other;
}

FGameplayTagContainer& FGameplayTagContainer::operator=(FGameplayTagContainer const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}
	GameplayTags.Empty(Other.GameplayTags.Num());
	GameplayTags.Append(Other.GameplayTags);

	return *this;
}

bool FGameplayTagContainer::operator==(FGameplayTagContainer const& Other) const
{
	if (GameplayTags.Num() != Other.GameplayTags.Num())
	{
		return false;
	}
	return Filter(Other, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::All).Num() == this->Num();
}

bool FGameplayTagContainer::operator!=(FGameplayTagContainer const& Other) const
{
	if (GameplayTags.Num() != Other.GameplayTags.Num())
	{
		return true;
	}
	return Filter(Other, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::All).Num() != this->Num();
}

bool FGameplayTagContainer::HasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const
{
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
	for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
	{
		if (TagManager.GameplayTagsMatch(*It, TagToCheck, TagMatchType, TagToCheckMatchType) == true)
		{
			return true;
		}
	}
	return false;
}

FGameplayTagContainer FGameplayTagContainer::GetGameplayTagParents() const
{
	FGameplayTagContainer ResultContainer;
	ResultContainer.AppendTags(*this);

	for (TArray<FGameplayTag>::TConstIterator TagIterator(GameplayTags); TagIterator; ++TagIterator)
	{
		FGameplayTagContainer ParentTags = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagParents(*TagIterator);
		ResultContainer.AppendTags(ParentTags);
	}

	return ResultContainer;
}

FGameplayTagContainer FGameplayTagContainer::Filter(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, TEnumAsByte<EGameplayContainerMatchType::Type> ContainerMatchType) const
{
	FGameplayTagContainer ResultContainer;
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

	
	for (TArray<FGameplayTag>::TConstIterator OtherIt(OtherContainer.GameplayTags); OtherIt; ++OtherIt)
	{
		bool bTagFound = false;
		
		for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
		{
			if (TagManager.GameplayTagsMatch(*It, *OtherIt, TagMatchType, OtherTagMatchType) == true)
			{
				bTagFound = true;
				if (ContainerMatchType == EGameplayContainerMatchType::Any)
				{
					ResultContainer.AddTag(*It);
				}
			}
		}
		if (ContainerMatchType == EGameplayContainerMatchType::All && bTagFound == false)
		{
			return ResultContainer; // return the empty container;
		}
	}

	if (ContainerMatchType == EGameplayContainerMatchType::All)
	{
		ResultContainer.AppendTags(*this);
	}
	return ResultContainer;
}


bool FGameplayTagContainer::MatchesAll(FGameplayTagContainer const& Other, bool bCountEmptyAsMatch) const
{
	if (Other.Num() == 0)
	{
		return bCountEmptyAsMatch;
	}

	return Filter(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::All).Num() > 0;
}

bool FGameplayTagContainer::MatchesAny(FGameplayTagContainer const& Other, bool bCountEmptyAsMatch) const
{
	if (Other.Num() == 0)
	{
		return bCountEmptyAsMatch;
	}

	return Filter(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::Any).Num() > 0;
}

void FGameplayTagContainer::AppendTags(FGameplayTagContainer const& Other)
{
	//add all the tags
	for(TArray<FGameplayTag>::TConstIterator It(Other.GameplayTags); It; ++It)
	{
		AddTag(*It);
	}
}

void FGameplayTagContainer::AddTag(const FGameplayTag& TagToAdd)
{
	if (TagToAdd.IsValid())
	{
		// Don't want duplicate tags
		GameplayTags.AddUnique(TagToAdd);
	}
}

void FGameplayTagContainer::RemoveTag(FGameplayTag TagToRemove)
{
	GameplayTags.Remove(TagToRemove);
}

void FGameplayTagContainer::RemoveAllTags()
{
	GameplayTags.Empty();
}

bool FGameplayTagContainer::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		FGameplayTagContainer ExpandedContainer = GetGameplayTagParents();
		Tags.Empty(ExpandedContainer.Num());

		for (auto TagIter = ExpandedContainer.CreateConstIterator(); TagIter; ++TagIter)
		{
			Tags.Add((*TagIter).GetTagName());
		}
	}

	Ar << Tags;

	if (Ar.IsLoading())
	{
		RedirectTags();
		
		UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
		for (auto It = Tags.CreateConstIterator(); It; ++It)
		{
			FGameplayTag Tag = TagManager.RequestGameplayTag(*It);
			TagManager.AddLeafTagToContainer(*this, Tag);
		}
	}
	return true;
}

void FGameplayTagContainer::RedirectTags()
{
	FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
	
	for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
	{
		if (It.Key() == TEXT("GameplayTagRedirects"))
		{
			FName OldTagName = NAME_None;
			FName NewTagName;

			FParse::Value( *It.Value(), TEXT("OldTagName="), OldTagName );
			FParse::Value( *It.Value(), TEXT("NewTagName="), NewTagName );

			int32 TagIndex = 0;
			if (Tags.Find(OldTagName, TagIndex))
			{
				Tags.RemoveAt(TagIndex);
				Tags.AddUnique(NewTagName);
			}

			FGameplayTag OldTag = TagManager.RequestGameplayTag(OldTagName);
			FGameplayTag NewTag = TagManager.RequestGameplayTag(NewTagName);

			if (HasTag(OldTag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
			{
				RemoveTag(OldTag);
				AddTag(NewTag);
			}
		}
	}
}

int32 FGameplayTagContainer::Num() const
{
	return GameplayTags.Num();
}

FString FGameplayTagContainer::ToString() const
{
	FString RetString = TEXT("(GameplayTags=(");
	for (int i = 0; i < GameplayTags.Num(); ++i)
	{
		RetString += TEXT("(TagName=\"");
		RetString += GameplayTags[i].ToString();
		RetString += TEXT("\")");
		if (i < GameplayTags.Num() - 1)
		{
			RetString += TEXT(",");
		}
	}
	RetString += TEXT("))");
	return RetString;
}

FGameplayTag::FGameplayTag()
	: TagName(NAME_None)
{
}

bool FGameplayTag::Matches(const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().GameplayTagsMatch(*this, Other, MatchTypeOne, MatchTypeTwo);
}

bool FGameplayTag::IsValid() const
{
	return (TagName != NAME_None);
}

FGameplayTag::FGameplayTag(FName Name)
	: TagName(Name)
{
	check(IGameplayTagsModule::Get().GetGameplayTagsManager().ValidateTagCreation(Name));
}
