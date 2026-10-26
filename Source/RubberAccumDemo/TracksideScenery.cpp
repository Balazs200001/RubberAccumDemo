// Copyright 2026 Balázs Fábián. All Rights Reserved.

#include "TracksideScenery.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RacetrackCircuit.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTracksideScenery, Log, All);

namespace TracksideColours
{
	const FLinearColor WhiteLine(0.80f, 0.80f, 0.78f);
	const FLinearColor KerbRed(0.60f, 0.015f, 0.02f);
	const FLinearColor KerbYellow(0.95f, 0.65f, 0.02f);
	const FLinearColor GreenStrip(0.03f, 0.30f, 0.06f);
	const FLinearColor RunOffAsphalt(0.16f, 0.16f, 0.16f);
	const FLinearColor RunOffYellowStripe(0.90f, 0.62f, 0.02f);
	const FLinearColor RunOffWhiteStripe(0.80f, 0.80f, 0.78f);
	const FLinearColor Gravel(0.35f, 0.26f, 0.17f);
	const FLinearColor VergeLight(0.09f, 0.25f, 0.04f);
	const FLinearColor VergeDark(0.07f, 0.20f, 0.03f);
	const FLinearColor Skirt(0.05f, 0.14f, 0.025f);
	const FLinearColor TerrainGrass(0.065f, 0.18f, 0.03f);
}

namespace TracksideLayout
{
	constexpr int32 AuthoredSpaPointCount = 78;

	constexpr double LeftSide = -1.0;

	constexpr double RightSide = 1.0;

	struct FPointRange
	{
		const TCHAR* Name;

		int32 FirstPoint;

		int32 LastPoint;
	};

	const FPointRange GravelCorners[] = {
		{ TEXT("Bruxelles"), 30, 34 },
		{ TEXT("Pouhon"), 39, 42 },
		{ TEXT("Stavelot"), 52, 54 },
	};

	double DistanceAtPoint(const USplineComponent& Spline, int32 Point)
	{
		return Spline.GetDistanceAlongSplineAtSplinePoint(Point % Spline.GetNumberOfSplinePoints());
	}

	bool IsWithin(double Distance, double From, double To)
	{
		return From <= To ? (Distance >= From && Distance <= To) : (Distance >= From || Distance <= To);
	}
}

namespace TracksideRings
{
	double WrapDistance(double Distance, double Length)
	{
		const double Wrapped = FMath::Fmod(Distance, Length);

		return Wrapped < 0.0 ? Wrapped + Length : Wrapped;
	}

