// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ActorRecording.h"
#include "Misc/ScopedSlowTask.h"
#include "GameFramework/Character.h"
#include "Camera/CameraActor.h"
#include "Engine/SimpleConstructionScript.h"
#include "Editor.h"
#include "Animation/SkeletalMeshActor.h"
#include "LevelSequenceBindingReference.h"
#include "SequenceRecorderSettings.h"
#include "SequenceRecorderUtils.h"
#include "Sections/MovieSceneAnimationSectionRecorder.h"
#include "Sections/MovieScene3DTransformSectionRecorderSettings.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "MovieSceneFolder.h"
#include "MovieSceneTimeHelpers.h"
#include "CameraRig_Crane.h"
#include "CameraRig_Rail.h"
#include "SequenceRecorder.h"
#include "Features/IModularFeatures.h"
#include "Toolkits/AssetEditorManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));
static const FName MovieSceneSectionRecorderFactoryName("MovieSceneSectionRecorderFactory");

UActorRecording::UActorRecording(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActorSettings(this)
{
	TakeNumber = 1;
	bActive = true;
	bCreateLevelSequence = false;
	bWasSpawnedPostRecord = false;
	Guid.Invalidate();
	bNewComponentAddedWhileRecording = false;

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		AnimationSettings = Settings->DefaultAnimationSettings;
	}
}

bool UActorRecording::IsRelevantForRecording(AActor* Actor)
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	// don't record actors that sequencer has spawned itself!
	if(!Settings->bRecordSequencerSpawnedActors && Actor->ActorHasTag(SequencerActorTag))
	{
		return false;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(Actor);
	for(USceneComponent* SceneComponent : SceneComponents)
	{
		for (const FPropertiesToRecordForClass& PropertiesToRecordForClass : Settings->ClassesAndPropertiesToRecord)
		{
			if (SceneComponent->IsA(PropertiesToRecordForClass.Class))
			{
				return true;
			}
		}
	}

	return false;
}

bool UActorRecording::StartRecording(ULevelSequence* CurrentSequence, float CurrentSequenceTime, const FString& BaseAssetPath, const FString& SessionName)
{
	if (!bActive)
	{
		return false;
	}

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
	
	AActor* Actor = GetActorToRecord();

	if (Actor && bCreateLevelSequence)
	{
		FString AssetPath = BaseAssetPath / GetTargetName(Actor);

		TakeNumber = SequenceRecorderUtils::GetNewTakeNumber(AssetPath, GetTargetName(Actor), SessionName, TakeNumber);
		FString TakeName = SequenceRecorderUtils::MakeTakeName(GetTargetName(Actor), SessionName, TakeNumber);

		if (TargetLevelSequence != nullptr)
		{
			CurrentSequence = TargetLevelSequence;

			if (ShouldDuplicateLevelSequence())
			{
				IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

				CurrentSequence = CastChecked<ULevelSequence>(SequenceRecorderUtils::DuplicateAsset(AssetPath, TakeName, TargetLevelSequence));
					
				TargetLevelSequence = CurrentSequence;
			}
		}
		else
		{
			CurrentSequence = SequenceRecorderUtils::MakeNewAsset<ULevelSequence>(AssetPath, TakeName);

			if (CurrentSequence)
			{
				CurrentSequence->Initialize();
			}

			if (bCreateLevelSequence)
			{
				TargetLevelSequence = CurrentSequence;
			}
		}

		if (CurrentSequence)
		{			
			FAssetRegistryModule::AssetCreated(CurrentSequence);
		}
	}

	bNewComponentAddedWhileRecording = false;
	DuplicatedDynamicComponents.Reset();

	if(Actor)
	{
		if (TargetAnimation != nullptr)
		{
			IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(TargetAnimation, false);
			if (EditorInstance)
			{
				UE_LOG(LogAnimation, Log, TEXT("Closing '%s' so we don't invalidate the open version when unloading it."), *TargetAnimation->GetName());
				EditorInstance->CloseWindow();
			}
		}

		if(CurrentSequence != nullptr)
		{
			StartRecordingActorProperties(CurrentSequence, CurrentSequenceTime);
		}
		else
		{
			FString AnimAssetPath;
			FString AnimAssetName;
			if (TargetLevelSequence != nullptr)
			{
				AnimAssetName = TargetLevelSequence->GetName();
				AnimAssetPath = FPaths::GetPath(TargetLevelSequence->GetPathName());
				if (Settings->AnimationSubDirectory.Len() > 0)
				{
					AnimAssetPath /= Settings->AnimationSubDirectory;
				}
			}

			TSharedPtr<FMovieSceneAnimationSectionRecorder> AnimationRecorder = MakeShareable(new FMovieSceneAnimationSectionRecorder(AnimationSettings, TargetAnimation, AnimAssetPath, AnimAssetName));
			AnimationRecorder->CreateSection(GetActorToRecord(), nullptr, FGuid(), 0.0f);
			AnimationRecorder->Record(0.0f);
			SectionRecorders.Add(AnimationRecorder);			
		}
	}

	return true;
}

