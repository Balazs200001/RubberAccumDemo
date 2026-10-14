#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
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

	const FSlateBrush* CollapsedArrow = nullptr;

	const FSlateBrush* ExpandedArrow = nullptr;

	const FSlateBrush* CloseCross = nullptr;

	float WindowWidth = 0.0f;

	FMargin WindowPadding;

	FMargin TitleBarPadding;

	FMargin ButtonPadding;

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
