// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAudioSlider.h"
#include "Brushes/SlateImageBrush.h"
#include "DSP/Dsp.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

// SAudioSliderBase
const FVector2D SAudioSliderBase::LinearRange = FVector2D(0.0f, 1.0f);
const FVariablePrecisionNumericInterface SAudioSliderBase::NumericInterface = FVariablePrecisionNumericInterface();

SAudioSliderBase::SAudioSliderBase()
{
}

void SAudioSliderBase::Construct(const SAudioSliderBase::FArguments& InArgs)
{
	OnValueChanged = InArgs._OnValueChanged;
	ValueAttribute = InArgs._Value;
	AlwaysShowLabel = InArgs._AlwaysShowLabel;
	LabelBackgroundColor = InArgs._LabelBackgroundColor;
	SliderBackgroundColor = InArgs._SliderBackgroundColor;
	SliderBarColor = InArgs._SliderBarColor;
	SliderThumbColor = InArgs._SliderThumbColor;
	WidgetBackgroundColor = InArgs._WidgetBackgroundColor;
	Orientation = InArgs._Orientation;
	DesiredSizeOverride = InArgs._DesiredSizeOverride;

	// Get style
	const ISlateStyle* AudioSliderStyle = FSlateStyleRegistry::FindSlateStyle("AudioSliderStyle");
	if (AudioSliderStyle)
	{
		// Todo: set textbox max size to size of maximum possible value within range 
		//const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		//FVector2D MaxValueLabelSize = FMath::Max(FontMeasureService->Measure(FText::AsNumber(OutputRange.X), FTextBlockStyle::GetDefault().Font), FontMeasureService->Measure(FText::AsNumber(OutputRange.Y), FTextBlockStyle::GetDefault().Font));

		// Create components
		// Images 
		SAssignNew(WidgetBackgroundImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.WidgetBackground"))
			.ColorAndOpacity(WidgetBackgroundColor);

		SAssignNew(SliderBackgroundTopCapImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundCap"))
			.ColorAndOpacity(SliderBackgroundColor)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f));
		SAssignNew(SliderBackgroundRectangleImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundRectangle"))
			.ColorAndOpacity(SliderBackgroundColor);
		SAssignNew(SliderBackgroundBottomCapImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundCap"))
			.ColorAndOpacity(SliderBackgroundColor)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f));

		SAssignNew(SliderBarTopCapImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBarCap"))
			.ColorAndOpacity(SliderBarColor)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f));
		SAssignNew(SliderBarRectangleImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBarRectangle"))
			.ColorAndOpacity(SliderBarColor);
		SAssignNew(SliderBarBottomCapImage, SImage)
			.Image(AudioSliderStyle->GetBrush("AudioSlider.SliderBarCap"))
			.ColorAndOpacity(SliderBarColor)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f));

		// Text label
		SAssignNew(TextLabel, SConstraintCanvas)
			.Visibility_Lambda([this]()
			{
				return (AlwaysShowLabel.Get() || this->IsHovered()) ? EVisibility::Visible : EVisibility::Hidden;
			})
			+ SConstraintCanvas::Slot()
				.Anchors(FAnchors(0.0f, 0.5f, 1.0f, 0.5f))
				.Alignment(FVector2D(0.0f, 0.5f))
				.Offset(FMargin(0.0f, 0.0f, 0.0f, 20.0f))
				[
					SAssignNew(LabelBackgroundImage, SImage)
					.Image(AudioSliderStyle->GetBrush("AudioSlider.LabelBackground"))
					.ColorAndOpacity(LabelBackgroundColor)
				]
			+ SConstraintCanvas::Slot()
			.Anchors(FAnchors(0.0f, 0.5f, 1.0f, 0.5f))
			.Alignment(FVector2D(0.0f, 0.5f))
			.Offset(FMargin(0.0f, 0.0f, 100.0f, 32.0f))
			.Offset(FMargin(0.0f, 0.0f, 0.0f, 32.0f))
			.AutoSize(true)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.5)
				.Padding(4.0f, 0.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ValueText, SEditableText)
					.Text(FText::AsNumber(GetOutputValue(ValueAttribute.Get())))
					.Justification(ETextJustify::Right)
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
					{
						const float OutputValue = FCString::Atof(*Text.ToString());
						const float LinValue = GetLinValue(OutputValue);
						ValueAttribute.Set(LinValue);
						Slider->SetValue(ValueAttribute);
						OnValueChanged.ExecuteIfBound(LinValue);
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f, 6.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(UnitsText, SEditableText)
					.Text(FText::FromString("units"))
				]
			];

		// Underlying slider widget
		SAssignNew(Slider, SSlider)
			.Value(ValueAttribute.Get())
			.Style(&AudioSliderStyle->GetWidgetStyle<FSliderStyle>("AudioSlider.Slider"))
			.Orientation(Orientation.Get())
			.OnValueChanged_Lambda([this](float Value)
				{
					ValueAttribute.Set(Value);
					OnValueChanged.ExecuteIfBound(Value);
					const float OutputValue = GetOutputValue(Value);
					ValueText->SetText(FText::FromString(NumericInterface.ToString(OutputValue)));
				});
	}

	ChildSlot
	[
		CreateWidgetLayout()
	];
}