	double SignedCurvatureAt(const USplineComponent& Spline, double Distance, double Length, double SampleDistance)
	{
		const FVector Behind = Spline.GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance - SampleDistance, Length)), ESplineCoordinateSpace::World);

		const FVector Ahead = Spline.GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance + SampleDistance, Length)), ESplineCoordinateSpace::World);

		const double TurnAngle = FMath::Atan2(
			FVector::CrossProduct(Behind, Ahead).Z, FVector::DotProduct(Behind, Ahead));

		return TurnAngle / (2.0 * SampleDistance);
	}

	int32 RingsWithin(double Distance, double Spacing, int32 Count)
	{
		return FMath::Clamp(FMath::RoundToInt32(Distance / FMath::Max(Spacing, 1.0)), 0, FMath::Max(Count / 2 - 1, 0));
	}

	int32 CircularGap(int32 A, int32 B, int32 Count)
	{
		const int32 Gap = FMath::Abs(A - B) % Count;

		return FMath::Min(Gap, Count - Gap);
	}

	TArray<double> SmoothedAround(const TArray<double>& Values, int32 Radius)
	{
		const int32 Count = Values.Num();
		TArray<double> Smoothed;
		Smoothed.SetNumZeroed(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			double Sum = 0.0;

			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				Sum += Values[(Index + Offset + Count) % Count];
			}

			Smoothed[Index] = Sum / (2 * Radius + 1);
		}

		return Smoothed;
	}

	TArray<double> ErodedAround(const TArray<double>& Values, int32 Radius)
	{
		const int32 Count = Values.Num();
		TArray<double> Eroded;
		Eroded.SetNumZeroed(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			double Lowest = Values[Index];

			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				Lowest = FMath::Min(Lowest, Values[(Index + Offset + Count) % Count]);
			}

			Eroded[Index] = Lowest;
		}

		return Eroded;
	}

	struct FRingGrid
	{
		FRingGrid(const TArray<FTracksideRing>& InRings, double InCellSize, int32 Stride = 1)
			: Rings(InRings)
			, CellSize(InCellSize)
		{
			for (int32 Index = 0; Index < Rings.Num(); Index += FMath::Max(Stride, 1))
			{
				Cells.FindOrAdd(CellOf(Rings[Index].RoadCentre)).Add(Index);
			}
		}

		FIntPoint CellOf(const FVector& Location) const
		{
			return FIntPoint(FMath::FloorToInt32(Location.X / CellSize), FMath::FloorToInt32(Location.Y / CellSize));
		}

		template <typename FunctionType>
		void ForEachNear(const FVector& Location, int32 Reach, FunctionType&& Function) const
		{
			const FIntPoint Centre = CellOf(Location);

			for (int32 X = -Reach; X <= Reach; ++X)
			{
				for (int32 Y = -Reach; Y <= Reach; ++Y)
				{
					if (const TArray<int32>* Members = Cells.Find(Centre + FIntPoint(X, Y)))
					{
						for (const int32 Ring : *Members)
						{
							Function(Ring);
						}
					}
				}
			}
		}

		int32 NearestRing(const FVector& Location, int32 Reach) const
		{
			int32 Nearest = INDEX_NONE;
			double NearestDistanceSquared = TNumericLimits<double>::Max();

			ForEachNear(Location, Reach, [&](int32 Ring)
			{
				const double DistanceSquared = FVector::DistSquaredXY(Rings[Ring].RoadCentre, Location);

				if (DistanceSquared < NearestDistanceSquared)
				{
					NearestDistanceSquared = DistanceSquared;
					Nearest = Ring;
				}
			});

			return Nearest;
		}

		const TArray<FTracksideRing>& Rings;

		double CellSize;

		TMap<FIntPoint, TArray<int32>> Cells;
	};
}

namespace TracksideMesh
{
	constexpr double ThinnestStrip = 0.5;

	struct FSection
	{
		void AddQuadFacing(
			const FVector& HereInner,
			const FVector& HereOuter,
			const FVector& NextInner,
			const FVector& NextOuter,
			const FVector& Facing,
			const FLinearColor& Colour)
		{
			const int32 Base = Vertices.Num();
			const FVector Normal = Facing.GetSafeNormal();

			Vertices.Append({ HereInner, HereOuter, NextInner, NextOuter });
			Normals.Append({ Normal, Normal, Normal, Normal });
			Colours.Append({ Colour, Colour, Colour, Colour });

			const FVector WoundNormal = FVector::CrossProduct(NextInner - HereOuter, NextOuter - HereInner);

			if (FVector::DotProduct(WoundNormal, Facing) >= 0.0)
			{
				Triangles.Append({ Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2 });
			}
			else
			{
				Triangles.Append({ Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3 });
			}
		}

		int32 AddVertex(const FVector& Position, const FVector& Normal, const FLinearColor& Colour)
		{
			Normals.Add(Normal.GetSafeNormal());
			Colours.Add(Colour);
			return Vertices.Add(Position);
		}

		void AddTriangleFacing(int32 A, int32 B, int32 C, const FVector& Facing)
		{
			const FVector WoundNormal = FVector::CrossProduct(Vertices[C] - Vertices[A], Vertices[B] - Vertices[A]);

			if (FVector::DotProduct(WoundNormal, Facing) >= 0.0)
			{
				Triangles.Append({ A, B, C });
			}
			else
			{
				Triangles.Append({ A, C, B });
			}
		}

		void MoveIntoLocalSpace(const FTransform& WorldToLocal)
		{
			for (FVector& Vertex : Vertices)
			{
				Vertex = WorldToLocal.TransformPosition(Vertex);
			}

			for (FVector& Normal : Normals)
			{
				Normal = WorldToLocal.TransformVectorNoScale(Normal);
			}
		}

		int32 TriangleCount() const
		{
			return Triangles.Num() / 3;
		}

		TArray<FVector> Vertices;

		TArray<int32> Triangles;

		TArray<FVector> Normals;

		TArray<FLinearColor> Colours;
	};
}

using TracksideLayout::LeftSide;
using TracksideLayout::RightSide;

