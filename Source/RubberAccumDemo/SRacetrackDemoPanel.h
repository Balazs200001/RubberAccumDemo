#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ARacetrackCircuit;
struct FDemoWindowStyle;

class SRacetrackDemoPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRacetrackDemoPanel)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(TSharedPtr<const FDemoWindowStyle>, Style)

		SLATE_ARGUMENT(TWeakObjectPtr<ARacetrackCircuit>, Circuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeSimulationSection();

	TSharedRef<SWidget> MakeSurfaceSection();

	TSharedRef<SWidget> MakeRow(
		const FText& Label,
		const TSharedRef<SWidget>& Control,
		const TAttribute<bool>& IsRowEnabled = true) const;

	TSharedRef<SWidget> MakeCheckBox(
		const FText& Label,
		TFunction<bool()> IsChecked,
		TFunction<void(bool)> OnChanged) const;

	TSharedRef<SWidget> MakeReadout(TFunction<FText()> Text) const;

	FText HalfLifeText() const;

	FText ApexSaturationText() const;

	FText StraightSaturationText() const;

	TSharedPtr<const FDemoWindowStyle> Style;

	TWeakObjectPtr<ARacetrackCircuit> Circuit;
};
