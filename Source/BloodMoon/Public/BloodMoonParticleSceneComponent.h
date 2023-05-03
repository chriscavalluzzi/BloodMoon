#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "FGTimeSubsystem.h"
#include "BloodMoonParticleSceneComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLOODMOON_API UBloodMoonParticleSceneComponent : public USceneComponent {
	GENERATED_BODY()

public:	
	UBloodMoonParticleSceneComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Start();
	void End();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UNiagaraSystem* groundParticleSystem;
	UNiagaraComponent* groundParticleSystemInstance;

	void CreateGroundParticleSystem();
	void UpdateGroundParticleSystem();
	void DestroyGroundParticleSystem();
	AFGTimeOfDaySubsystem* GetTimeSubsystem();
};