double FTracksideTerrain::LowestAround(const FVector& Location) const
{
	const int32 IndexX = FMath::Clamp(FMath::FloorToInt32((Location.X - Origin.X) / CellSize), 0, CountX - 2);
	const int32 IndexY = FMath::Clamp(FMath::FloorToInt32((Location.Y - Origin.Y) / CellSize), 0, CountY - 2);

	return FMath::Min(
		FMath::Min(HeightAt(IndexX, IndexY), HeightAt(IndexX + 1, IndexY)),
		FMath::Min(HeightAt(IndexX, IndexY + 1), HeightAt(IndexX + 1, IndexY + 1)));
}

ATracksideScenery::ATracksideScenery()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	bRunConstructionScriptOnDrag = false;
#endif

	SceneryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneryRoot"));
	SetRootComponent(SceneryRoot);

	Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(SceneryRoot);
	Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ground->SetCanEverAffectNavigation(false);
	Ground->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TracksideMaterial(
		TEXT("/Game/Trackside/M_Trackside.M_Trackside"));

	if (TracksideMaterial.Succeeded())
	{
		SceneryMaterial = TracksideMaterial.Object;
	}
}

void ATracksideScenery::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Rebuild();
}

void ATracksideScenery::Rebuild()
{
	ClearScenery();

	if (!CanDressTheCircuit())
	{
		UE_LOG(LogTracksideScenery, Warning,
			TEXT("%s builds nothing: it needs Circuit set to a circuit on the Spa layout, with its %d authored points"),
			*GetName(),
			TracksideLayout::AuthoredSpaPointCount);
		return;
	}

	const TArray<FTracksideRing> Rings = SampleRings();

	TArray<FTracksideBands> Left;
	TArray<FTracksideBands> Right;
	LayOutBands(Rings, Left, Right);

	const FTracksideTerrain Terrain = ShapeTerrain(Rings, Left, Right);
	BuildGround(Rings, Left, Right, Terrain);
	BuildTerrain(Terrain);
}

void ATracksideScenery::ClearScenery()
{
	if (Ground != nullptr)
	{
		Ground->ClearAllMeshSections();
	}
}

bool ATracksideScenery::CanDressTheCircuit() const
{
	if (Circuit == nullptr || Ground == nullptr || Circuit->GetLayout() != ETrackLayout::Spa)
	{
		return false;
	}

	const USplineComponent* Spline = Circuit->GetCentreline();

	return Spline != nullptr
		&& Spline->GetNumberOfSplinePoints() == TracksideLayout::AuthoredSpaPointCount
		&& Circuit->GetTrackLength() > UE_DOUBLE_SMALL_NUMBER
		&& Circuit->GetRoadSegmentCount() >= 3;
}

TArray<FTracksideRing> ATracksideScenery::SampleRings() const
{
	const USplineComponent& Spline = *Circuit->GetCentreline();
	const double Length = Circuit->GetTrackLength();
	const int32 SegmentCount = Circuit->GetRoadSegmentCount();
	const double RoadHeight = Circuit->GetRoadHeightAboveSpline();

	TArray<FTracksideRing> Rings;
	Rings.SetNum(SegmentCount);

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const double AlongTrack = static_cast<double>(Index) / SegmentCount;
		const double Distance = Length * AlongTrack;
		const float DistanceFloat = static_cast<float>(FMath::Min(Distance, Length));

		FTracksideRing& Ring = Rings[Index];
		Ring.Distance = Distance;
		Ring.Up = Spline.GetUpVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		Ring.Right = Spline.GetRightVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		Ring.RoadCentre = Spline.GetLocationAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World)
			+ Ring.Up * RoadHeight;
		Ring.SignedCurvature = TracksideRings::SignedCurvatureAt(Spline, Distance, Length, CurvatureSampleDistance);
	}

	return Rings;
}