static FString GetUniqueSpawnableName(UMovieScene* MovieScene, const FString& BaseName)
{
	FString BlueprintName = BaseName;
	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == BlueprintName;
	};

	int32 Index = 2;
	FString UniqueString;
	while (MovieScene->FindSpawnable(DuplName))
	{
		BlueprintName.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		BlueprintName += UniqueString;
	}

	return BlueprintName;
}

void UActorRecording::GetSceneComponents(TArray<USceneComponent*>& OutArray, bool bIncludeNonCDO/*=true*/)
{
	// it is not enough to just go through the owned components array here
	// we need to traverse the scene component hierarchy as well, as some components may be 
	// owned by other actors (e.g. for pooling) and some may not be part of the hierarchy
	if(GetActorToRecord() != nullptr)
	{
		USceneComponent* RootComponent = GetActorToRecord()->GetRootComponent();
		if(RootComponent)
		{
			// note: GetChildrenComponents clears array!
			RootComponent->GetChildrenComponents(true, OutArray);
			OutArray.Add(RootComponent);
		}

		// add owned components that are *not* part of the hierarchy
		TInlineComponentArray<USceneComponent*> OwnedComponents(GetActorToRecord());
		for(USceneComponent* OwnedComponent : OwnedComponents)
		{
			check(OwnedComponent);
			if(OwnedComponent->GetAttachParent() == nullptr && OwnedComponent != RootComponent)
			{
				OutArray.Add(OwnedComponent);
			}
		}

		if(!bIncludeNonCDO)
		{
			AActor* CDO = Cast<AActor>(GetActorToRecord()->GetClass()->GetDefaultObject());

			auto ShouldRemovePredicate = [&](UActorComponent* PossiblyRemovedComponent)
				{
					if (PossiblyRemovedComponent == nullptr)
					{
						return true;
					}

					// try to find a component with this name in the CDO
					for (UActorComponent* SearchComponent : CDO->GetComponents())
					{
						if (SearchComponent->GetClass() == PossiblyRemovedComponent->GetClass() &&
							SearchComponent->GetFName() == PossiblyRemovedComponent->GetFName())
						{
							return false;
						}
					}

					// remove if its not found
					return true;
				};

			OutArray.RemoveAllSwap(ShouldRemovePredicate);
		}
	}
}

void UActorRecording::SyncTrackedComponents(bool bIncludeNonCDO/*=true*/)
{
	TArray<USceneComponent*> NewComponentArray;
	GetSceneComponents(NewComponentArray, bIncludeNonCDO);

	// Expire section recorders that are watching components no longer attached to our actor
	TSet<USceneComponent*> ExpiredComponents;
	for (TWeakObjectPtr<USceneComponent>& WeakComponent : TrackedComponents)
	{
		if (USceneComponent* Component = WeakComponent.Get())
		{
			ExpiredComponents.Add(Component);
		}
	}
	for (USceneComponent* Component : NewComponentArray)
	{
		ExpiredComponents.Remove(Component);
	}

	for (TSharedPtr<IMovieSceneSectionRecorder>& SectionRecorder : SectionRecorders)
	{
		if (USceneComponent* Component = Cast<USceneComponent>(SectionRecorder->GetSourceObject()))
		{
			if (ExpiredComponents.Contains(Component))
			{
				SectionRecorder->InvalidateObjectToRecord();
			}
		}
	}

	TrackedComponents.Reset(NewComponentArray.Num());
	for(USceneComponent* SceneComponent : NewComponentArray)
	{
		TrackedComponents.Add(SceneComponent);
	}
}

void UActorRecording::InvalidateObjectToRecord()
{
	ActorToRecord = nullptr;
	for(auto& SectionRecorder : SectionRecorders)
	{
		SectionRecorder->InvalidateObjectToRecord();
	}
}

