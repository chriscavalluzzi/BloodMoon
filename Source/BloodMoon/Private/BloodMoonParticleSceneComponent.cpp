#include "BloodMoonParticleSceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/SubsystemActorManager.h"

UBloodMoonParticleSceneComponent::UBloodMoonParticleSceneComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickInterval(5.0);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> groundParticleSystemFinder(TEXT("NiagaraSystem'/BloodMoon/Particles/BloodMoon_GroundParticleSystem_Niagara.BloodMoon_GroundParticleSystem_Niagara'"));
	groundParticleSystem = groundParticleSystemFinder.Object;
}

void UBloodMoonParticleSceneComponent::BeginPlay() {
	Super::BeginPlay();	
}

void UBloodMoonParticleSceneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	End();
}

void UBloodMoonParticleSceneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (groundParticleSystemInstance) {
		UpdateGroundParticleSystem();
	}
}

void UBloodMoonParticleSceneComponent::Start() {
	CreateGroundParticleSystem();
}

void UBloodMoonParticleSceneComponent::End() {
	DestroyGroundParticleSystem();
}

void UBloodMoonParticleSceneComponent::CreateGroundParticleSystem() {
	if (!IsValid(groundParticleSystemInstance)) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting ground particle system"))
		groundParticleSystemInstance = UNiagaraFunctionLibrary::SpawnSystemAttached(groundParticleSystem, this, "", FVector(0, 0, 0), FRotator(), EAttachLocation::SnapToTarget, false);
		groundParticleSystemInstance->SetNiagaraVariableFloat(FString("SpawnRadius"), 2000.0f);
		groundParticleSystemInstance->SetNiagaraVariableVec3(FString("WindVector"), FVector(-200.0f,200.0f,200.0f));
		UpdateGroundParticleSystem();
	}
}

void UBloodMoonParticleSceneComponent::UpdateGroundParticleSystem() {
	if (IsValid(groundParticleSystemInstance)) {
		if (AFGTimeOfDaySubsystem* subsystem = GetTimeSubsystem()) {
			float nightPct = subsystem->GetNightPct();
			groundParticleSystemInstance->SetNiagaraVariableFloat(FString("SpawnMultiplier"), nightPct);
			groundParticleSystemInstance->SetNiagaraVariableFloat(FString("WindMultiplier"), pow(nightPct,7));
		}
	}
}

void UBloodMoonParticleSceneComponent::DestroyGroundParticleSystem() {
	if (IsValid(groundParticleSystemInstance)) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Destroying ground particle system"))
		groundParticleSystemInstance->DestroyInstance();
	}
	groundParticleSystemInstance = nullptr;
}

AFGTimeOfDaySubsystem* UBloodMoonParticleSceneComponent::GetTimeSubsystem() {
	return AFGTimeOfDaySubsystem::Get(GetWorld());
}