// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSnapshotsEditorCustomWidgetGenerator.h"

#include "LevelSnapshotsLog.h"
#include "Views/Results/LevelSnapshotsEditorResultsRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSnapshotsEditor"

void LevelSnapshotsEditorCustomWidgetGenerator::CreateRowsForPropertiesNotHandledByPropertyRowGenerator(
	TFieldPath<FProperty> InFieldPath,
	UObject* InSnapshotObject,
	UObject* InWorldObject, 
	const TWeakPtr<SLevelSnapshotsEditorResults>& InResultsView,
	const TWeakPtr<FLevelSnapshotsEditorResultsRow>& InDirectParentRow)
{
	if (!ensure(InFieldPath->IsValidLowLevel() && IsValid(InSnapshotObject) && IsValid(InWorldObject)))
	{
		return;
	}

	TSharedPtr<SWidget> CustomSnapshotWidget;
	TSharedPtr<SWidget> CustomWorldWidget;

	if (InFieldPath->GetName() == "AttachParent")
	{
		auto WidgetTextEditLambda = [] (const FString& InWidgetText, const UObject* InPropertyObject)
		{
			if (const USceneComponent* AsSceneComponent = Cast<USceneComponent>(InPropertyObject))
			{
				if (const AActor* OwningActor = AsSceneComponent->GetOwner())
				{
					return FString::Printf(TEXT("%s.%s"), *OwningActor->GetActorLabel(), *InWidgetText);
				}
			}

			return InWidgetText;
		};
		
		CustomSnapshotWidget = GenerateObjectPropertyWidget(InFieldPath, InSnapshotObject, WidgetTextEditLambda);
		CustomWorldWidget = GenerateObjectPropertyWidget(InFieldPath, InWorldObject, WidgetTextEditLambda);
	}
	else
	{
		UE_LOG(LogLevelSnapshots, Warning, TEXT("%hs: Unsupported Property found named '%s' with FieldPath: %s"), __FUNCTION__, *InFieldPath->GetAuthoredName(), *InFieldPath.ToString());
		
		CustomSnapshotWidget = GenerateGenericPropertyWidget(InFieldPath, InSnapshotObject, nullptr);
		CustomWorldWidget = GenerateGenericPropertyWidget(InFieldPath, InWorldObject, nullptr);
	}

	if (!CustomSnapshotWidget.IsValid() && !CustomWorldWidget.IsValid())
	{
		return;
	}

	// Create property
	const FLevelSnapshotsEditorResultsRowPtr NewProperty = 
		MakeShared<FLevelSnapshotsEditorResultsRow>(InFieldPath->GetDisplayNameText(), FLevelSnapshotsEditorResultsRow::SingleProperty, ECheckBoxState::Checked, 
		InResultsView, InDirectParentRow);

	NewProperty->InitPropertyRowWithCustomWidget(InDirectParentRow, InFieldPath.Get(), CustomSnapshotWidget, CustomWorldWidget);

	InDirectParentRow.Pin()->AddToChildRows(NewProperty);
}

TSharedPtr<SWidget> LevelSnapshotsEditorCustomWidgetGenerator::GenerateGenericPropertyWidget(
	TFieldPath<FProperty> InFieldPath, UObject* InObject,
	TFunction<FString(const FString&, const UObject*)> InWidgetTextEditLambda)
{
	const FProperty* Property = InFieldPath.Get();

	if (CastField<FStructProperty>(Property))
	{
		// Don't generate widgets for FStructProperty
		return nullptr;
	}

	if (InObject->IsValidLowLevel())
	{
		void* PropertyValue;

		UObject* ParentObject = Property->GetOwner<UObject>();
		const FName OwnerObjectName = ParentObject->GetFName();

		if (const FProperty* OwnerProperty = FindFProperty<FProperty>(InObject->GetClass(), OwnerObjectName))
		{
			void* OwnerPropertyValue = OwnerProperty->ContainerPtrToValuePtr<void>(InObject);
			PropertyValue = OwnerProperty->ContainerPtrToValuePtr<void>(OwnerPropertyValue);
		}
		else
		{
			PropertyValue = Property->ContainerPtrToValuePtr<void>(InObject);
		}

		if (PropertyValue != nullptr)
		{
			FString ValueText;
			Property->ExportTextItem(ValueText, PropertyValue, nullptr, ParentObject, PPF_None);

			if (InWidgetTextEditLambda)
			{
				ValueText = InWidgetTextEditLambda(ValueText, InObject);
			}

			return SNew(STextBlock)
				.Text(FText::FromString(ValueText))
				.Font(FEditorStyle::GetFontStyle("BoldFont"))
				.ToolTipText(FText::FromString(ValueText));
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> LevelSnapshotsEditorCustomWidgetGenerator::GenerateObjectPropertyWidget(
	TFieldPath<FProperty> InFieldPath, const UObject* InObject, TFunction<FString(const FString&, const UObject*)> InWidgetTextEditLambda)
{
	if (InObject->IsValidLowLevel())
	{
		if (const FObjectProperty* AsAttachParentObjectProperty = CastField<FObjectProperty>(InFieldPath.Get()))
		{
			const UObject* PropertyObject = AsAttachParentObjectProperty->GetObjectPropertyValue_InContainer(InObject);

			if (PropertyObject->IsValidLowLevel())
			{
				FString WidgetText = PropertyObject->GetName();

				if (InWidgetTextEditLambda)
				{
					WidgetText = InWidgetTextEditLambda(WidgetText, PropertyObject);
				}

				return SNew(STextBlock)
				.Text(FText::FromString(WidgetText))
				.Font(FEditorStyle::GetFontStyle("BoldFont"))
				.ToolTipText(FText::FromString(WidgetText));

			}
			else
			{
				return SNew(STextBlock)
				.Text(LOCTEXT("LevelSnapshotsEditorResults_AttachParentInvalidText","No Attach Parent found."));
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