void SAudioSliderBase::SetValue(float LinValue)
{
	ValueAttribute.Set(LinValue);
	const float OutputValue = GetOutputValue(LinValue);
	ValueText->SetText(FText::FromString(NumericInterface.ToString(OutputValue)));
	Slider->SetValue(LinValue);
}

void SAudioSliderBase::SetOrientation(EOrientation InOrientation)
{
	SetAttribute(Orientation, TAttribute<EOrientation>(InOrientation), EInvalidateWidgetReason::Layout);
	Slider->SetOrientation(InOrientation);
	WidgetSwitcher->SetActiveWidgetIndex(Orientation.Get());

	// Set widget component orientations
	const ISlateStyle* AudioSliderStyle = FSlateStyleRegistry::FindSlateStyle("AudioSliderStyle");
	if (AudioSliderStyle)
	{
		if (Orientation.Get() == Orient_Horizontal)
		{
			WidgetBackgroundImage->SetDesiredSizeOverride(AudioSliderStyle->GetVector("AudioSlider.DesiredWidgetSizeHorizontal"));

			SliderBackgroundTopCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-90.0f)), FVector2D(AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundCap")->ImageSize.Y / 2.0f, 0.0f)));
			SliderBackgroundRectangleImage->SetDesiredSizeOverride(FVector2D(AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundRectangle")->ImageSize.Y, AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundRectangle")->ImageSize.X));
			SliderBackgroundBottomCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f)), FVector2D(-AudioSliderStyle->GetBrush("AudioSlider.SliderBackgroundCap")->ImageSize.Y / 2.0f, 0.0f)));

			SliderBarTopCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-90.0f)), FVector2D(AudioSliderStyle->GetBrush("AudioSlider.SliderBarCap")->ImageSize.Y / 2.0f, 0.0f)));
			SliderBarRectangleImage->SetDesiredSizeOverride(FVector2D(AudioSliderStyle->GetBrush("AudioSlider.SliderBarRectangle")->ImageSize.Y, AudioSliderStyle->GetBrush("AudioSlider.SliderBarRectangle")->ImageSize.X));
			SliderBarBottomCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f)), FVector2D(-AudioSliderStyle->GetBrush("AudioSlider.SliderBarCap")->ImageSize.Y / 2.0f, 0.0f)));

		}
		else if (Orientation.Get() == Orient_Vertical)
		{
			WidgetBackgroundImage->SetDesiredSizeOverride(TOptional<FVector2D>());

			SliderBackgroundTopCapImage->SetRenderTransform(FSlateRenderTransform());
			SliderBackgroundRectangleImage->SetDesiredSizeOverride(TOptional<FVector2D>());
			SliderBackgroundBottomCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(180.0f))));
				
			SliderBarTopCapImage->SetRenderTransform(FSlateRenderTransform());
			SliderBarRectangleImage->SetDesiredSizeOverride(TOptional<FVector2D>());
			SliderBarBottomCapImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(180.0f))));
		}
	}
}

FVector2D SAudioSliderBase::ComputeDesiredSize(float) const
{
	if (DesiredSizeOverride.Get().IsSet())
	{
		return DesiredSizeOverride.Get().GetValue();
	}
	const ISlateStyle* AudioSliderStyle = FSlateStyleRegistry::FindSlateStyle("AudioSliderStyle");
	if (ensure(AudioSliderStyle))
	{
		const FName DesiredSizeName = Orientation.Get() == Orient_Vertical
			? "AudioSlider.DesiredWidgetSizeVertical"
			: "AudioSlider.DesiredWidgetSizeHorizontal";
		return AudioSliderStyle->GetVector(DesiredSizeName);
	}
	else
	{
		FVector2D DesiredWidgetSize = FVector2D(26.0f, 477.0f);
		return Orientation.Get() == Orient_Vertical ? DesiredWidgetSize : FVector2D(DesiredWidgetSize.Y, DesiredWidgetSize.X);
	}
}

void SAudioSliderBase::SetDesiredSizeOverride(const FVector2D Size)
{
	SetAttribute(DesiredSizeOverride, TAttribute<TOptional<FVector2D>>(Size), EInvalidateWidgetReason::Layout);
}

