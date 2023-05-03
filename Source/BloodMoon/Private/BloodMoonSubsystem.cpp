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
#include "Engine/WorldComposition.h"

ABloodMoonSubsystem::ABloodMoonSubsystem() {
	//PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<ULevelSequence> midnightSequenceFinder(TEXT("LevelSequence'/BloodMoon/BloodMoonMidnightSequence.BloodMoonMidnightSequence'"));
	midnightSequence = midnightSequenceFinder.Object;

	RegisterImmediateHooks();
}

void ABloodMoonSubsystem::BeginPlay() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting up..."))

	Super::BeginPlay();

	CreateGroundParticleComponent();
	UpdateConfig();
	RegisterDelayedHooks();
	RegisterDelegates();
	GetTimeSubsystem()->mNumberOfPassedDays = 48; // DEV
	UpdateBloodMoonNightStatus();
}

void ABloodMoonSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	isDestroyed = true;
}

void ABloodMoonSubsystem::RegisterImmediateHooks() {
#if !WITH_EDITOR
	AFGSkySphere* exampleSkySphere = GetMutableDefault<AFGSkySphere>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::BeginPlay, exampleSkySphere, [this](auto& scope, AFGSkySphere* self) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Registering SkySphere..."))
		skySphere = self;
		moonLight = self->mMoonLight;
		//UE_LOG(LogTemp, Warning, TEXT(">>>>> MOON ROTATION AXIS: %s"), *self->mMoonRotationAxis.ToCompactString())
		//UE_LOG(LogTemp, Warning, TEXT(">>>>> MOON ROTATION ORIGIN: %s"), *self->mMoonOriginRotation.ToCompactString())
	});
#endif
}

void ABloodMoonSubsystem::RegisterDelayedHooks() {
#if !WITH_EDITOR
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty, [this](UConfigManager* self, const FConfigId& ConfigId) {
		if (!this->isDestroyed && ConfigId == FConfigId{ "BloodMoon", "" }) {
			// The user config has been updated, reload it
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Config marked dirty, reloading...."))
			this->UpdateConfig();
		}
	})

	AFGCharacterPlayer* examplePlayerCharacter = GetMutableDefault<AFGCharacterPlayer>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::SetupPlayerInputComponent, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self, UInputComponent* PlayerInputComponent) {
		if (!this->isDestroyed) {
			CreateGroundParticleComponent();
			UpdateBloodMoonNightStatus();
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::CrouchPressed, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self) {
		// DEV test action
		//ResetCreatureSpawners();
		//TriggerBloodMoonMidnight();
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::Tick, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self, float deltaTime) {
		// DEV
		//UE_LOG(LogTemp, Warning, TEXT("LOC: %s | ROT: %s"), *self->GetTransform().GetLocation().ToCompactString(), *self->GetCameraComponentForwardVector().ToCompactString())
	});

	AFGSkySphere* exampleSkySphere = GetMutableDefault<AFGSkySphere>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::Tick, exampleSkySphere, [this](auto& scope, AFGSkySphere* self, float deltaTime) {
		//if (IsValid(GetTimeSubsystem())) {
		//	//UE_LOG(LogTemp, Warning, TEXT(">>> DAY %d - %f"), this->GetDayNumber(), this->GetNightPercent());
		//}
	});
	SUBSCRIBE_METHOD(ULightComponent::SetLightColor, [this](auto& scope, ULightComponent* self, FLinearColor color, bool bSRGB = true) {
		if (isBloodMoonNight && !this->isDestroyed && self->GetOwner() == moonLight) {
			scope(self, FLinearColor(1, 0, 0), bSRGB);
			scope.Cancel();
		}
	});
#endif
}

void ABloodMoonSubsystem::UpdateConfig() {
	if (IsValid(this)) {
		FBloodMoon_ConfigStruct config = FBloodMoon_ConfigStruct::GetActiveConfig();

		config_enableMod = config.enableMod;
		config_daysBetweenBloodMoon = config.daysBetweenBloodMoon;
		config_enableCutscene = config.enableCutscene;
		config_enableParticleEffects = config.enableParticleEffects;
		config_enableRevive = config.enableRevive;
		config_enableReviveNearBases = config.enableReviveNearBases;

		UpdateBloodMoonNightStatus();

		if (config_enableMod && config_enableParticleEffects && isBloodMoonNight) {
			StartGroundParticleSystem();
		} else {
			EndGroundParticleSystem();
		}
	}
}

void ABloodMoonSubsystem::CreateGroundParticleComponent() {
	if (GetCharacter() && !GetGroundParticleComponent()) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Creating ParticleSceneComponent..."))
		UActorComponent* newComponent = GetCharacter()->AddComponentByClass(UBloodMoonParticleSceneComponent::StaticClass(), false, FTransform(), false);
		Cast<UBloodMoonParticleSceneComponent>(newComponent);
	}
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
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] New day: %d"), GetDayNumber())
	if (isBloodMoonNight) {
		TriggerBloodMoonMidnight();
	}
}

void ABloodMoonSubsystem::OnDayStateChanged() {
	// Default night begins at 16:30
	UpdateBloodMoonNightStatus();
}

void ABloodMoonSubsystem::Tick(float deltaSeconds) {
	
}

