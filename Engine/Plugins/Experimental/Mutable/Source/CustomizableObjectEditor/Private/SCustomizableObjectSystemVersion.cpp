// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCustomizableObjectSystemVersion.h"
#include "CustomizableObjectEditorModule.h"
#include "ICustomizableObjectModule.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Interfaces/IPluginManager.h"


void SCustomizableObjectSystemVersion::Construct( const FArguments& InArgs )
{
	FString Version = ICustomizableObjectModule::Get().GetPluginVersion();

	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0)
		.Padding(2)
		[
			SNew(STextBlock)
				.Text(FText::FromString("v:"))
			]
			+SGridPanel::Slot(1,0)
			.Padding(2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Version))
			]			
		]
	];

}


void SCustomizableObjectSystemVersion::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}



