#include "SRacetrackDemoPanel.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RacetrackCircuit.h"
#include "SDemoWindow.h"
#include "SurfaceAccumulationSubsystem.h"
#include "Widgets/Input/SButton.h"
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
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeViewSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeCameraSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeCarSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSurfaceStateSection()
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
					TAttribute<bool>::CreateSP(this, &SRacetrackDemoPanel::IsFlyCameraInUse))
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

TSharedRef<SWidget> SRacetrackDemoPanel::MakeViewSection()
{
	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("View")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Channel")),
					MakeChoiceRow(
						{
							FText::FromString(TEXT("All")),
							FText::FromString(TEXT("Rubber")),
							FText::FromString(TEXT("Skid")),
							FText::FromString(TEXT("Marbles"))
						},
						[this]()
						{
							const ARacetrackCircuit* Track = Circuit.Get();

							return Track != nullptr && Track->GetSurfaceLook() == ESurfaceLook::Debug
								? static_cast<int32>(Track->GetDebugView())
								: INDEX_NONE;
						},
						[this](int32 Index)
						{
							ARacetrackCircuit* Track = Circuit.Get();

							if (Track == nullptr)
							{
								return;
							}

							if (Track->GetSurfaceLook() == ESurfaceLook::Asphalt)
							{
								Track->SetSurfaceLook(ESurfaceLook::Debug);
							}

							Track->SetDebugView(static_cast<ESurfaceDebugView>(Index));
						}))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Look")),
					MakeChoiceRow(
						{
							FText::FromString(TEXT("Debug")),
							FText::FromString(TEXT("Asphalt"))
						},
						[this]()
						{
							const ARacetrackCircuit* Track = Circuit.Get();
							return Track != nullptr ? static_cast<int32>(Track->GetSurfaceLook()) : INDEX_NONE;
						},
						[this](int32 Index)
						{
							if (ARacetrackCircuit* Track = Circuit.Get())
							{
								Track->SetSurfaceLook(static_cast<ESurfaceLook>(Index));
							}
						}))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Lighting")),
					MakeChoiceRow(
						{
							FText::FromString(TEXT("Map sun")),
							FText::FromString(TEXT("Low sun")),
							FText::FromString(TEXT("Overcast"))
						},
						[this]()
						{
							const ARacetrackCircuit* Track = Circuit.Get();
							return Track != nullptr ? static_cast<int32>(Track->GetSceneLighting()) : INDEX_NONE;
						},
						[this](int32 Index)
						{
							if (ARacetrackCircuit* Track = Circuit.Get())
							{
								Track->SetSceneLighting(static_cast<ESceneLighting>(Index));
							}
						}))
			]
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeCameraSection()
{
	const TAttribute<bool> IsFollowRowEnabled = TAttribute<bool>::CreateSP(this, &SRacetrackDemoPanel::IsFlyCameraInUse);

	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("Camera")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeCheckBox(
					FText::FromString(TEXT("Follow the leader")),
					[this]()
					{
						const ARacetrackCircuit* Track = Circuit.Get();
						return Track != nullptr && Track->IsFollowingTheLeader();
					},
					[this](bool bFollow)
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SetFollowTheLeader(bFollow);
						}
					},
					IsFollowRowEnabled)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeRow(
					FText::FromString(TEXT("Distance")),
					SNew(SDemoWindowValueBox)
					.Style(Style)
					.Value_Lambda([this]()
					{
						const ARacetrackCircuit* Track = Circuit.Get();
						return Track != nullptr ? Track->GetFollowDistance() : 0.0f;
					})
					.ValueText_Lambda([this]()
					{
						const ARacetrackCircuit* Track = Circuit.Get();

						return FText::FromString(FString::Printf(
							TEXT("%.0f cm"),
							Track != nullptr ? Track->GetFollowDistance() : 0.0f));
					})
					.OnValueCommitted_Lambda([this](float Centimetres)
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SetFollowDistance(Centimetres);
						}
					}),
					IsFollowRowEnabled)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Style->RowPadding)
			[
				MakeReadout([]()
				{
					return FText::FromString(TEXT("Hold the left mouse button to look. W A S D, Q E move"));
				})
			]
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeCarSection()
{
	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("Car")))
		[
			MakeButton(
				TAttribute<FText>::CreateLambda([this]()
				{
					const ARacetrackCircuit* Track = Circuit.Get();

					return FText::FromString(Track != nullptr && Track->IsDriving()
						? TEXT("Back to the fly camera")
						: TEXT("Drive the car"));
				}),
				[this]()
				{
					if (ARacetrackCircuit* Track = Circuit.Get())
					{
						Track->ToggleDriving();
					}
				})
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeSurfaceStateSection()
{
	return SNew(SDemoWindowSection)
		.Style(Style)
		.Title(FText::FromString(TEXT("Surface state")))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				MakeButton(
					FText::FromString(TEXT("Save...")),
					[this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->SaveSurfaceStateWithDialog();
						}
					})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
			[
				MakeButton(
					FText::FromString(TEXT("Load...")),
					[this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->LoadSurfaceStateWithDialog();
						}
					})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
			[
				MakeButton(
					FText::FromString(TEXT("Surface report")),
					[this]()
					{
						if (ARacetrackCircuit* Track = Circuit.Get())
						{
							Track->ReportSurfaceCoverage();
						}
					})
			]
		];
}

TSharedRef<SWidget> SRacetrackDemoPanel::MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClicked) const
{
	return SNew(SButton)
		.ButtonStyle(&Style->Button)
		.IsFocusable(false)
		.HAlign(HAlign_Center)
		.ContentPadding(Style->ButtonPadding)
		.OnClicked_Lambda([OnClicked]()
		{
			OnClicked();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(Style->Font)
			.Text(Label)
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
	TFunction<void(bool)> OnChanged,
	const TAttribute<bool>& IsCheckBoxEnabled) const
{
	return SNew(SCheckBox)
		.Style(&Style->CheckBox)
		.IsFocusable(false)
		.IsEnabled(IsCheckBoxEnabled)
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

TSharedRef<SWidget> SRacetrackDemoPanel::MakeChoiceRow(
	const TArray<FText>& Labels,
	TFunction<int32()> SelectedIndex,
	TFunction<void(int32)> Choose) const
{
	const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		Row->AddSlot()
		.FillWidth(1.0f)
		.Padding(FMargin(Index == 0 ? 0.0f : 2.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(&Style->ChoiceButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.IsChecked_Lambda([SelectedIndex, Index]()
			{
				return SelectedIndex() == Index ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Choose, Index](ECheckBoxState NewState)
			{
				if (NewState == ECheckBoxState::Checked)
				{
					Choose(Index);
				}
			})
			[
				SNew(STextBlock)
				.Font(Style->Font)
				.Text(Labels[Index])
			]
		];
	}

	return Row;
}

bool SRacetrackDemoPanel::IsFlyCameraInUse() const
{
	const ARacetrackCircuit* Track = Circuit.Get();
	return Track != nullptr && !Track->IsDriving();
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