void ABloodMoonSubsystem::ResetCreatureSpawners() {
	if (config_enableRevive) {
		TArray<AActor*> creatureSpawners;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFGCreatureSpawner::StaticClass(), creatureSpawners);
		for (int i = 0; i < creatureSpawners.Num(); i++) {
			AFGCreatureSpawner* spawner = Cast<AFGCreatureSpawner>(creatureSpawners[i]);
			if (spawner && (config_enableReviveNearBases || !spawner->IsNearBase())) {
				for (int j = 0; j < spawner->mSpawnData.Num(); j++) {
					FSpawnData* spawnData = &spawner->mSpawnData[j];
					if (spawnData->NumTimesKilled >= 1 && !IsValid(spawnData->Creature)) {
						spawnData->WasKilled = false;
						spawnData->NumTimesKilled = 0;
						spawnData->KilledOnDayNr = -1;
						UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Reviving: Spawner %d, Creature %d/%d"), i, j+1, spawner->mSpawnData.Num())
					}
					spawner->PopulateSpawnData();
				}
			}
		}
	}
}

void ABloodMoonSubsystem::OnMidnightSequenceStart() {
	ResetCreatureSpawners();
}

void ABloodMoonSubsystem::OnMidnightSequenceEnd() {
	ResumeWorldCompositionUpdates();
	TriggerBloodMoonPostMidnight();
}

void ABloodMoonSubsystem::UpdateBloodMoonNightStatus() {
	if (config_enableMod) {
		AFGTimeOfDaySubsystem* timeSubsystem = GetTimeSubsystem();
		if (GetDayNumber() % config_daysBetweenBloodMoon == (config_daysBetweenBloodMoon - 1) && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() >= 0.5f) {
			TriggerBloodMoonEarlyNight();
		} else if (GetDayNumber() % config_daysBetweenBloodMoon == 0 && GetDayNumber() > 0 && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() < 0.5f) {
			TriggerBloodMoonPostMidnight();
		} else {
			ResetToStandardMoon();
		}
	} else {
		ResetToStandardMoon();
	}
}

void ABloodMoonSubsystem::TriggerBloodMoonEarlyNight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] State: Blood moon night"))
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	StartGroundParticleSystem();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonPreMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] State: Blood moon pre-midnight"))
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	StartGroundParticleSystem();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting blood moon midnight sequence"))
	StartGroundParticleSystem();
	//PauseTimeSubsystem();
	if (config_enableCutscene) {
		// TODO: Check that suspension of world composition updates here won't impact ability to manually load levels/tiles in sequence
		SuspendWorldCompositionUpdates();
		BuildMidnightSequence();
	} else {
		ResetCreatureSpawners();
	}
}

void ABloodMoonSubsystem::TriggerBloodMoonPostMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] State: Blood moon post-midnight"))
	isBloodMoonNight = true;
	isBloodMoonDone = true;
	StartGroundParticleSystem();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::ResetToStandardMoon() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] State: Normal"))
	isBloodMoonNight = false;
	isBloodMoonDone = false;
	EndGroundParticleSystem();
	UnpauseTimeSubsystem();
}

ACharacter* ABloodMoonSubsystem::GetCharacter() {
	return UGameplayStatics::GetPlayerCharacter(this, 0);
}

UBloodMoonParticleSceneComponent* ABloodMoonSubsystem::GetGroundParticleComponent() {
	if (ACharacter* character = GetCharacter()) {
		return GetCharacter()->FindComponentByClass<UBloodMoonParticleSceneComponent>();
	}
	return nullptr;
}

void ABloodMoonSubsystem::StartGroundParticleSystem() {
	UBloodMoonParticleSceneComponent* particles = GetGroundParticleComponent();
	if (particles && config_enableParticleEffects) {
		particles->Start();
	}
}

void ABloodMoonSubsystem::EndGroundParticleSystem() {
	UBloodMoonParticleSceneComponent* particles = GetGroundParticleComponent();
	if (particles) {
		particles->End();
	}
}

void ABloodMoonSubsystem::BuildMidnightSequence() {
	FMovieSceneSequencePlaybackSettings sequenceSettings;
	sequenceSettings.bAutoPlay = true;
	sequenceSettings.bRestoreState = true;
	sequenceSettings.bDisableLookAtInput = true;
	sequenceSettings.bDisableMovementInput = true;
	sequenceSettings.bHideHud = true;
	FMovieSceneSequenceLoopCount loopCount;
	loopCount.Value = 0;
	sequenceSettings.LoopCount = loopCount;

	ALevelSequenceActor* midnightSequenceActorPtr = NewObject<ALevelSequenceActor>(this);
	midnightSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, midnightSequence, sequenceSettings, midnightSequenceActorPtr);
}

void ABloodMoonSubsystem::SuspendWorldCompositionUpdates() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Suspending world composition updates"))
	vanillaTilesStreamingTimeThreshold = GetWorld()->WorldComposition->TilesStreamingTimeThreshold;
	GetWorld()->WorldComposition->TilesStreamingTimeThreshold = DBL_MAX;
}

void ABloodMoonSubsystem::ResumeWorldCompositionUpdates() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Resuming world composition updates"))
	GetWorld()->WorldComposition->TilesStreamingTimeThreshold = vanillaTilesStreamingTimeThreshold;
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