bool UActorRecording::ValidComponent(USceneComponent* SceneComponent) const
{
	if(SceneComponent != nullptr)
	{
		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		for (const FPropertiesToRecordForClass& PropertiesToRecordForClass : Settings->ClassesAndPropertiesToRecord)
		{			
			if (PropertiesToRecordForClass.Class != nullptr && SceneComponent->IsA(PropertiesToRecordForClass.Class) && !SceneComponent->bIsEditorOnly)
			{
				return true;
			}
		}
	}

	return false;
}

void UActorRecording::FindOrAddFolder(UMovieScene* MovieScene)
{
	check(GetActorToRecord() != nullptr);

	FName FolderName(NAME_None);
	if(GetActorToRecord()->IsA<ACharacter>() || GetActorToRecord()->IsA<ASkeletalMeshActor>())
	{
		FolderName = TEXT("Characters");
	}
	else if(GetActorToRecord()->IsA<ACameraActor>() || GetActorToRecord()->IsA<ACameraRig_Crane>() || GetActorToRecord()->IsA<ACameraRig_Rail>())
	{
		FolderName = TEXT("Cameras");
	}
	else
	{
		FolderName = TEXT("Misc");
	}

	// look for a folder to put us in
	UMovieSceneFolder* FolderToUse = nullptr;
	for(UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
	{
		if(Folder->GetFolderName() == FolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	if(FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(FolderName);
		MovieScene->GetRootFolders().Add(FolderToUse);
	}

	FolderToUse->AddChildObjectBinding(Guid);
}

ULevelSequence* UActorRecording::GetActiveLevelSequence(ULevelSequence* InLevelSequence)
{
	if (TargetLevelSequence != nullptr)
	{
		return TargetLevelSequence;
	}

	return InLevelSequence;
}

bool UActorRecording::ShouldDuplicateLevelSequence()
{
	// Duplicate the level sequence if the take number we're trying to write to is not the same as our existing take number
	if (TargetLevelSequence != nullptr)
	{
		FString TakeName = TargetLevelSequence->GetName();
		FString TargetActorName;
		FString TargetSessionName;
		uint32 TargetTakeNumber;
		if (SequenceRecorderUtils::ParseTakeName(TakeName, TargetActorName, TargetSessionName, TargetTakeNumber))
		{
			if (TargetTakeNumber != TakeNumber)
			{
				return true;
			}
		}
	}

	return false;
}


FString UActorRecording::GetTargetName(AActor* InActor)
{
	if (!TargetName.IsEmpty())
	{
		return TargetName.ToString();
	}

	if (InActor)
	{
		return InActor->GetActorLabel();
	}

	return FString();
}


FGuid UActorRecording::GetActorInSequence(AActor* InActor, ULevelSequence* CurrentSequence)
{
	FString ActorTargetName = GetTargetName(InActor);

	CurrentSequence = GetActiveLevelSequence(CurrentSequence);

	UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
	
	for (int32 SpawnableCount = 0; SpawnableCount < MovieScene->GetSpawnableCount(); ++SpawnableCount)
	{
		const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableCount);
		if (Spawnable.GetName() == ActorTargetName || Spawnable.Tags.Contains(*InActor->GetActorLabel()))
		{
			return Spawnable.GetGuid();
		}
	}

	for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
		if (Possessable.GetName() == ActorTargetName || Possessable.Tags.Contains(*InActor->GetActorLabel()))
		{
			return Possessable.GetGuid();
		}
	}
	return FGuid();
}

void UActorRecording::StartRecordingActorProperties(ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	CurrentSequence = GetActiveLevelSequence(CurrentSequence);

	if(CurrentSequence != nullptr)
	{
		// set up our spawnable or possessable for this actor
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();

		AActor* Actor = GetActorToRecord();

		// Look for an existing guid in the current sequence to record to 
		FString ObjectBindingName = GetTargetName(Actor);
		Guid = GetActorInSequence(Actor, CurrentSequence);


		if (!Guid.IsValid())
		{
			if (bRecordToPossessable)
			{ 
				Guid = MovieScene->AddPossessable(ObjectBindingName, Actor->GetClass());
				CurrentSequence->BindPossessableObject(Guid, *Actor, Actor->GetWorld());
			}
			else
			{
				FString TemplateName = GetUniqueSpawnableName(MovieScene, Actor->GetName());

				AActor* ObjectTemplate = CastChecked<AActor>(CurrentSequence->MakeSpawnableTemplateFromInstance(*Actor, *TemplateName));

				if (ObjectTemplate)
				{
					TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
					ObjectTemplate->GetComponents(SkeletalMeshComponents);
					for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
					{
						SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
						SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
						SkeletalMeshComponent->ForcedLodModel = 1;
					}

					// Disable possession of pawns otherwise the recorded character will auto possess the player
					if (ObjectTemplate->IsA(APawn::StaticClass()))
					{
						APawn* Pawn = CastChecked<APawn>(ObjectTemplate);
						Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
					}

					Guid = MovieScene->AddSpawnable(ObjectBindingName, *ObjectTemplate);
				}
			}
		}

		// now add tracks to record
		if(Guid.IsValid())
		{
			// Tag the possessable/spawnable with the original actor label so we can find it later
			if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Guid))
			{
				if (!Possessable->Tags.Contains(*Actor->GetActorLabel()))
				{
					Possessable->Tags.AddUnique(FName(*Actor->GetActorLabel()));
				}
				Possessable->SetName(ObjectBindingName);
			}

			if (FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid))
			{
				if (!Spawnable->Tags.Contains(*Actor->GetActorLabel()))
				{
					Spawnable->Tags.AddUnique(FName(*Actor->GetActorLabel()));
				}
				Spawnable->SetName(ObjectBindingName);
			}

			// add our folder
			FindOrAddFolder(MovieScene);

			// force set recording to record translations as we need this with no animation
			UMovieScene3DTransformSectionRecorderSettings* TransformSettings = ActorSettings.GetSettingsObject<UMovieScene3DTransformSectionRecorderSettings>();
			check(TransformSettings);
			if (!TransformSettings->bRecordTransforms)
			{
				UE_LOG(LogAnimation, Warning, TEXT("Transform recording is not enabled for '%s'. Resulting animation may not match gameplay."), *Actor->GetActorLabel());
			}

			// grab components so we can track attachments
			// don't include non-CDO here as they wont be part of our initial BP (duplicated above)
			// we will catch these 'extra' components on the first tick
			const bool bIncludeNonCDO = false;
			SyncTrackedComponents(bIncludeNonCDO);

			TInlineComponentArray<USceneComponent*> SceneComponents(GetActorToRecord());

			// check if components need recording
			TInlineComponentArray<USceneComponent*> ValidSceneComponents;
			for(TWeakObjectPtr<USceneComponent>& SceneComponent : TrackedComponents)
			{
				if(ValidComponent(SceneComponent.Get()))
				{
					ValidSceneComponents.Add(SceneComponent.Get());

					// add all parent components too
					TArray<USceneComponent*> ParentComponents;
					SceneComponent->GetParentComponents(ParentComponents);
					for(USceneComponent* ParentComponent : ParentComponents)
					{	
						ValidSceneComponents.AddUnique(ParentComponent);
					}
				}
			}

			ProcessNewComponentArray(ValidSceneComponents);

			TSharedPtr<FMovieSceneAnimationSectionRecorder> FirstAnimRecorder = nullptr;
			for(USceneComponent* SceneComponent : ValidSceneComponents)
			{
				TSharedPtr<FMovieSceneAnimationSectionRecorder> AnimRecorder = StartRecordingComponentProperties(SceneComponent->GetFName(), SceneComponent, GetActorToRecord(), CurrentSequence, CurrentSequenceTime, AnimationSettings, TargetAnimation);
				if(!FirstAnimRecorder.IsValid() && AnimRecorder.IsValid() && GetActorToRecord()->IsA<ACharacter>())
				{
					FirstAnimRecorder = AnimRecorder;
				}
			}

			// we need to create a transform track even if we arent recording transforms
			if (FSequenceRecorder::Get().GetTransformRecorderFactory().CanRecordObject(GetActorToRecord()))
			{
				TSharedPtr<IMovieSceneSectionRecorder> Recorder = FSequenceRecorder::Get().GetTransformRecorderFactory().CreateSectionRecorder(TransformSettings->bRecordTransforms, FirstAnimRecorder);
				if(Recorder.IsValid())
				{ 
					Recorder->CreateSection(GetActorToRecord(), MovieScene, Guid, CurrentSequenceTime);
					Recorder->Record(CurrentSequenceTime);
					SectionRecorders.Add(Recorder);
				}
			}

			TArray<IMovieSceneSectionRecorderFactory*> ModularFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneSectionRecorderFactory>(MovieSceneSectionRecorderFactoryName);
			for (IMovieSceneSectionRecorderFactory* Factory : ModularFeatures)
			{
				if (Factory->CanRecordObject(GetActorToRecord()))
				{
					TSharedPtr<IMovieSceneSectionRecorder> Recorder = Factory->CreateSectionRecorder(ActorSettings);
					if (Recorder.IsValid())
					{
						Recorder->CreateSection(GetActorToRecord(), MovieScene, Guid, CurrentSequenceTime);
						Recorder->Record(CurrentSequenceTime);
						SectionRecorders.Add(Recorder);
					}
				}
			}	
		}
	}
}

