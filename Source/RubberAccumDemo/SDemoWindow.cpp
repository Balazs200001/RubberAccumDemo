#include "SDemoWindow.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace DemoWindowPalette
{
	FLinearColor FromImGui(uint8 Red, uint8 Green, uint8 Blue, uint8 Alpha = 255)
	{
		return FLinearColor::FromSRGBColor(FColor(Red, Green, Blue, Alpha));
	}
}

FDemoWindowStyle::FDemoWindowStyle()
	: Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
	, TextColour(DemoWindowPalette::FromImGui(255, 255, 255))
	, DisabledTextColour(DemoWindowPalette::FromImGui(128, 128, 128))
	, WindowWidth(360.0f)
	, WindowPadding(8.0f, 6.0f)
	, TitleBarPadding(4.0f, 2.0f)
	, ButtonPadding(6.0f, 2.0f)
	, SectionPadding(4.0f, 2.0f, 0.0f, 4.0f)
	, RowPadding(0.0f, 2.0f)
	, LabelWidth(90.0f)
	, CornerButtonOffset(16.0f, 16.0f)
{
	using DemoWindowPalette::FromImGui;

	WindowBody = FSlateRoundedBoxBrush(FromImGui(15, 15, 15, 240), 0.0f, FromImGui(110, 110, 128, 128), 1.0f);
	TitleBar = FSlateColorBrush(FromImGui(41, 74, 122));

	Button = FButtonStyle()
		.SetNormal(FSlateColorBrush(FromImGui(66, 150, 250, 102)))
		.SetHovered(FSlateColorBrush(FromImGui(66, 150, 250)))
		.SetPressed(FSlateColorBrush(FromImGui(15, 135, 250)))
		.SetDisabled(FSlateColorBrush(FromImGui(66, 150, 250, 40)))
		.SetNormalForeground(TextColour)
		.SetHoveredForeground(TextColour)
		.SetPressedForeground(TextColour)
		.SetDisabledForeground(DisabledTextColour)
		.SetNormalPadding(FMargin(0.0f))
		.SetPressedPadding(FMargin(0.0f));

	TitleBarButton = FButtonStyle(Button).SetNormal(FSlateNoResource());

	SectionHeader = FButtonStyle(Button)
		.SetNormal(FSlateColorBrush(FromImGui(66, 150, 250, 79)))
		.SetHovered(FSlateColorBrush(FromImGui(66, 150, 250, 204)))
		.SetPressed(FSlateColorBrush(FromImGui(66, 150, 250)));

	const FVector2f CheckBoxSize(16.0f, 16.0f);
	const FCheckBoxStyle& CoreCheckBox = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox");

	FSlateBrush CheckMark = CoreCheckBox.CheckedImage;
	CheckMark.TintColor = FSlateColor(FromImGui(66, 150, 250));

	CheckBox = FCheckBoxStyle(CoreCheckBox)
		.SetBackgroundImage(FSlateRoundedBoxBrush(FromImGui(41, 74, 122, 138), 0.0f, CheckBoxSize))
		.SetBackgroundHoveredImage(FSlateRoundedBoxBrush(FromImGui(66, 150, 250, 102), 0.0f, CheckBoxSize))
		.SetBackgroundPressedImage(FSlateRoundedBoxBrush(FromImGui(66, 150, 250, 171), 0.0f, CheckBoxSize))
		.SetCheckedImage(CheckMark)
		.SetCheckedHoveredImage(CheckMark)
		.SetCheckedPressedImage(CheckMark)
		.SetForegroundColor(TextColour)
		.SetHoveredForegroundColor(TextColour)
		.SetPressedForegroundColor(TextColour)
		.SetCheckedForegroundColor(TextColour)
		.SetCheckedHoveredForegroundColor(TextColour)
		.SetCheckedPressedForegroundColor(TextColour)
		.SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));

	ValueBox = FSpinBoxStyle()
		.SetBackgroundBrush(FSlateColorBrush(FromImGui(41, 74, 122, 138)))
		.SetHoveredBackgroundBrush(FSlateColorBrush(FromImGui(66, 150, 250, 102)))
		.SetActiveBackgroundBrush(FSlateColorBrush(FromImGui(66, 150, 250, 171)))
		.SetActiveFillBrush(FSlateNoResource())
		.SetHoveredFillBrush(FSlateNoResource())
		.SetInactiveFillBrush(FSlateNoResource())
		.SetArrowsImage(FSlateNoResource())
		.SetForegroundColor(TextColour)
		.SetTextPadding(FMargin(4.0f, 2.0f))
		.SetInsetPadding(FMargin(0.0f));

	CollapsedArrow = FCoreStyle::Get().GetBrush("TreeArrow_Collapsed");
	ExpandedArrow = FCoreStyle::Get().GetBrush("TreeArrow_Expanded");
	CloseCross = FCoreStyle::Get().GetBrush("Icons.X");
}

