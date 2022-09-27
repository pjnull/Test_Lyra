// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectNodeProjectorParameterDetails.h"
#include "CustomizableObjectEditorModule.h"
#include "PropertyCustomizationHelpers.h"
#include "CustomizableObjectEditorUtilities.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Runtime/Launch/Resources/Version.h"
#include "Engine/SkeletalMesh.h"
#include "Widgets/Input/STextComboBox.h"
#include "Math/Vector4.h"
#include "Math/Vector.h"

#include "Runtime/Launch/Resources/Version.h"
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 22) || ENGINE_MAJOR_VERSION >= 5
#include "HAL/PlatformApplicationMisc.h"
#endif


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeProjectorParameterDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeProjectorParameterDetails );
}


void FCustomizableObjectNodeProjectorParameterDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	NodeConstant = nullptr;
	NodeParameter = nullptr;
	DetailBuilderPtr = &DetailBuilder;

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 18) || ENGINE_MAJOR_VERSION >= 5
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		if (DetailsView->GetSelectedObjects()[0].Get()->IsA(UCustomizableObjectNodeProjectorConstant::StaticClass()))
		{
			NodeConstant = Cast<UCustomizableObjectNodeProjectorConstant>(DetailsView->GetSelectedObjects()[0].Get());
		}
		else if (DetailsView->GetSelectedObjects()[0].Get()->IsA(UCustomizableObjectNodeProjectorParameter::StaticClass()))
		{
			NodeParameter = Cast<UCustomizableObjectNodeProjectorParameter>(DetailsView->GetSelectedObjects()[0].Get());
		}
	}
#else
	const IDetailsView& DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView.GetSelectedObjects().Num())
	{
		if (DetailsView.GetSelectedObjects()[0].Get()->IsA(UCustomizableObjectNodeProjectorConstant::StaticClass()))
		{
			NodeConstant = Cast<UCustomizableObjectNodeProjectorConstant>(DetailsView.GetSelectedObjects()[0].Get());
		}
		else if (DetailsView.GetSelectedObjects()[0].Get()->IsA(UCustomizableObjectNodeProjectorParameter::StaticClass()))
		{
			NodeParameter = Cast<UCustomizableObjectNodeProjectorParameter>(DetailsView.GetSelectedObjects()[0].Get());
		}
	}
#endif

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Clipboard" );
	//BlocksCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );

	if ((NodeConstant != nullptr) || (NodeParameter != nullptr))
	{
		BlocksCategory.AddCustomRow( LOCTEXT("FCustomizableObjectNodeProjectorParameterDetails", "Projector Data") )
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText( LOCTEXT( "Copy_Projector", "Copy projector location to clipboard.") )
				.OnClicked( this, &FCustomizableObjectNodeProjectorParameterDetails::OnProjectorCopyPressed )
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Copy","Copy"))
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText( LOCTEXT( "Paste_Projector", "Paste projector location from clipboard.") )
				.OnClicked( this, &FCustomizableObjectNodeProjectorParameterDetails::OnProjectorPastePressed )
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Paste","Paste"))
				]
			]
		];
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("Node", "Node") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "Node not found", "Node not found" ) )
		];
	}

	TSharedPtr<IPropertyHandle> ReferenceSkeletonIndexPropertyHandle = DetailBuilder.GetProperty("ReferenceSkeletonIndex");
	const FSimpleDelegate OnReferenceSkeletonIndexChangedDelegate =
		FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeProjectorParameterDetails::OnReferenceSkeletonIndexChanged);
	ReferenceSkeletonIndexPropertyHandle->SetOnPropertyValueChanged(OnReferenceSkeletonIndexChangedDelegate);

	IDetailCategoryBuilder& SnapToBoneCategory = DetailBuilder.EditCategory("ProjectorSnapToBone");
	DetailBuilder.HideProperty("ProjectorBone");

	SkeletalMesh = nullptr;
	TSharedPtr<FString> BoneToSelect;

	UCustomizableObject* CustomizableObject = nullptr;
	FName ProjectorBoneName;
	int32 ReferenceIndex = 0;

	if (NodeConstant != nullptr)
	{
		CustomizableObject = Cast<UCustomizableObject>(NodeConstant->GetCustomizableObjectGraph()->GetOuter());
		ProjectorBoneName = NodeConstant->ProjectorBone;
		ReferenceIndex = NodeConstant->ReferenceSkeletonIndex;
	}
	else if (NodeParameter != nullptr)
	{
		CustomizableObject = Cast<UCustomizableObject>(NodeParameter->GetCustomizableObjectGraph()->GetOuter());
		ProjectorBoneName = NodeParameter->ProjectorBone;
		ReferenceIndex = NodeParameter->ReferenceSkeletonIndex;
	}

	//UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Node->GetCustomizableObjectGraph()->GetOuter());

	if (CustomizableObject)
	{
		SkeletalMesh = CustomizableObject->GetRefSkeletalMesh(ReferenceIndex);
	}

	if (SkeletalMesh)
	{
		BoneComboOptions.Empty();
			
		for (int32 i = 0; i < SkeletalMesh->GetRefSkeleton().GetNum(); ++i)
		{
			FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(i);
			BoneComboOptions.Add(MakeShareable(new FString(BoneName.ToString())));

			if (BoneName == ProjectorBoneName)
			{
				BoneToSelect = BoneComboOptions.Last();
			}
		}

        BoneComboOptions.Sort(CompareNames);

		// Add them to the parent combo box
		TSharedRef<IPropertyHandle> BoneProperty = DetailBuilder.GetProperty("ProjectorBone");

		SnapToBoneCategory.AddCustomRow(LOCTEXT("UCustomizableObjectNodeProjectorParameter", "Projector Bone"))
		[
			SNew(SProperty, BoneProperty)
			.ShouldDisplayName(false)
			.CustomWidget()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("FCustomizableObjectNodeRemoveMeshDetails", "Projector Bone"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					+SHorizontalBox::Slot().HAlign(HAlign_Fill)
					[
						SNew(STextComboBox)
						.OptionsSource(&BoneComboOptions)
						.InitiallySelectedItem(BoneToSelect)
						.OnSelectionChanged(this, &FCustomizableObjectNodeProjectorParameterDetails::OnBoneComboBoxSelectionChanged, BoneProperty)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			]
		];
	}
}


