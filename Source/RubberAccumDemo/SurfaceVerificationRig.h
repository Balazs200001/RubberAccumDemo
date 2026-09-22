#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacetrackSurfaceTypes.h"
#include "SurfaceRegion.h"
#include "SurfaceVerificationRig.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class ASurfaceVerificationRig : public AActor
{
	GENERATED_BODY()

public:
	ASurfaceVerificationRig();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Surface Verification")
	FSurfaceRegion Region;

	UPROPERTY(EditAnywhere, Category = "Surface Verification")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(EditAnywhere, Category = "Surface Verification", meta = (ClampMin = "0.0"))
	float ContactSpeed = 5500.0f;

	UPROPERTY(EditAnywhere, Category = "Surface Verification", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlipAmount = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Surface Verification", meta = (ClampMin = "0.0"))
	float ContactWidth = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Surface Verification")
	EAccumulationChannel Channel = EAccumulationChannel::SkidMark;

private:
	void ResizeSurfaceToRegion();

	UPROPERTY(VisibleAnywhere, Category = "Surface Verification")
	TObjectPtr<UStaticMeshComponent> Surface;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMaterialInstance;

	double DistanceSwept = 0.0;
};