void ATracksideScenery::LayOutBands(
	const TArray<FTracksideRing>& Rings,
	TArray<FTracksideBands>& OutLeft,
	TArray<FTracksideBands>& OutRight) const
{
	using namespace TracksideRings;

	const int32 Count = Rings.Num();
	const USplineComponent& Spline = *Circuit->GetCentreline();
	const double Spacing = Circuit->GetTrackLength() / Count;
	const double HalfWidth = Circuit->GetTrackWidth() * 0.5;

	TArray<double> Curvature;
	Curvature.Reserve(Count);

	for (const FTracksideRing& Ring : Rings)
	{
		Curvature.Add(Ring.SignedCurvature);
	}

	const TArray<double> CornerCurvature = SmoothedAround(Curvature, RingsWithin(CornerSmoothingDistance, Spacing, Count));

	const double RunOffStartCurvature = 1.0 / RunOffStartRadius;
	const double RunOffFullCurvature = FMath::Max(1.0 / RunOffFullRadius, RunOffStartCurvature * 1.001);

	TArray<double> LeftRunOffShare;
	TArray<double> RightRunOffShare;
	LeftRunOffShare.Reserve(Count);
	RightRunOffShare.Reserve(Count);

	for (const double Turning : CornerCurvature)
	{
		const double Share = FMath::Clamp(
			(FMath::Abs(Turning) - RunOffStartCurvature) / (RunOffFullCurvature - RunOffStartCurvature), 0.0, 1.0);

		LeftRunOffShare.Add(Turning > 0.0 ? Share : 0.0);
		RightRunOffShare.Add(Turning < 0.0 ? Share : 0.0);
	}

	const int32 RunOffRadius = RingsWithin(RunOffSmoothingDistance, Spacing, Count);
	LeftRunOffShare = SmoothedAround(LeftRunOffShare, RunOffRadius);
	RightRunOffShare = SmoothedAround(RightRunOffShare, RunOffRadius);

	TArray<TPair<double, double>> GravelSpans;

	for (const TracksideLayout::FPointRange& Corner : TracksideLayout::GravelCorners)
	{
		GravelSpans.Emplace(
			TracksideLayout::DistanceAtPoint(Spline, Corner.FirstPoint),
			TracksideLayout::DistanceAtPoint(Spline, Corner.LastPoint));
	}

	const double WidestBands = WhiteLineWidth + KerbWidth + GreenStripWidth + MaxRunOffWidth + VergeWidth;
	const FRingGrid Grid(Rings, 1000.0);
	const int32 Reach = FMath::CeilToInt32((HalfWidth + WidestBands + ClearanceMargin) / Grid.CellSize);
	const int32 OwnRings = RingsWithin(OwnTrackWindow, Spacing, Count);
	const int32 SmoothingRadius = RingsWithin(WidthSmoothingDistance, Spacing, Count);

	const auto ClearWidth = [&](int32 Index, double Side)
	{
		const FVector Edge = Rings[Index].RoadCentre + Rings[Index].Right * (Side * HalfWidth);
		const FVector Outward = Rings[Index].Right * Side;

		for (double Width = 0.0; Width <= WidestBands; Width += ClearanceStep)
		{
			const int32 Nearest = Grid.NearestRing(Edge + Outward * (Width + ClearanceMargin), Reach);

			if (Nearest != INDEX_NONE && CircularGap(Nearest, Index, Count) > OwnRings)
			{
				return FMath::Max(0.0, Width - ClearanceStep);
			}
		}

		return WidestBands;
	};

	int32 NarrowedRingSides = 0;

	const auto LayOutSide = [&](double Side, const TArray<double>& RunOffShare, TArray<FTracksideBands>& OutBands)
	{
		TArray<FTracksideBands> Wanted;
		TArray<double> Clear;
		Wanted.SetNum(Count);
		Clear.SetNum(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double InsideTurning = Side * CornerCurvature[Index];
			const bool bKerb = InsideTurning > 1.0 / InsideKerbRadius || -InsideTurning > 1.0 / OutsideKerbRadius;

			FTracksideBands& Bands = Wanted[Index];
			Bands.Line = WhiteLineWidth;
			Bands.Kerb = bKerb ? KerbWidth : 0.0;
			Bands.Green = bKerb ? GreenStripWidth : 0.0;
			Bands.RunOff = RunOffShare[Index] * MaxRunOffWidth;
			Bands.Verge = FMath::Lerp(VergeWidth, VergeBehindRunOffWidth, RunOffShare[Index]);

			for (const TPair<double, double>& Span : GravelSpans)
			{
				Bands.bGravel |= TracksideLayout::IsWithin(Rings[Index].Distance, Span.Key, Span.Value);
			}

			Clear[Index] = ClearWidth(Index, Side);
		}

		const TArray<double> Available = SmoothedAround(ErodedAround(Clear, SmoothingRadius), SmoothingRadius);
		OutBands.SetNum(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			double Remaining = FMath::Min(Wanted[Index].Total(), FMath::Max(Available[Index], 0.0));

			if (Remaining < Wanted[Index].Total())
			{
				++NarrowedRingSides;
			}

			const auto Take = [&Remaining](double Width)
			{
				const double Taken = FMath::Min(Width, Remaining);
				Remaining -= Taken;
				return Taken;
			};

			FTracksideBands& Bands = OutBands[Index];
			Bands.bGravel = Wanted[Index].bGravel;
			Bands.Line = Take(Wanted[Index].Line);
			Bands.Kerb = Take(Wanted[Index].Kerb);
			Bands.Green = Take(Wanted[Index].Green);
			Bands.RunOff = Take(Wanted[Index].RunOff);
			Bands.Verge = Take(Wanted[Index].Verge);
		}
	};

	LayOutSide(LeftSide, LeftRunOffShare, OutLeft);
	LayOutSide(RightSide, RightRunOffShare, OutRight);

	UE_LOG(LogTracksideScenery, Log,
		TEXT("%s laid out bands on %d rings %.1f cm apart; %d of %d ring sides were narrowed where another part of the "
			"track is near"),
		*GetName(),
		Count,
		Spacing,
		NarrowedRingSides,
		2 * Count);
}

