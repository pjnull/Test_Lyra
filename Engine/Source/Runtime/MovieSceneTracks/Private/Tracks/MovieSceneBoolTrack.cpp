// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneBoolTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

bool UMovieSceneBoolTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneBoolSection::StaticClass();
}

UMovieSceneSection* UMovieSceneBoolTrack::CreateNewSection()
{
	return NewObject<UMovieSceneBoolSection>(this, NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieSceneBoolTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneBoolPropertySectionTemplate(*CastChecked<const UMovieSceneBoolSection>(&InSection), *this);
}