void SDemoWindow::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	check(Style.IsValid());

	ChildSlot
	[
		SNew(SConstraintCanvas)
		+ SConstraintCanvas::Slot()
			.Expose(WindowSlot)
			.Anchors(FAnchors(1.0f, 0.0f))
			.Alignment(FVector2D(1.0f, 0.0f))
			.AutoSize(true)
			.Offset(FMargin(-InArgs._OffsetFromTopRight.X, InArgs._OffsetFromTopRight.Y, 0.0f, 0.0f))
		[
			SAssignNew(Frame, SBox)
			.WidthOverride(Style->WindowWidth)
			.Visibility_Lambda([this]()
			{
				return bOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(SBorder)
				.BorderImage(&Style->WindowBody)
				.Padding(FMargin(1.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeTitleBar(InArgs._Title)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.Padding(Style->WindowPadding)
						.Visibility_Lambda([this]()
						{
							return bCollapsed ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
						})
						[
							InArgs._Content.Widget
						]
					]
				]
			]
		]
		+ SConstraintCanvas::Slot()
			.Anchors(FAnchors(0.0f, 0.0f))
			.Alignment(FVector2D(0.0f, 0.0f))
			.AutoSize(true)
			.Offset(FMargin(Style->CornerButtonOffset.X, Style->CornerButtonOffset.Y, 0.0f, 0.0f))
		[
			SNew(SButton)
			.ButtonStyle(&Style->Button)
			.IsFocusable(false)
			.ContentPadding(Style->ButtonPadding)
			.Visibility_Lambda([this]()
			{
				return bOpen ? EVisibility::Collapsed : EVisibility::Visible;
			})
			.OnClicked_Lambda([this]()
			{
				SetOpen(true);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(Style->Font)
				.Text(InArgs._ClosedButtonText)
			]
		]
	];
}

TSharedRef<SWidget> SDemoWindow::MakeTitleBar(const FText& Title)
{
	return SAssignNew(TitleBar, SBorder)
		.BorderImage(&Style->TitleBar)
		.Padding(Style->TitleBarPadding)
		.OnMouseButtonDown(this, &SDemoWindow::HandleTitleBarMouseButtonDown)
		.OnMouseDoubleClick(this, &SDemoWindow::HandleTitleBarMouseButtonDown)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&Style->TitleBarButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(0.0f))
				.OnClicked(this, &SDemoWindow::HandleCollapseClicked)
				[
					SNew(SImage)
					.Image_Lambda([this]()
					{
						return bCollapsed ? Style->CollapsedArrow : Style->ExpandedArrow;
					})
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f))
			[
				SNew(STextBlock)
				.Font(Style->Font)
				.ColorAndOpacity(Style->TextColour)
				.Text(Title)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&Style->TitleBarButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(0.0f))
				.OnClicked(this, &SDemoWindow::HandleCloseClicked)
				[
					SNew(SImage)
					.Image(Style->CloseCross)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void SDemoWindow::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
}

FReply SDemoWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SDemoWindow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

FReply SDemoWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Handled();
}

