#include "BloodMoonSubsystem.h"
#include "Patching/NativeHookManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "FGGameMode.h"
#include "FGCharacterPlayer.h"
#include "Components/SceneCaptureComponent2D.h"
#include "FGSkySphere.h"
#include "FGWorldSettings.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Creature/FGCreatureSpawner.h"

ABloodMoonSubsystem::ABloodMoonSubsystem() {
	RegisterHooks();
	//PrimaryActorTick.bCanEverTick = true;
}

void ABloodMoonSubsystem::BeginPlay() {
	Super::BeginPlay();

	RegisterDelegates();
	UpdateBloodMoonNightStatus();
	GetTimeSubsystem()->mNumberOfPassedDays = 47; // DEV
}

void ABloodMoonSubsystem::RegisterHooks() {
#if !WITH_EDITOR
	AFGGameMode* exampleGameMode = GetMutableDefault<AFGGameMode>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGGameMode::PostLogin, exampleGameMode, [this](auto& scope, AFGGameMode* gm, APlayerController* pc) {
		if (gm->HasAuthority() && !gm->IsMainMenuGameMode()) {
			// This machine is the host
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting up..."))
		}
		});

	AFGCharacterPlayer* examplePlayerCharacter = GetMutableDefault<AFGCharacterPlayer>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::BeginPlay, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self) {
		if (self->IsLocallyControlled()) {
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Creating ParticleSceneComponent..."))
			UActorComponent* newComponent = self->AddComponentByClass(UBloodMoonParticleSceneComponent::StaticClass(), false, FTransform(), false);
			particleActorComponent = Cast< UBloodMoonParticleSceneComponent>(newComponent);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::CrouchPressed, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self) {
		// DEV test action
		ResetCreatureSpawners();
	});

	AFGSkySphere* exampleSkySphere = GetMutableDefault<AFGSkySphere>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::BeginPlay, exampleSkySphere, [this](auto& scope, AFGSkySphere* self) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Registering SkySphere..."))
		if (self->mMoonLight != moonLight) {
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] New MoonLight detected"))
		}
		skySphere = self;
		moonLight = self->mMoonLight;
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::Tick, exampleSkySphere, [this](auto& scope, AFGSkySphere* self, float deltaTime) {
		if (IsValid(GetTimeSubsystem())) {
			//UE_LOG(LogTemp, Warning, TEXT(">>> DAY %d - %f"), this->GetDayNumber(), this->GetNightPercent());
		}
		//FLinearColor color = moonLight->GetLightColor();
		//UE_LOG(LogTemp, Warning, TEXT(">< %f, %f, %f, %f"), color.R, color.G, color.B, color.A)
	});
	SUBSCRIBE_METHOD(ULightComponent::SetLightColor, [this](auto& scope, ULightComponent* self, FLinearColor color, bool bSRGB = true) {
		if (self->GetOwner() == moonLight && isBloodMoonNight) {
			scope(self, FLinearColor(1, 0, 0), bSRGB);
			scope.Cancel();
		}
	});
#endif
}

void ABloodMoonSubsystem::RegisterDelegates() {
	FScriptDelegate onNewDayDelegate;
	onNewDayDelegate.BindUFunction(this, "OnNewDay");
	GetTimeSubsystem()->mOnNewDayDelegate.Add(onNewDayDelegate);

	FScriptDelegate onDayStateDelegate;
	onDayStateDelegate.BindUFunction(this, "OnDayStateChanged");
	GetTimeSubsystem()->mOnDayStateDelegate.Add(onDayStateDelegate);
}

void ABloodMoonSubsystem::OnNewDay() {
	UE_LOG(LogTemp, Warning, TEXT(">>> NEW DAY: %d"), GetDayNumber())
	UpdateBloodMoonNightStatus();
	if (isBloodMoonNight) {
		TriggerBloodMoonMidnight();
	}
}

void ABloodMoonSubsystem::OnDayStateChanged() {
	// Default night begins at 16:30
	UE_LOG(LogTemp, Warning, TEXT(">>> DAY STATE CHANGE: %s"), GetTimeSubsystem()->IsDay() ? TEXT("day") : TEXT("night"))
	UpdateBloodMoonNightStatus();
}

void ABloodMoonSubsystem::Tick(float deltaSeconds) {
	
}

void ABloodMoonSubsystem::ResetCreatureSpawners() {
	TArray<AActor*> creatureSpawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFGCreatureSpawner::StaticClass(), creatureSpawners);
	for (int i = 0; i < creatureSpawners.Num(); i++) {
		AFGCreatureSpawner* spawner = Cast<AFGCreatureSpawner>(creatureSpawners[i]);
		if (spawner && !spawner->IsSpawnerActive()) {
			for (int j = 0; j < spawner->mSpawnData.Num(); j++) {
				FSpawnData* spawnData = &spawner->mSpawnData[j];
				if (spawnData->WasKilled && !IsValid(spawnData->Creature)) {
					spawnData->WasKilled = false;
					spawner->PopulateSpawnData();
					UE_LOG(LogTemp, Warning, TEXT(">>> RESPAWNING - Spawner %d, Creature %d/%d"), i, j, spawner->GetNumUnspawnedCreatures())
				}
			}
		}
	}
}

void ABloodMoonSubsystem::UpdateBloodMoonNightStatus() {
	AFGTimeOfDaySubsystem* timeSubsystem = GetTimeSubsystem();
	if (!isBloodMoonNight) {
		if (GetDayNumber() % 7 == 6 && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() > 0.5f) {
			TriggerBloodMoonEarlyNight();
		} else if (GetDayNumber() % 7 == 0 && GetDayNumber() > 0 && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() > 0.5f) {
			TriggerBloodMoonPostMidnight();
		}
	} else if (isBloodMoonNight && timeSubsystem->IsDay()) {
		ResetToStandardMoon();
	}
}

void ABloodMoonSubsystem::TriggerBloodMoonEarlyNight() {
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonPreMidnight() {
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonMidnight() {
	ResetCreatureSpawners();
	particleActorComponent->Start();
	//PauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonPostMidnight() {
	isBloodMoonNight = true;
	isBloodMoonDone = true;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::ResetToStandardMoon() {
	isBloodMoonNight = false;
	isBloodMoonDone = false;
	particleActorComponent->End();
	UnpauseTimeSubsystem();
}

AFGTimeOfDaySubsystem* ABloodMoonSubsystem::GetTimeSubsystem() {
	return AFGTimeOfDaySubsystem::Get(GetWorld());
}

void ABloodMoonSubsystem::PauseTimeSubsystem() {
	GetTimeSubsystem()->mUpdateTime = false;
}

void ABloodMoonSubsystem::UnpauseTimeSubsystem() {
	GetTimeSubsystem()->mUpdateTime = true;
}

int32 ABloodMoonSubsystem::GetDayNumber() {
	return GetTimeSubsystem()->GetPassedDays();
}

float ABloodMoonSubsystem::GetNightPercent() {
	return GetTimeSubsystem()->GetNightPct();
}