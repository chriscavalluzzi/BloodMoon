#include "BloodMoonParticleSceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/SubsystemActorManager.h"

UBloodMoonParticleSceneComponent::UBloodMoonParticleSceneComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> groundParticleSystemFinder(TEXT("NiagaraSystem'/BloodMoon/Particles/BloodMoon_GroundParticleSystem_Niagara.BloodMoon_GroundParticleSystem_Niagara'"));
	groundParticleSystem = groundParticleSystemFinder.Object;
}

UBloodMoonParticleSceneComponent::~UBloodMoonParticleSceneComponent() {
	End();
}

void UBloodMoonParticleSceneComponent::BeginPlay() {
	Super::BeginPlay();	
}

void UBloodMoonParticleSceneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetTimeSubsystem()) {
		//UE_LOG(LogTemp, Warning, TEXT(">>>>> DAY %d - %f"), GetTimeSubsystem()->GetPassedDays(), GetTimeSubsystem()->GetNightPct());
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