TSharedPtr<FMovieSceneAnimationSectionRecorder> UActorRecording::StartRecordingComponentProperties(const FName& BindingName, USceneComponent* SceneComponent, UObject* BindingContext, ULevelSequence* CurrentSequence, float CurrentSequenceTime, const FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InTargetSequence)
{
	CurrentSequence = GetActiveLevelSequence(CurrentSequence);

	// first create a possessable for this component to be controlled by
	UMovieScene* OwnerMovieScene = CurrentSequence->GetMovieScene();

	FGuid PossessableGuid;
	for (int32 PossessableCount = 0; PossessableCount < OwnerMovieScene->GetPossessableCount(); ++PossessableCount)
	{
		const FMovieScenePossessable& Possessable = OwnerMovieScene->GetPossessable(PossessableCount);
		if (Possessable.GetParent() == Guid && Possessable.GetName() == BindingName.ToString() && Possessable.GetPossessedObjectClass() == SceneComponent->GetClass())
		{
			PossessableGuid = Possessable.GetGuid();
			break;
		}
	}
	
	if (!PossessableGuid.IsValid())
	{
		PossessableGuid = OwnerMovieScene->AddPossessable(BindingName.ToString(), SceneComponent->GetClass());
	}

	// Set up parent/child guids for possessables within spawnables
	FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
	if (ensure(ChildPossessable))
	{
		ChildPossessable->SetParent(Guid);
	}

	FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(Guid);
	if (ParentSpawnable)
	{
		ParentSpawnable->AddChildPossessable(PossessableGuid);
	}

	CurrentSequence->BindPossessableObject(PossessableGuid, *SceneComponent, BindingContext);

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
	
	// First try built-in animation recorder...
	TSharedPtr<FMovieSceneAnimationSectionRecorder> AnimationRecorder = nullptr;
	if (FSequenceRecorder::Get().GetAnimationRecorderFactory().CanRecordObject(SceneComponent))
	{
		FString AnimAssetPath;
		FString AnimAssetName;
		if (TargetLevelSequence != nullptr)
		{
			AnimAssetName = TargetLevelSequence->GetName();
			AnimAssetPath = FPaths::GetPath(TargetLevelSequence->GetPathName());
			if (Settings->AnimationSubDirectory.Len() > 0)
			{
				AnimAssetPath /= Settings->AnimationSubDirectory;
			}
		}

		AnimationRecorder = FSequenceRecorder::Get().GetAnimationRecorderFactory().CreateSectionRecorder(InTargetSequence, InAnimationSettings, AnimAssetPath, AnimAssetName);
		AnimationRecorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime);
		AnimationRecorder->Record(CurrentSequenceTime);
		SectionRecorders.Add(AnimationRecorder);
	}

	// ...and transform...
	if (FSequenceRecorder::Get().GetTransformRecorderFactory().CanRecordObject(SceneComponent))
	{
		TSharedPtr<IMovieSceneSectionRecorder> Recorder = FSequenceRecorder::Get().GetTransformRecorderFactory().CreateSectionRecorder(true, nullptr);
		if (Recorder.IsValid())
		{
			Recorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime);
			Recorder->Record(CurrentSequenceTime);
			SectionRecorders.Add(Recorder);
		}
	}

	// ...now any external recorders
	TArray<IMovieSceneSectionRecorderFactory*> ModularFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneSectionRecorderFactory>(MovieSceneSectionRecorderFactoryName);
	for (IMovieSceneSectionRecorderFactory* Factory : ModularFeatures)
	{
		if (Factory->CanRecordObject(SceneComponent))
		{
			TSharedPtr<IMovieSceneSectionRecorder> Recorder = Factory->CreateSectionRecorder(ActorSettings);
			if (Recorder.IsValid())
			{
				Recorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime);
				Recorder->Record(CurrentSequenceTime);
				SectionRecorders.Add(Recorder);
			}
		}
	}

	return AnimationRecorder;
}

