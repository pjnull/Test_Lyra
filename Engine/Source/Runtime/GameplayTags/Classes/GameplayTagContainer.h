// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.generated.h"

UENUM(BlueprintType)
namespace EGameplayTagMatchType
{
	enum Type
	{
		Explicit,			// This will check for a match against just this tag
		IncludeParentTags,	// This will also check for matches against all parent tags
	};
}

UENUM(BlueprintType)
namespace EGameplayContainerMatchType
{
	enum Type
	{
		Any,	//	Means the filter is populated by any tag matches in this container.
		All,	//	Means the filter is only populated if all of the tags in this container match.
	};
}

USTRUCT(BlueprintType)
struct GAMEPLAYTAGS_API FGameplayTag
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FGameplayTag();

	/** Operators */
	bool operator==(FGameplayTag const& Other) const
	{
		return TagName == Other.TagName;
	}

	bool operator!=(FGameplayTag const& Other) const
	{
		return TagName != Other.TagName;
	}

	bool operator<(FGameplayTag const& Other) const
	{
		return TagName < Other.TagName;
	}
	
	/**
	 * Check to see if two FGameplayTags match
	 *
	 * @param Other			The second tag to compare against
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	bool Matches(const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
	
	/** Returns whether the tag is valid or not; Invalid tags are set to NAME_None and do not exist in the game-specific global dictionary */
	bool IsValid() const;

	/** Used so we can have a TMap of this struct*/
	friend uint32 GetTypeHash(const FGameplayTag& Tag)
	{
		return ::GetTypeHash(Tag.TagName);
	}

	/** Displays container as a string for blueprint graph usage */
	FString ToString() const
	{
		return TagName.ToString();
	}

	/** Get the tag represented as a name */
	FName GetTagName() const
	{
		return TagName;
	}

	friend FArchive& operator<<(FArchive& Ar, FGameplayTag& GameplayTag)
	{
		Ar << GameplayTag.TagName;
		return Ar;
	}

private:

	/** Intentionally private so only the tag manager can use */
	explicit FGameplayTag(FName InTagName);

	/** This Tags Name */
	UPROPERTY()
	FName TagName;

	friend class UGameplayTagsManager;
};

/** Simple struct for a gameplay tag container */
USTRUCT(BlueprintType)
struct GAMEPLAYTAGS_API FGameplayTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FGameplayTagContainer();
	FGameplayTagContainer(FGameplayTagContainer const& Other);

	/** Assignment/Equality operators */
	FGameplayTagContainer& operator=(FGameplayTagContainer const& Other);
	bool operator==(FGameplayTagContainer const& Other) const;
	bool operator!=(FGameplayTagContainer const& Other) const;

	/**  Returns a new container containing all of the tags of this container, as well as all of their parent tags */
	FGameplayTagContainer GetGameplayTagParents() const;

	/**
	 * Returns a filtered version of this container, as if the container were filtered by matches from the parameter container
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 * @param ContainerMatchType	Type of match to use for filtering
	 *
	 * @return A FGameplayTagContainer containing the filtered tags
	 */
	FGameplayTagContainer Filter(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, TEnumAsByte<EGameplayContainerMatchType::Type> ContainerMatchType) const;

	/**
	 * Checks if this container matches ANY of the tags in the specified container. Performs matching by expanding this container out
	 * to include its parent tags.
	 *
	 * @param Other					Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 *
	 * @return True if this container has ANY the tags of the passed in container
	 */
	bool MatchesAny(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const;

	/**
	* Checks if this container matches ALL of the tags in the specified container. Performs matching by expanding this container out to
	* include its parent tags.
	*
	* @param Other				Container we are checking against
	* @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	* 
	* @return True if this container has ALL the tags of the passed in container
	*/
	bool MatchesAll(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const;

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	virtual bool HasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const;

	/** 
	 * Adds all the tags from one container to this container 
	 *
	 * @param Other TagContainer that has the tags you want to add to this container 
	 */
	virtual void AppendTags(FGameplayTagContainer const& Other);

	/**
	 * Add the specified tag to the container
	 *
	 * @param TagToAdd Tag to add to the container
	 */
	void AddTag(const FGameplayTag& TagToAdd);

	/**
	 * Tag to remove from the container
	 * 
	 * @param TagToRemove	Tag to remove from the container
	 * @param RemoveChildlessParents Also Remove any parents that would be left with no children
	 */
	virtual void RemoveTag(FGameplayTag TagToRemove);

	/** Remove all tags from the container */
	virtual void RemoveAllTags();

	/**
	 * Serialize the tag container
	 *
	 * @param Ar	Archive to serialize to
	 *
	 * @return True if the container was serialized
	 */
	bool Serialize(FArchive& Ar);

	/** Renames any tags that may have changed by the ini file */
	void RedirectTags();

	/**
	 * Returns the Tag Count
	 *
	 * @return The number of tags
	 */
	int32 Num() const;

	/** Returns human readable Tag list */
	FString ToString() const;

	/** Creates a const iterator for the contents of this array */
	TArray<FGameplayTag>::TConstIterator CreateConstIterator() const
	{
		return GameplayTags.CreateConstIterator();
	}

private:

	// @todo: Remove this via engine version bump/native serialization soon
	/** Array of gameplay tags */
	UPROPERTY()
	TArray<FName> Tags;

	/** Array of gameplay tags */
	UPROPERTY()
	TArray<FGameplayTag> GameplayTags;
};

template<>
struct TStructOpsTypeTraits<FGameplayTagContainer> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true,
		WithCopy = true
	};
};
