// Copyright 2026 Balázs Fábián. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SCompoundWidget.h"

struct FDemoWindowStyle
{
	FDemoWindowStyle();

	FSlateFontInfo Font;

	FLinearColor TextColour;

	FLinearColor DisabledTextColour;

	FSlateBrush WindowBody;

	FSlateBrush TitleBar;

	FButtonStyle Button;

	FButtonStyle TitleBarButton;

	FButtonStyle SectionHeader;

	FCheckBoxStyle CheckBox;

	FCheckBoxStyle ChoiceButton;

	FSpinBoxStyle ValueBox;

	const FSlateBrush* CollapsedArrow = nullptr;

	const FSlateBrush* ExpandedArrow = nullptr;

	const FSlateBrush* CloseCross = nullptr;

	float WindowWidth = 0.0f;

	FMargin WindowPadding;

	FMargin TitleBarPadding;

	FMargin ButtonPadding;

	FMargin SectionPadding;

	FMargin RowPadding;

	float LabelWidth = 0.0f;

	FVector2f CornerButtonOffset = FVector2f::ZeroVector;
};

class SDemoWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoWindow)
		: _OffsetFromTopRight(FVector2f::ZeroVector)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(TSharedPtr<const FDemoWindowStyle>, Style)

		SLATE_ARGUMENT(FText, Title)

		SLATE_ARGUMENT(FText, ClosedButtonText)

		SLATE_ARGUMENT(FVector2f, OffsetFromTopRight)

		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetOpen(bool bInOpen);

	bool IsOpen() const
	{
		return bOpen;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	TSharedRef<SWidget> MakeTitleBar(const FText& Title);

	FReply HandleTitleBarMouseButtonDown(const FGeometry& TitleBarGeometry, const FPointerEvent& MouseEvent);

	FReply HandleCollapseClicked();

	FReply HandleCloseClicked();

	TSharedPtr<const FDemoWindowStyle> Style;

	SConstraintCanvas::FSlot* WindowSlot = nullptr;

	TSharedPtr<SWidget> Frame;

	TSharedPtr<SWidget> TitleBar;

	bool bOpen = true;

	bool bCollapsed = false;

	bool bDragging = false;

	FVector2f DragStartPosition = FVector2f::ZeroVector;

	FMargin DragStartOffset;
};

class SDemoWindowSection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoWindowSection)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(TSharedPtr<const FDemoWindowStyle>, Style)

		SLATE_ARGUMENT(FText, Title)

		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<const FDemoWindowStyle> Style;

	bool bExpanded = true;
};

class SDemoWindowValueBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoWindowValueBox)
		: _Value(0.0f)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(TSharedPtr<const FDemoWindowStyle>, Style)

		SLATE_ATTRIBUTE(float, Value)

		SLATE_ATTRIBUTE(FText, ValueText)

		SLATE_EVENT(FSimpleDelegate, OnStepDown)

		SLATE_EVENT(FSimpleDelegate, OnStepUp)

		SLATE_EVENT(FOnFloatValueChanged, OnValueCommitted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeStepButton(const FText& Label, const FSimpleDelegate& OnStep) const;

	void HandleValueCommitted(float NewValue, ETextCommit::Type CommitType);

	TSharedPtr<const FDemoWindowStyle> Style;

	TAttribute<float> Value;

	TAttribute<FText> ValueText;

	FOnFloatValueChanged OnValueCommitted;
};