void UActorRecording::Tick(float DeltaSeconds, ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	if (IsRecording())
	{
		CurrentSequence = GetActiveLevelSequence(CurrentSequence);

		if(CurrentSequence)
		{
			// check our components to see if they have changed
			static TArray<USceneComponent*> SceneComponents;
			GetSceneComponents(SceneComponents);

			if(TrackedComponents.Num() != SceneComponents.Num())
			{
				StartRecordingNewComponents(CurrentSequence, CurrentSequenceTime);
			}
		}

		for(auto& SectionRecorder : SectionRecorders)
		{
			SectionRecorder->Record(CurrentSequenceTime);
		}
	}
}

bool UActorRecording::StopRecording(ULevelSequence* OriginalSequence, float CurrentSequenceTime)
{
	if (!bActive)
	{
		return false;
	}

	ULevelSequence* CurrentSequence = GetActiveLevelSequence(OriginalSequence);

	FString ActorName;
	if(CurrentSequence)
	{
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		check(MovieScene);

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid);
		if(Spawnable)
		{
			ActorName = Spawnable->GetName();
		}
	}

	FScopedSlowTask SlowTask((float)SectionRecorders.Num() + 1.0f, FText::Format(NSLOCTEXT("SequenceRecorder", "ProcessingActor", "Processing Actor {0}"), FText::FromString(ActorName)));

	// stop property recorders
	for(auto& SectionRecorder : SectionRecorders)
	{
		SlowTask.EnterProgressFrame();

		SectionRecorder->FinalizeSection(CurrentSequenceTime);
	}

	SlowTask.EnterProgressFrame();

	SectionRecorders.Empty();

	if (TargetLevelSequence != nullptr)
	{
		// set movie scene playback range to encompass all sections
		SequenceRecorderUtils::ExtendSequencePlaybackRange(TargetLevelSequence);

		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		if(Settings->bAutoSaveAsset || GEditor == nullptr)
		{
			SequenceRecorderUtils::SaveLevelSequence(TargetLevelSequence);
		}
	}

	// Add this sequence as a subtrack
	if (CurrentSequence && OriginalSequence && OriginalSequence != CurrentSequence)
	{
		UMovieScene* MovieScene = OriginalSequence->GetMovieScene();
		UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(MovieScene->FindMasterTrack(UMovieSceneSubTrack::StaticClass()));
		if (!SubTrack)
		{
			SubTrack = Cast<UMovieSceneSubTrack>(MovieScene->AddMasterTrack(UMovieSceneSubTrack::StaticClass()));
		}

		// Remove the current take if it exists
		FString CurrentActorName;
		FString CurrentSessionName;
		uint32 CurrentTakeNumber;
		TOptional<int32> RowIndex;
		TOptional<int32> NewTakeNumber;
		if (SequenceRecorderUtils::ParseTakeName(CurrentSequence->GetName(), CurrentActorName, CurrentSessionName, CurrentTakeNumber))
		{
			NewTakeNumber = CurrentTakeNumber + 1;

			for (UMovieSceneSection* Section : SubTrack->GetAllSections())
			{
				UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
				UMovieSceneSequence* SubSequence = SubSection->GetSequence();
				if (SubSequence)
				{
					FString SubActorName;
					FString SubSessionName;
					uint32 SubTakeNumber;
					if (SequenceRecorderUtils::ParseTakeName(SubSequence->GetName(), SubActorName, SubSessionName, SubTakeNumber))
					{
						if (SubActorName == CurrentActorName && SubSessionName == CurrentSessionName)
						{
							RowIndex = Section->GetRowIndex();
							SubTrack->RemoveSection(*Section);
							break;
						}
					}
				}
			}
		}

		// Add new take sub section
		if (!RowIndex.IsSet())
		{
			RowIndex = SubTrack->GetMaxRowIndex() + 1;
		}

		FFrameNumber RecordStartTime = OriginalSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
		UMovieSceneSubSection* SubSection = SubTrack->AddSequence(CurrentSequence, RecordStartTime,  MovieScene::DiscreteSize(CurrentSequence->GetMovieScene()->GetPlaybackRange()));
		SubSection->SetRowIndex(RowIndex.GetValue());		

		SubTrack->FixRowIndices();

		// Increment the take number for the next recording
		if (NewTakeNumber.IsSet())
		{
			TakeNumber = NewTakeNumber.GetValue();
		}
	}

	// Swap to editor actor in case the actor was set while in PIE
	if (AActor* Actor = ActorToRecord.Get())
	{
		if (AActor* EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(Actor))
		{
			ActorToRecord = EditorActor;
		}
	}

	return true;
}