FReply FCustomizableObjectNodeProjectorParameterDetails::OnProjectorCopyPressed()
{
	FString ExportedText;

	if (NodeParameter)
	{
		UScriptStruct* Struct = NodeParameter->DefaultValue.StaticStruct();
        Struct->ExportText(ExportedText, &NodeParameter->DefaultValue, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
	}
	else if(NodeConstant)
	{
		UScriptStruct* Struct = NodeConstant->Value.StaticStruct();
        Struct->ExportText(ExportedText, &NodeConstant->Value, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
	}

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 22) || ENGINE_MAJOR_VERSION >= 5
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
#else
	FPlatformMisc::ClipboardCopy(*ExportedText);
#endif

	return FReply::Handled();
}


FReply FCustomizableObjectNodeProjectorParameterDetails::OnProjectorPastePressed()
{
	FString ClipText;

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 22) || ENGINE_MAJOR_VERSION >= 5
	FPlatformApplicationMisc::ClipboardPaste(ClipText);
#else
	FPlatformMisc::ClipboardPaste(ClipText);
#endif

	TSharedPtr<ICustomizableObjectEditor> Editor = nullptr;

	if (NodeParameter)
	{
		UScriptStruct* Struct = NodeParameter->DefaultValue.StaticStruct();
		Struct->ImportText(*ClipText, &NodeParameter->DefaultValue, nullptr, 0, GLog, GetPathNameSafe(Struct));
		NodeParameter->ParameterSetModified = 2;
		Editor = NodeParameter->GetGraphEditor();
	}
	else if (NodeConstant)
	{
		UScriptStruct* Struct = NodeConstant->Value.StaticStruct();
		Struct->ImportText(*ClipText, &NodeConstant->Value, nullptr, 0, GLog, GetPathNameSafe(Struct));
		NodeConstant->ParameterSetModified = 2;
		Editor = NodeConstant->GetGraphEditor();
	}

	if (Editor.IsValid())
	{
		Editor->UpdateGraphNodeProperties();
	}

	return FReply::Handled();
}

void FCustomizableObjectNodeProjectorParameterDetails::OnBoneComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> BoneProperty)
{
	for (int OptionIndex = 0; OptionIndex < BoneComboOptions.Num(); ++OptionIndex)
	{
		if (BoneComboOptions[OptionIndex] == Selection)
		{
			FVector Location = FVector::ZeroVector;
			FVector Direction = FVector::ForwardVector;

			if (SkeletalMesh)
			{
				const TArray<FTransform>& BoneArray = SkeletalMesh->GetRefSkeleton().GetRefBonePose();
				int32 ParentIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(FName(**Selection));

				FVector ChildLocation = FVector::ForwardVector;

				for (int32 i = 0; i < SkeletalMesh->GetRefSkeleton().GetNum(); ++i)
				{
					if (SkeletalMesh->GetRefSkeleton().GetParentIndex(i) == ParentIndex)
					{
						ChildLocation = BoneArray[i].TransformPosition(FVector::ZeroVector);
						break;
					}
				}

				while (ParentIndex >= 0)
				{
					Location = BoneArray[ParentIndex].TransformPosition(Location);
					ChildLocation = BoneArray[ParentIndex].TransformPosition(ChildLocation);
					ParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(ParentIndex);
				}

				Direction = (ChildLocation - Location).GetSafeNormal();

				if ((Location != FVector::ZeroVector) && (Direction != FVector::ZeroVector))
				{
					FVector UpTemp = (FVector::DotProduct(Direction, FVector(0.0f, 0.0f, 1.0f)) > 0.99f) ? FVector(0.1f, 0.1f, 1.0f).GetSafeNormal() : FVector(0.0f, 0.0f, 1.0f);
					FVector Right = FVector::CrossProduct(UpTemp, Direction);
					FVector UpFinal = FVector::CrossProduct(Direction, Right);

					if (NodeConstant != nullptr)
					{
						NodeConstant->BoneComboBoxLocation = Location;
						NodeConstant->BoneComboBoxForwardDirection = Direction;
						NodeConstant->BoneComboBoxUpDirection = UpFinal.GetSafeNormal();
					}
					else if (NodeParameter != nullptr)
					{
						NodeParameter->BoneComboBoxLocation = Location;
						NodeParameter->BoneComboBoxForwardDirection = Direction;
						NodeParameter->BoneComboBoxUpDirection = UpFinal.GetSafeNormal();
					}
				}
			}

			BoneProperty->SetValue(*BoneComboOptions[OptionIndex].Get());

			return;
		}
	}
}


void FCustomizableObjectNodeProjectorParameterDetails::OnReferenceSkeletonIndexChanged()
{
	if (NodeConstant != nullptr)
	{
		NodeConstant->ProjectorBone = FName();
	}
	else if (NodeParameter != nullptr)
	{
		NodeParameter->ProjectorBone = FName();
	}

	DetailBuilderPtr->ForceRefreshDetails();
}

#undef LOCTEXT_NAMESPACE