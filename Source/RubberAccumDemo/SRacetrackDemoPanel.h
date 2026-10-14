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

	TSharedRef<SWidget> MakeViewSection();

	TSharedRef<SWidget> MakeCameraSection();

	TSharedRef<SWidget> MakeCarSection();

	TSharedRef<SWidget> MakeSurfaceStateSection();

	TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClicked) const;

	TSharedRef<SWidget> MakeRow(
		const FText& Label,
		const TSharedRef<SWidget>& Control,
		const TAttribute<bool>& IsRowEnabled = true) const;

	TSharedRef<SWidget> MakeCheckBox(
		const FText& Label,
		TFunction<bool()> IsChecked,
		TFunction<void(bool)> OnChanged,
		const TAttribute<bool>& IsCheckBoxEnabled = true) const;

	TSharedRef<SWidget> MakeChoiceRow(
		const TArray<FText>& Labels,
		TFunction<int32()> SelectedIndex,
		TFunction<void(int32)> Choose) const;

	bool IsFlyCameraInUse() const;

	TSharedRef<SWidget> MakeReadout(TFunction<FText()> Text) const;

	FText HalfLifeText() const;

	FText ApexSaturationText() const;

	FText StraightSaturationText() const;

	TSharedPtr<const FDemoWindowStyle> Style;

	TWeakObjectPtr<ARacetrackCircuit> Circuit;
};