bool UActorRecording::IsRecording() const
{
	return GetActorToRecord() && SectionRecorders.Num() > 0;
}

AActor* UActorRecording::GetActorToRecord() const
{
	if (AActor* AssignedActor = ActorToRecord.Get())
	{
		AActor* OutActor = EditorUtilities::GetSimWorldCounterpartActor(AssignedActor);

		if (OutActor != nullptr)
		{
			return OutActor;
		}

		return AssignedActor;
	}

	return nullptr;
}

void UActorRecording::SetActorToRecord(AActor* InActor)
{
	ActorToRecord = InActor; 

	if (InActor != nullptr)
	{
		bRecordToPossessable = false;

		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		for (const FSettingsForActorClass& SettingsForActorClass : Settings->PerActorSettings)
		{
			if (InActor->GetClass()->IsChildOf(SettingsForActorClass.Class))
			{
				bRecordToPossessable = SettingsForActorClass.bRecordToPossessable;
			}
		}
	}
}

void UActorRecording::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UActorRecording, ActorToRecord))
	{
		if (AActor* Actor = GetActorToRecord())
		{
			bRecordToPossessable = false;

			const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
			for (const FSettingsForActorClass& SettingsForActorClass : Settings->PerActorSettings)
			{
				if (Actor->GetClass()->IsChildOf(SettingsForActorClass.Class))
				{
					bRecordToPossessable = SettingsForActorClass.bRecordToPossessable;
				}
			}
		}
	}
}