void SAudioSliderBase::SetUnitsText(const FText Units)
{
	UnitsText->SetText(Units);
}

void SAudioSliderBase::SetTextReadOnly(const bool bIsReadOnly)
{
	UnitsText->SetIsReadOnly(bIsReadOnly);
	ValueText->SetIsReadOnly(bIsReadOnly);
}

void SAudioSliderBase::SetAlwaysShowLabel(const bool bAlwaysShowLabel)
{
	AlwaysShowLabel = bAlwaysShowLabel;
}

const float SAudioSliderBase::GetOutputValue(const float LinValue)
{
	return FMath::Clamp(LinValue, OutputRange.X, OutputRange.Y);
}

const float SAudioSliderBase::GetLinValue(const float OutputValue)
{
	return OutputValue;
}

void SAudioSliderBase::SetSliderBackgroundColor(FSlateColor InSliderBackgroundColor)
{
	SetAttribute(SliderBackgroundColor, TAttribute<FSlateColor>(InSliderBackgroundColor), EInvalidateWidgetReason::Paint);
	SliderBackgroundRectangleImage->SetColorAndOpacity(SliderBackgroundColor);
	SliderBackgroundTopCapImage->SetColorAndOpacity(SliderBackgroundColor);
	SliderBackgroundBottomCapImage->SetColorAndOpacity(SliderBackgroundColor);
}

void SAudioSliderBase::SetSliderBarColor(FSlateColor InSliderBarColor)
{
	SetAttribute(SliderBarColor, TAttribute<FSlateColor>(InSliderBarColor), EInvalidateWidgetReason::Paint);
	SliderBarRectangleImage->SetColorAndOpacity(SliderBarColor);
	SliderBarTopCapImage->SetColorAndOpacity(SliderBarColor);
	SliderBarBottomCapImage->SetColorAndOpacity(SliderBarColor);
}

void SAudioSliderBase::SetSliderThumbColor(FSlateColor InSliderThumbColor)
{
	SetAttribute(SliderThumbColor, TAttribute<FSlateColor>(InSliderThumbColor), EInvalidateWidgetReason::Paint);
	Slider->SetSliderHandleColor(SliderThumbColor.Get());
}

void SAudioSliderBase::SetLabelBackgroundColor(FSlateColor InLabelBackgroundColor)
{
	SetAttribute(LabelBackgroundColor, TAttribute<FSlateColor>(InLabelBackgroundColor), EInvalidateWidgetReason::Paint);
	LabelBackgroundImage->SetColorAndOpacity(LabelBackgroundColor);
}

void SAudioSliderBase::SetWidgetBackgroundColor(FSlateColor InWidgetBackgroundColor)
{
	SetAttribute(WidgetBackgroundColor, TAttribute<FSlateColor>(InWidgetBackgroundColor), EInvalidateWidgetReason::Paint);
	WidgetBackgroundImage->SetColorAndOpacity(WidgetBackgroundColor);
}

void SAudioSliderBase::SetOutputRange(const FVector2D Range)
{
	if (Range.Y > Range.X)
	{
		OutputRange = Range;
		SetValue(FMath::Clamp(ValueAttribute.Get(), Range.X, Range.Y));
	}
}

TSharedRef<SWidgetSwitcher> SAudioSliderBase::CreateWidgetLayout()
{
	const ISlateStyle* AudioSliderStyle = FSlateStyleRegistry::FindSlateStyle("AudioSliderStyle");
	if (AudioSliderStyle)
	{
		SAssignNew(WidgetSwitcher, SWidgetSwitcher);
	
		// Create overall layout
		// Horizontal orientation
		WidgetSwitcher->AddSlot(EOrientation::Orient_Horizontal)
		[
			SNew(SOverlay)
			// Widget background image
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				WidgetBackgroundImage.ToSharedRef()
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				// Slider
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SOverlay)
					// Slider background image
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SliderBackgroundTopCapImage.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SliderBackgroundRectangleImage.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SliderBackgroundBottomCapImage.ToSharedRef()
						]
					]
					// Slider bar image
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.Padding(20.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SliderBarTopCapImage.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SliderBarRectangleImage.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SliderBarBottomCapImage.ToSharedRef()
						]
					]
					+ SOverlay::Slot()
					// Actual SSlider
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.Padding(10.0f, 0.0f)
					[
						Slider.ToSharedRef()
					]
				]
				// Text Label
				+ SHorizontalBox::Slot()
				.Padding(AudioSliderStyle->GetFloat("AudioSlider.LabelVerticalPadding"), 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					TextLabel.ToSharedRef()
				]
			]	
		];
		// Vertical orientation
		WidgetSwitcher->AddSlot(EOrientation::Orient_Vertical)
		[
			SNew(SOverlay)
			// Widget background image
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			[
				WidgetBackgroundImage.ToSharedRef()
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				// Text Label
				+ SVerticalBox::Slot()
				.Padding(0.0f, AudioSliderStyle->GetFloat("AudioSlider.LabelVerticalPadding"))
				.AutoHeight()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					TextLabel.ToSharedRef()
				]
				// Slider
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				[
					SNew(SOverlay)
					// Slider background image
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.AutoHeight()
						[
							SliderBackgroundTopCapImage.ToSharedRef()
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.FillHeight(1.0f)
						[
							SliderBackgroundRectangleImage.ToSharedRef()
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.AutoHeight()
						[
							SliderBackgroundBottomCapImage.ToSharedRef()
						]
					]
					// Slider bar image
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					.Padding(0.0f, 10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.AutoHeight()
						[
							SliderBarTopCapImage.ToSharedRef()
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.FillHeight(1.0f)
						[
							SliderBarRectangleImage.ToSharedRef()
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.AutoHeight()
						[
							SliderBarBottomCapImage.ToSharedRef()
						]
					]
					+ SOverlay::Slot()
					// Actual SSlider
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						Slider.ToSharedRef()
					]
				]
			]		
		];
		WidgetSwitcher->SetActiveWidgetIndex(Orientation.Get());
		SetOrientation(Orientation.Get());
	}
	return WidgetSwitcher.ToSharedRef();
}