FReply SDemoWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || !HasMouseCapture() || WindowSlot == nullptr || !Frame.IsValid() || !TitleBar.IsValid())
	{
		return FReply::Unhandled();
	}

	const FVector2f Now = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2f Start = MyGeometry.AbsoluteToLocal(DragStartPosition);
	const FVector2f Moved = Now - Start;
	const FVector2f CanvasSize = MyGeometry.GetLocalSize();

	const float LeftmostOffset = FMath::Min(static_cast<float>(Frame->GetDesiredSize().X) - CanvasSize.X, 0.0f);
	const float LowestOffset = FMath::Max(CanvasSize.Y - static_cast<float>(TitleBar->GetDesiredSize().Y), 0.0f);

	FMargin Offset = DragStartOffset;
	Offset.Left = FMath::Clamp(DragStartOffset.Left + Moved.X, LeftmostOffset, 0.0f);
	Offset.Top = FMath::Clamp(DragStartOffset.Top + Moved.Y, 0.0f, LowestOffset);

	WindowSlot->SetOffset(Offset);

	return FReply::Handled();
}

void SDemoWindow::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bDragging = false;
}

FReply SDemoWindow::HandleTitleBarMouseButtonDown(const FGeometry& TitleBarGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || WindowSlot == nullptr)
	{
		return FReply::Handled();
	}

	bDragging = true;
	DragStartPosition = MouseEvent.GetScreenSpacePosition();
	DragStartOffset = WindowSlot->GetOffset();

	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SDemoWindow::HandleCollapseClicked()
{
	bCollapsed = !bCollapsed;
	return FReply::Handled();
}

FReply SDemoWindow::HandleCloseClicked()
{
	SetOpen(false);
	return FReply::Handled();
}

void SDemoWindowSection::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	check(Style.IsValid());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Style->RowPadding)
		[
			SNew(SButton)
			.ButtonStyle(&Style->SectionHeader)
			.IsFocusable(false)
			.ContentPadding(Style->ButtonPadding)
			.OnClicked_Lambda([this]()
			{
				bExpanded = !bExpanded;
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image_Lambda([this]()
					{
						return bExpanded ? Style->ExpandedArrow : Style->CollapsedArrow;
					})
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Font(Style->Font)
					.Text(InArgs._Title)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(Style->SectionPadding)
			.Visibility_Lambda([this]()
			{
				return bExpanded ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			})
			[
				InArgs._Content.Widget
			]
		]
	];
}

void SDemoWindowValueBox::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Value = InArgs._Value;
	ValueText = InArgs._ValueText;
	OnValueCommitted = InArgs._OnValueCommitted;
	check(Style.IsValid());

	const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	if (InArgs._OnStepDown.IsBound())
	{
		Row->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		[
			MakeStepButton(FText::FromString(TEXT("-")), InArgs._OnStepDown)
		];
	}

	Row->AddSlot()
	.FillWidth(1.0f)
	[
		SNew(SSpinBox<float>)
		.Style(&Style->ValueBox)
		.Font(Style->Font)
		.Value(Value)
		.OnGetDisplayValue_Lambda([this](float) -> TOptional<FText>
		{
			return ValueText.Get();
		})
		.EnableSlider(false)
		.EnableWheel(false)
		.Justification(ETextJustify::Center)
		.OnValueCommitted(this, &SDemoWindowValueBox::HandleValueCommitted)
	];

	if (InArgs._OnStepUp.IsBound())
	{
		Row->AddSlot()
		.AutoWidth()
		.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
		[
			MakeStepButton(FText::FromString(TEXT("+")), InArgs._OnStepUp)
		];
	}

	ChildSlot
	[
		Row
	];
}

TSharedRef<SWidget> SDemoWindowValueBox::MakeStepButton(const FText& Label, const FSimpleDelegate& OnStep) const
{
	return SNew(SButton)
		.ButtonStyle(&Style->Button)
		.IsFocusable(false)
		.ContentPadding(Style->ButtonPadding)
		.OnClicked_Lambda([OnStep]()
		{
			OnStep.ExecuteIfBound();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(Style->Font)
			.Text(Label)
		];
}

void SDemoWindowValueBox::HandleValueCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnCleared || FMath::IsNearlyEqual(NewValue, Value.Get(), UE_KINDA_SMALL_NUMBER))
	{
		return;
	}

	OnValueCommitted.ExecuteIfBound(NewValue);
}