static FName FindParentComponentOwnerClassName(USceneComponent* SceneComponent, UBlueprint* Blueprint)
{
	if(SceneComponent->GetAttachParent())
	{
		FName AttachName = SceneComponent->GetAttachParent()->GetFName();

		// see if we can find this component in the BP inheritance hierarchy
		while(Blueprint)
		{
			if(Blueprint->SimpleConstructionScript->FindSCSNode(AttachName) != nullptr)
			{
				return Blueprint->GetFName();
			}

			Blueprint = Cast<UBlueprint>(Blueprint->GeneratedClass->GetSuperClass()->ClassGeneratedBy);
		}
	}

	return NAME_None;
}

int32 GetAttachmentDepth(USceneComponent* Component)
{
	int32 Depth = 0;

	USceneComponent* Parent = Component->GetAttachParent();
	while (Parent)
	{
		++Depth;
		Parent = Parent->GetAttachParent();
	}

	return Depth;
}

void UActorRecording::ProcessNewComponentArray(TInlineComponentArray<USceneComponent*>& ProspectiveComponents) const
{
	// Only iterate as far as the current size of the array (it may grow inside the loop)
	int32 LastIndex = ProspectiveComponents.Num();
	for(int32 Index = 0; Index < LastIndex; ++Index)
	{
		USceneComponent* NewComponent = ProspectiveComponents[Index];

		USceneComponent* Parent = ProspectiveComponents[Index]->GetAttachParent();

		while (Parent)
		{
			TWeakObjectPtr<USceneComponent> WeakParent(Parent);
			if (TrackedComponents.Contains(WeakParent) || ProspectiveComponents.Contains(Parent) || Parent->GetOwner() != NewComponent->GetOwner())
			{
				break;
			}
			else
			{
				ProspectiveComponents.Add(Parent);
			}

			Parent = Parent->GetAttachParent();
		}
	}

	// Sort parent first, to ensure that attachments get added properly
	TMap<USceneComponent*, int32> AttachmentDepths;
	for (USceneComponent* Component : ProspectiveComponents)
	{
		AttachmentDepths.Add(Component, GetAttachmentDepth(Component));
	}

	ProspectiveComponents.Sort(
		[&](USceneComponent& A, USceneComponent& B)
		{
			return *AttachmentDepths.Find(&A) < *AttachmentDepths.Find(&B);
		}
	);
}

