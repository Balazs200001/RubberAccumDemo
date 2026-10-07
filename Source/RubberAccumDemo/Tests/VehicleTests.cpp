#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../SplineFollowingCar.h"

namespace RubberAccumDemoVehicleTest
{
	UWorld* FindVehicleTestWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const bool bUsableType = Context.WorldType == EWorldType::Editor
				|| Context.WorldType == EWorldType::Game
				|| Context.WorldType == EWorldType::PIE;

			if (bUsableType && Context.World() != nullptr)
			{
				return Context.World();
			}
		}

		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleFieldCarsCollideWithNothing,
	"RubberAccumDemo.Vehicle.FieldCarsCollideWithNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleFieldCarsCollideWithNothing::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Car == nullptr)
	{
		AddError(TEXT("Failed to spawn ASplineFollowingCar."));
		return false;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Car->GetComponents(Primitives);

	TestTrue(TEXT("The car has a primitive component to check"), Primitives.Num() > 0);

	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		TestTrue(
			FString::Printf(
				TEXT("%s collides with nothing, so the player's car and the fly camera pass through the field"),
				*Primitive->GetName()),
			Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	}

	Car->Destroy();

	return true;
}

#endif