void ATracksideScenery::BuildGround(
	const TArray<FTracksideRing>& Rings,
	const TArray<FTracksideBands>& Left,
	const TArray<FTracksideBands>& Right,
	const FTracksideTerrain& Terrain)
{
	const int32 Count = Rings.Num();
	const double HalfWidth = Circuit->GetTrackWidth() * 0.5;

	TracksideMesh::FSection Section;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FTracksideRing& Here = Rings[Index];
		const FTracksideRing& Next = Rings[(Index + 1) % Count];

		const auto AddSide = [&](double Side, const FTracksideBands& BandsHere, const FTracksideBands& BandsNext)
		{
			const auto Point = [&](const FTracksideRing& Ring, double Offset, double Lift)
			{
				return Ring.RoadCentre + Ring.Right * (Side * (HalfWidth + Offset)) + Ring.Up * Lift;
			};

			const auto AddStrip = [&](
				double InnerHere, double OuterHere, double InnerNext, double OuterNext,
				double InnerLift, double OuterLift, const FLinearColor& Colour)
			{
				if (OuterHere - InnerHere < TracksideMesh::ThinnestStrip && OuterNext - InnerNext < TracksideMesh::ThinnestStrip)
				{
					return;
				}

				Section.AddQuadFacing(
					Point(Here, InnerHere, InnerLift),
					Point(Here, OuterHere, OuterLift),
					Point(Next, InnerNext, InnerLift),
					Point(Next, OuterNext, OuterLift),
					Here.Up,
					Colour);
			};

			const double LineHere = BandsHere.Line;
			const double LineNext = BandsNext.Line;
			const double KerbHere = LineHere + BandsHere.Kerb;
			const double KerbNext = LineNext + BandsNext.Kerb;
			const double GreenHere = KerbHere + BandsHere.Green;
			const double GreenNext = KerbNext + BandsNext.Green;
			const double RunOffHere = GreenHere + BandsHere.RunOff;
			const double RunOffNext = GreenNext + BandsNext.RunOff;
			const double VergeHere = RunOffHere + BandsHere.Verge;
			const double VergeNext = RunOffNext + BandsNext.Verge;

			AddStrip(0.0, LineHere, 0.0, LineNext, 0.0, 0.0, TracksideColours::WhiteLine);

			const FLinearColor& KerbColour = Index % 2 == 0 ? TracksideColours::KerbRed : TracksideColours::KerbYellow;
			const double CrownHere = LineHere + BandsHere.Kerb * 0.5;
			const double CrownNext = LineNext + BandsNext.Kerb * 0.5;
			AddStrip(LineHere, CrownHere, LineNext, CrownNext, 0.0, KerbCrownHeight, KerbColour);
			AddStrip(CrownHere, KerbHere, CrownNext, KerbNext, KerbCrownHeight, 0.0, KerbColour);

			AddStrip(KerbHere, GreenHere, KerbNext, GreenNext, 0.0, 0.0, TracksideColours::GreenStrip);

			if (BandsHere.bGravel)
			{
				AddStrip(GreenHere, RunOffHere, GreenNext, RunOffNext, 0.0, 0.0, TracksideColours::Gravel);
			}
			else
			{
				const double Boundaries[] = {
					RunOffStripeInset,
					RunOffStripeInset + RunOffStripeWidth,
					RunOffStripeInset + RunOffStripePitch,
					RunOffStripeInset + RunOffStripePitch + RunOffStripeWidth,
				};

				const FLinearColor* StripeColours[] = {
					&TracksideColours::RunOffAsphalt,
					&TracksideColours::RunOffYellowStripe,
					&TracksideColours::RunOffAsphalt,
					&TracksideColours::RunOffWhiteStripe,
					&TracksideColours::RunOffAsphalt,
				};

				double InnerHere = GreenHere;
				double InnerNext = GreenNext;

				const int32 StripeCount = static_cast<int32>(UE_ARRAY_COUNT(StripeColours));

				for (int32 Stripe = 0; Stripe < StripeCount; ++Stripe)
				{
					const bool bLast = Stripe == StripeCount - 1;
					const double OuterHere = bLast ? RunOffHere : FMath::Min(GreenHere + Boundaries[Stripe], RunOffHere);
					const double OuterNext = bLast ? RunOffNext : FMath::Min(GreenNext + Boundaries[Stripe], RunOffNext);

					AddStrip(InnerHere, OuterHere, InnerNext, OuterNext, 0.0, 0.0, *StripeColours[Stripe]);

					InnerHere = OuterHere;
					InnerNext = OuterNext;
				}
			}

			const bool bLightStripe = FMath::FloorToInt64(Here.Distance / MowingStripeLength) % 2 == 0;
			AddStrip(
				RunOffHere, VergeHere, RunOffNext, VergeNext, 0.0, -VergeFall,
				bLightStripe ? TracksideColours::VergeLight : TracksideColours::VergeDark);

			const auto SkirtFoot = [&](const FTracksideRing& Ring, double Offset)
			{
				const FVector Top = Point(Ring, Offset, -VergeFall);
				const double Foot = FMath::Min(Top.Z - MinimumSkirtDepth, Terrain.LowestAround(Top) - TerrainDrop);

				return FVector(Top.X, Top.Y, Foot);
			};

			Section.AddQuadFacing(
				Point(Here, VergeHere, -VergeFall),
				SkirtFoot(Here, VergeHere),
				Point(Next, VergeNext, -VergeFall),
				SkirtFoot(Next, VergeNext),
				Here.Right * Side,
				TracksideColours::Skirt);
		};

		AddSide(LeftSide, Left[Index], Left[(Index + 1) % Count]);
		AddSide(RightSide, Right[Index], Right[(Index + 1) % Count]);
	}

	Section.MoveIntoLocalSpace(Ground->GetComponentTransform().Inverse());

	Ground->CreateMeshSection_LinearColor(
		0,
		Section.Vertices,
		Section.Triangles,
		Section.Normals,
		TArray<FVector2D>(),
		Section.Colours,
		TArray<FProcMeshTangent>(),
		false);

	if (SceneryMaterial != nullptr)
	{
		Ground->SetMaterial(0, SceneryMaterial);
	}

	UE_LOG(LogTracksideScenery, Log, TEXT("%s built its ground bands: %d triangles"), *GetName(), Section.TriangleCount());
}