void UActorRecording::StartRecordingNewComponents(ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	CurrentSequence = GetActiveLevelSequence(CurrentSequence);

	if (GetActorToRecord() != nullptr)
	{
		// find the new component(s)
		TInlineComponentArray<USceneComponent*> NewComponents;
		TArray<USceneComponent*> SceneComponents;
		GetSceneComponents(SceneComponents);
		for(USceneComponent* SceneComponent : SceneComponents)
		{
			if(ValidComponent(SceneComponent))
			{
				TWeakObjectPtr<USceneComponent> WeakSceneComponent(SceneComponent);
				int32 FoundIndex = TrackedComponents.Find(WeakSceneComponent);
				if(FoundIndex == INDEX_NONE)
				{
					// new component!
					NewComponents.Add(SceneComponent);
				}
			}
		}

		ProcessNewComponentArray(NewComponents);

		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		check(MovieScene);

		FAnimationRecordingSettings ComponentAnimationSettings = AnimationSettings;
		ComponentAnimationSettings.bRemoveRootAnimation = false;
		ComponentAnimationSettings.bRecordInWorldSpace = false;

		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		if (!bRecordToPossessable)
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid);
			check(Spawnable);

			AActor* ObjectTemplate = CastChecked<AActor>(Spawnable->GetObjectTemplate());

			for (USceneComponent* SceneComponent : NewComponents)
			{
				// new component, so we need to add this to our BP if it didn't come from SCS
				FName NewName;
				if (SceneComponent->CreationMethod != EComponentCreationMethod::SimpleConstructionScript)
				{
					// Give this component a unique name within its parent
					NewName = *FString::Printf(TEXT("Dynamic%s"), *SceneComponent->GetFName().GetPlainNameString());
					NewName.SetNumber(1);
					while (FindObjectFast<UObject>(ObjectTemplate, NewName))
					{
						NewName.SetNumber(NewName.GetNumber() + 1);
					}

					USceneComponent* TemplateRoot = ObjectTemplate->GetRootComponent();
					USceneComponent* AttachToComponent = nullptr;

					// look for a similar attach parent in the current structure
					USceneComponent* AttachParent = SceneComponent->GetAttachParent();
					if(AttachParent != nullptr)
					{
						// First off, check if we're attached to a component that has already been duplicated into this object
						// If so, the name lookup will fail, so we use a direct reference
						if (TWeakObjectPtr<USceneComponent>* DuplicatedComponent = DuplicatedDynamicComponents.Find(AttachParent))
						{
							AttachToComponent = DuplicatedComponent->Get();
						}
					
						// If we don't have an attachment parent duplicated already, perform a name lookup
						if (!AttachToComponent)
						{
							FName AttachName = SceneComponent->GetAttachParent()->GetFName();

							TInlineComponentArray<USceneComponent*> AllChildren;
							ObjectTemplate->GetComponents(AllChildren);

							for (USceneComponent* Child : AllChildren)
							{
								CA_SUPPRESS(28182); // Dereferencing NULL pointer. 'Child' contains the same NULL value as 'AttachToComponent' did.
								if (Child->GetFName() == AttachName)
								{
									AttachToComponent = Child;
									break;
								}
							}
						}
					}

					if (!AttachToComponent)
					{
						AttachToComponent = ObjectTemplate->GetRootComponent();
					}

					USceneComponent* NewTemplateComponent = Cast<USceneComponent>(StaticDuplicateObject(SceneComponent, ObjectTemplate, NewName, RF_AllFlags & ~RF_Transient));
					NewTemplateComponent->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, SceneComponent->GetAttachSocketName());

					ObjectTemplate->AddInstanceComponent(NewTemplateComponent);

					DuplicatedDynamicComponents.Add(SceneComponent, NewTemplateComponent);
				}
				else
				{
					NewName = SceneComponent->GetFName();
				}

				StartRecordingComponentProperties(NewName, SceneComponent, GetActorToRecord(), CurrentSequence, CurrentSequenceTime, ComponentAnimationSettings, nullptr);

				bNewComponentAddedWhileRecording = true;
			}

			SyncTrackedComponents();
		}
		else
		{
			for (USceneComponent* SceneComponent : NewComponents)
			{
				// new component, start recording
				StartRecordingComponentProperties(SceneComponent->GetFName(), SceneComponent, GetActorToRecord(), CurrentSequence, CurrentSequenceTime, ComponentAnimationSettings, nullptr);
			}

			SyncTrackedComponents();
		}
	}
}
