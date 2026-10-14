#include "SRacetrackDemoPanel.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RacetrackCircuit.h"
#include "SDemoWindow.h"
#include "SurfaceAccumulationSubsystem.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SRacetrackDemoPanel::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Circuit = InArgs._Circuit;
	check(Style.IsValid());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSimulationSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSurfaceSection()
		]
	];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeSimulationSection()
{
	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("Simulation")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeCheckBox(
					FText::FromString(TEXT("Field running")),
					[this]()
					{
						const ARacetrackCircuit* Track = Circuit.Get();
						return Track != nullptr && !Track->IsFieldPaused();
					},
					[this](bool bRunning)
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SetFieldPaused(!bRunning);
						}
					})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Time")),
					SNew(SDemoWindowValueBox)
					.Style(Style)
					.Value_Lambda([this]()
					{
						return Circuit.IsValid() ? UGameplayStatics::GetGlobalTimeDilation(Circuit.Get()) : 1.0f;
					})
					.ValueText_Lambda([this]()
					{
						return FText::FromString(FString::Printf(
							TEXT("%.2fx"),
							Circuit.IsValid() ? UGameplayStatics::GetGlobalTimeDilation(Circuit.Get()) : 1.0f));
					})
					.OnStepDown_Lambda([this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SlowDownTime();
						}
					})
					.OnStepUp_Lambda([this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SpeedUpTime();
						}
					})
					.OnValueCommitted_Lambda([this](float Dilation)
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->ApplyTimeDilation(Dilation);
						}
					}),
					TAttribute<bool>::CreateLambda([this]()
					{
						const ARacetrackCircuit* Track = Circuit.Get();
						return Track != nullptr && !Track->IsDriving();
					}))
			]
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeSurfaceSection()
{
	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("Surface")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Deposition")),
					SNew(SDemoWindowValueBox)
					.Style(Style)
					.Value_Lambda([]()
					{
						return USurfaceAccumulationSubsystem::GetDepositionScale();
					})
					.ValueText_Lambda([]()
					{
						return FText::FromString(FString::Printf(
							TEXT("%.2fx"),
							USurfaceAccumulationSubsystem::GetDepositionScale()));
					})
					.OnStepDown_Lambda([this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->DepositSlower();
						}
					})
					.OnStepUp_Lambda([this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->DepositFaster();
						}
					})
					.OnValueCommitted_Lambda([this](float Scale)
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->ApplyDepositionScale(Scale);
						}
					}))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Decay")),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
					[
						MakeCheckBox(
							FText::GetEmpty(),
							[]()
							{
								return USurfaceAccumulationSubsystem::GetDecayRate() > 0.0f;
							},
							[this](bool bDecaying)
							{
								if (ARacetrackCircuit* Track = Circuit.Get())
								{
									Track->ApplyDecayRate(bDecaying ? 1.0f : 0.0f);
								}
							})
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SDemoWindowValueBox)
						.Style(Style)
						.Value_Lambda([]()
						{
							return USurfaceAccumulationSubsystem::GetDecayRate();
						})
						.ValueText_Lambda([]()
						{
							const float Rate = USurfaceAccumulationSubsystem::GetDecayRate();

							return Rate > 0.0f
								? FText::FromString(FString::Printf(TEXT("%.2fx"), Rate))
								: FText::FromString(TEXT("off"));
						})
						.OnStepDown_Lambda([this]()
						{
							if (ARacetrackCircuit* Track = Circuit.Get())
							{
								Track->DecaySlower();
							}
						})
						.OnStepUp_Lambda([this]()
						{
							if (ARacetrackCircuit* Track = Circuit.Get())
							{
								Track->DecayFaster();
							}
						})
						.OnValueCommitted_Lambda([this](float Rate)
						{
							if (ARacetrackCircuit* Track = Circuit.Get())
							{
								Track->ApplyDecayRate(Rate);
							}
						})
					])
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeReadout([this]()
				{
					return HalfLifeText();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeReadout([this]()
				{
					return ApexSaturationText();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeReadout([this]()
				{
					return StraightSaturationText();
				})
			]
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeRow(
	const FText& Label,
	const TSharedRef<SWidget>& Control,
	const TAttribute<bool>& IsRowEnabled) const
{
	return SNew(SHorizontalBox)
		.IsEnabled(IsRowEnabled)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(Style->LabelWidth)
			[
				SNew(STextBlock)
				.Font(Style->Font)
				.ColorAndOpacity(Style->TextColour)
				.Text(Label)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			Control
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeCheckBox(
	const FText& Label,
	TFunction<bool()> IsChecked,
	TFunction<void(bool)> OnChanged) const
{
	return SNew(SCheckBox)
		.Style(&Style->CheckBox)
		.IsFocusable(false)
		.IsChecked_Lambda([IsChecked]()
		{
			return IsChecked() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([OnChanged](ECheckBoxState NewState)
		{
			OnChanged(NewState == ECheckBoxState::Checked);
		})
		[
			SNew(STextBlock)
			.Font(Style->Font)
			.Text(Label)
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeReadout(TFunction<FText()> Text) const
{
	return SNew(STextBlock)
		.Font(Style->Font)
		.ColorAndOpacity(Style->DisabledTextColour)
		.Text_Lambda(MoveTemp(Text));
}

FText SRacetrackDemoPanel::HalfLifeText() const
{
	const ARacetrackCircuit* Track = Circuit.Get();
	UWorld* World = Track != nullptr ? Track->GetWorld() : nullptr;
	const USurfaceAccumulationSubsystem* Subsystem =
		World != nullptr ? World->GetSubsystem<USurfaceAccumulationSubsystem>() : nullptr;
	const float Rate = USurfaceAccumulationSubsystem::GetDecayRate();

	if (Track == nullptr || Subsystem == nullptr || Rate <= 0.0f)
	{
		return FText::FromString(TEXT("Half-life: none, nothing fades"));
	}

	return FText::FromString(FString::Printf(
		TEXT("Half-life: %.1f laps, %.0f s of track time"),
		Track->GetDecayHalfLifeInLaps() / Rate,
		Subsystem->GetDecayHalfLifeSeconds() / Rate));
}

FText SRacetrackDemoPanel::ApexSaturationText() const
{
	const ARacetrackCircuit* Track = Circuit.Get();
	const TOptional<FLapsToSaturation> Estimate =
		Track != nullptr ? Track->EstimateLapsToSaturation() : TOptional<FLapsToSaturation>();

	if (!Estimate.IsSet())
	{
		return FText::FromString(TEXT("Apex: no estimate"));
	}

	return FText::FromString(FString::Printf(
		TEXT("Apex: 63 %% in %.0f laps, 95 %% in %.0f"),
		Estimate->ApexLapsToSixtyThreePercent(),
		Estimate->ApexLapsToNinetyFivePercent()));
}

FText SRacetrackDemoPanel::StraightSaturationText() const
{
	const ARacetrackCircuit* Track = Circuit.Get();
	const TOptional<FLapsToSaturation> Estimate =
		Track != nullptr ? Track->EstimateLapsToSaturation() : TOptional<FLapsToSaturation>();

	if (!Estimate.IsSet())
	{
		return FText::FromString(TEXT("Straight: no estimate"));
	}

	return FText::FromString(FString::Printf(
		TEXT("Straight: 63 %% in %.0f laps, 95 %% in %.0f"),
		Estimate->StraightLapsToSixtyThreePercent(),
		Estimate->StraightLapsToNinetyFivePercent()));
}