FTracksideTerrain ATracksideScenery::ShapeTerrain(
	const TArray<FTracksideRing>& Rings,
	const TArray<FTracksideBands>& Left,
	const TArray<FTracksideBands>& Right) const
{
	using namespace TracksideRings;

	const int32 Count = Rings.Num();
	const double Spacing = Circuit->GetTrackLength() / Count;
	const double HalfWidth = Circuit->GetTrackWidth() * 0.5;

	FBox2D Bounds(ForceInit);
	double HeightSum = 0.0;
	double WidestBands = 0.0;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		Bounds += FVector2D(Rings[Index].RoadCentre.X, Rings[Index].RoadCentre.Y);
		HeightSum += Rings[Index].RoadCentre.Z;
		WidestBands = FMath::Max(WidestBands, FMath::Max(Left[Index].Total(), Right[Index].Total()));
	}

	FTracksideTerrain Terrain;
	Terrain.CellSize = TerrainCellSize;
	Terrain.Origin = Bounds.Min - FVector2D(TerrainMargin, TerrainMargin);
	Terrain.CountX = FMath::CeilToInt32((Bounds.GetSize().X + 2.0 * TerrainMargin) / TerrainCellSize) + 1;
	Terrain.CountY = FMath::CeilToInt32((Bounds.GetSize().Y + 2.0 * TerrainMargin) / TerrainCellSize) + 1;
	Terrain.BorderHeight = HeightSum / Count;
	Terrain.Heights.SetNumUninitialized(Terrain.CountX * Terrain.CountY);

	const int32 NearStride = FMath::Max(1, FMath::RoundToInt32(TerrainSampleSpacing / Spacing));
	const FRingGrid NearSamples(Rings, TerrainNearSmoothing, NearStride);
	const FRingGrid Footprints(Rings, 1000.0);
	const int32 FootprintReach = FMath::CeilToInt32(
		(HalfWidth + WidestBands + TerrainFootprintMargin) / Footprints.CellSize);

	TArray<int32> FarSamples;

	for (int32 Index = 0; Index < Count; Index += NearStride * 4)
	{
		FarSamples.Add(Index);
	}

	const double NearFalloff = 1.0 / (2.0 * TerrainNearSmoothing * TerrainNearSmoothing);
	const double FarFalloff = 1.0 / (2.0 * TerrainFarSmoothing * TerrainFarSmoothing);
	double Lowest = TNumericLimits<double>::Max();
	double Highest = TNumericLimits<double>::Lowest();
	int32 PushedUnderTheBands = 0;

	for (int32 IndexY = 0; IndexY < Terrain.CountY; ++IndexY)
	{
		for (int32 IndexX = 0; IndexX < Terrain.CountX; ++IndexX)
		{
			const FVector Location(
				Terrain.Origin.X + IndexX * Terrain.CellSize, Terrain.Origin.Y + IndexY * Terrain.CellSize, 0.0);

			double FarWeight = 0.0;
			double FarSum = 0.0;

			for (const int32 Sample : FarSamples)
			{
				const double Weight = FMath::Exp(-FVector::DistSquaredXY(Rings[Sample].RoadCentre, Location) * FarFalloff);
				FarWeight += Weight;
				FarSum += Weight * Rings[Sample].RoadCentre.Z;
			}

			double NearWeight = 1.0;
			double NearSum = FarWeight > UE_DOUBLE_SMALL_NUMBER ? FarSum / FarWeight : Terrain.BorderHeight;

			NearSamples.ForEachNear(Location, 3, [&](int32 Sample)
			{
				const double Weight = FMath::Exp(-FVector::DistSquaredXY(Rings[Sample].RoadCentre, Location) * NearFalloff);
				NearWeight += Weight;
				NearSum += Weight * Rings[Sample].RoadCentre.Z;
			});

			const double BorderDistance = Terrain.CellSize * FMath::Min(
				FMath::Min(IndexX, Terrain.CountX - 1 - IndexX), FMath::Min(IndexY, Terrain.CountY - 1 - IndexY));

			double Height = FMath::Lerp(
				Terrain.BorderHeight, NearSum / NearWeight, FMath::SmoothStep(0.0, 1.0, BorderDistance / TerrainBorderBlend));

			const int32 Nearest = Footprints.NearestRing(Location, FootprintReach);

			if (Nearest != INDEX_NONE)
			{
				const FTracksideRing& Ring = Rings[Nearest];
				const FVector Across = Location - Ring.RoadCentre;
				const bool bRightOfTheTrack = Across.X * Ring.Right.X + Across.Y * Ring.Right.Y >= 0.0;
				const double Footprint = HalfWidth + TerrainFootprintMargin
					+ (bRightOfTheTrack ? Right[Nearest] : Left[Nearest]).Total();

				if (FVector::DistXY(Location, Ring.RoadCentre) < Footprint && Height > Ring.RoadCentre.Z - TerrainDrop)
				{
					Height = Ring.RoadCentre.Z - TerrainDrop;
					++PushedUnderTheBands;
				}
			}

			Terrain.Heights[IndexY * Terrain.CountX + IndexX] = Height;
			Lowest = FMath::Min(Lowest, Height);
			Highest = FMath::Max(Highest, Height);
		}
	}

	UE_LOG(LogTracksideScenery, Log,
		TEXT("%s shaped its terrain: %d x %d points %.0f cm apart, heights %.0f to %.0f cm, border at %.0f cm, %d points "
			"pushed under the bands"),
		*GetName(),
		Terrain.CountX,
		Terrain.CountY,
		Terrain.CellSize,
		Lowest,
		Highest,
		Terrain.BorderHeight,
		PushedUnderTheBands);

	return Terrain;
}