// SAudioSlider
SAudioSlider::SAudioSlider()
{
}

void SAudioSlider::Construct(const SAudioSliderBase::FArguments& InArgs)
{
	SAudioSliderBase::Construct(InArgs);
}

void SAudioSlider::SetLinToOutputCurve(const TWeakObjectPtr<const UCurveFloat> InLinToOutputCurve)
{
	LinToOutputCurve = InLinToOutputCurve;
	ValueText->SetText(FText::FromString(SAudioSliderBase::NumericInterface.ToString(GetOutputValue(ValueAttribute.Get()))));
}

void SAudioSlider::SetOutputToLinCurve(const TWeakObjectPtr<const UCurveFloat> InOutputToLinCurve)
{
	OutputToLinCurve = InOutputToLinCurve;
}

const TWeakObjectPtr<const UCurveFloat> SAudioSlider::GetOutputToLinCurve()
{
	return OutputToLinCurve;
}

const TWeakObjectPtr<const UCurveFloat> SAudioSlider::GetLinToOutputCurve()
{
	return LinToOutputCurve;
}

const float SAudioSlider::GetOutputValue(const float LinValue)
{
	if (LinToOutputCurve.IsValid())
	{
		return LinToOutputCurve->GetFloatValue(LinValue);
	}
	return FMath::GetMappedRangeValueClamped(LinearRange, OutputRange, LinValue);
}

const float SAudioSlider::GetLinValue(const float OutputValue)
{
	if (OutputToLinCurve.IsValid())
	{
		return OutputToLinCurve->GetFloatValue(OutputValue);
	}
	return FMath::GetMappedRangeValueClamped(OutputRange, LinearRange, OutputValue);
}

// SAudioVolumeSlider
SAudioVolumeSlider::SAudioVolumeSlider()
{
	OutputRange = FVector2D(-100.0f, 12.0f);
}

void SAudioVolumeSlider::Construct(const SAudioSlider::FArguments& InArgs)
{
	SAudioSlider::Construct(InArgs);

	SetLinToOutputCurve(LoadObject<UCurveFloat>(NULL, TEXT("/AudioWidgets/AudioControlCurves/AudioControl_LinDbCurveDefault"), NULL, LOAD_None, NULL));
	SetOutputToLinCurve(LoadObject<UCurveFloat>(NULL, TEXT("/AudioWidgets/AudioControlCurves/AudioControl_DbLinCurveDefault"), NULL, LOAD_None, NULL));
	SetUnitsText(FText::FromString("dB"));
}

// SAudioFrequencySlider
SAudioFrequencySlider::SAudioFrequencySlider()
{
	OutputRange = FVector2D(20.0f, 20000.0f);
}

void SAudioFrequencySlider::Construct(const SAudioSlider::FArguments& InArgs)
{
	SAudioSliderBase::Construct(InArgs);
	SetUnitsText(FText::FromString("Hz"));
}

const float SAudioFrequencySlider::GetOutputValue(const float LinValue)
{
	return Audio::GetLogFrequencyClamped(LinValue, LinearRange, OutputRange);
}

const float SAudioFrequencySlider::GetLinValue(const float OutputValue)
{
	return Audio::GetLinearFrequencyClamped(OutputValue, LinearRange, OutputRange);
}