void ATracksideScenery::BuildTerrain(const FTracksideTerrain& Terrain)
{
	TracksideMesh::FSection Section;

	for (int32 IndexY = 0; IndexY < Terrain.CountY; ++IndexY)
	{
		for (int32 IndexX = 0; IndexX < Terrain.CountX; ++IndexX)
		{
			const int32 West = FMath::Max(IndexX - 1, 0);
			const int32 East = FMath::Min(IndexX + 1, Terrain.CountX - 1);
			const int32 South = FMath::Max(IndexY - 1, 0);
			const int32 North = FMath::Min(IndexY + 1, Terrain.CountY - 1);

			const double SlopeX = (Terrain.HeightAt(East, IndexY) - Terrain.HeightAt(West, IndexY))
				/ ((East - West) * Terrain.CellSize);
			const double SlopeY = (Terrain.HeightAt(IndexX, North) - Terrain.HeightAt(IndexX, South))
				/ ((North - South) * Terrain.CellSize);

			const double Shade = 0.82 + 0.3 * FMath::Frac(FMath::Sin(IndexX * 12.9898 + IndexY * 78.233) * 43758.5453);

			Section.AddVertex(
				FVector(
					Terrain.Origin.X + IndexX * Terrain.CellSize,
					Terrain.Origin.Y + IndexY * Terrain.CellSize,
					Terrain.HeightAt(IndexX, IndexY)),
				FVector(-SlopeX, -SlopeY, 1.0),
				TracksideColours::TerrainGrass * static_cast<float>(Shade));
		}
	}

	for (int32 IndexY = 0; IndexY + 1 < Terrain.CountY; ++IndexY)
	{
		for (int32 IndexX = 0; IndexX + 1 < Terrain.CountX; ++IndexX)
		{
			const int32 SouthWest = IndexY * Terrain.CountX + IndexX;
			const int32 SouthEast = SouthWest + 1;
			const int32 NorthWest = SouthWest + Terrain.CountX;
			const int32 NorthEast = NorthWest + 1;

			Section.AddTriangleFacing(SouthWest, SouthEast, NorthWest, FVector::UpVector);
			Section.AddTriangleFacing(SouthEast, NorthEast, NorthWest, FVector::UpVector);
		}
	}

	const FVector2D Near = Terrain.Origin;
	const FVector2D Far = Terrain.Origin + Terrain.Size();
	const double Reach = FarPlaneExtent;
	const double Height = Terrain.BorderHeight;

	const auto AddFarPlane = [&](double MinX, double MinY, double MaxX, double MaxY)
	{
		Section.AddQuadFacing(
			FVector(MinX, MinY, Height),
			FVector(MaxX, MinY, Height),
			FVector(MinX, MaxY, Height),
			FVector(MaxX, MaxY, Height),
			FVector::UpVector,
			TracksideColours::TerrainGrass);
	};

	AddFarPlane(Near.X - Reach, Near.Y - Reach, Far.X + Reach, Near.Y);
	AddFarPlane(Near.X - Reach, Far.Y, Far.X + Reach, Far.Y + Reach);
	AddFarPlane(Near.X - Reach, Near.Y, Near.X, Far.Y);
	AddFarPlane(Far.X, Near.Y, Far.X + Reach, Far.Y);

	Section.MoveIntoLocalSpace(Ground->GetComponentTransform().Inverse());

	Ground->CreateMeshSection_LinearColor(
		1,
		Section.Vertices,
		Section.Triangles,
		Section.Normals,
		TArray<FVector2D>(),
		Section.Colours,
		TArray<FProcMeshTangent>(),
		false);

	if (SceneryMaterial != nullptr)
	{
		Ground->SetMaterial(1, SceneryMaterial);
	}

	UE_LOG(LogTracksideScenery, Log, TEXT("%s built its terrain: %d triangles"), *GetName(), Section.TriangleCount());
}
