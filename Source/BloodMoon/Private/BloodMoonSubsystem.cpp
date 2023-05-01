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
	RegisterHooks();
	//PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<ULevelSequence> midnightSequenceFinder(TEXT("LevelSequence'/BloodMoon/BloodMoonMidnightSequence.BloodMoonMidnightSequence'"));
	midnightSequence = midnightSequenceFinder.Object;
}

void ABloodMoonSubsystem::BeginPlay() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting up..."))

	Super::BeginPlay();

	RegisterDelegates();
	UpdateBloodMoonNightStatus();
	GetTimeSubsystem()->mNumberOfPassedDays = 48; // DEV
}

void ABloodMoonSubsystem::RegisterHooks() {
#if !WITH_EDITOR
	AFGGameMode* exampleGameMode = GetMutableDefault<AFGGameMode>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGGameMode::PostLogin, exampleGameMode, [this](auto& scope, AFGGameMode* gm, APlayerController* pc) {
		if (gm->HasAuthority() && !gm->IsMainMenuGameMode()) {
			// This machine is the host
		}
	});

	AFGCharacterPlayer* examplePlayerCharacter = GetMutableDefault<AFGCharacterPlayer>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::BeginPlay, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self) {
		if (self->IsLocallyControlled()) {
			// TODO: This is creating too many components, switch to a function that will definitely only apply to the main player object
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Creating ParticleSceneComponent..."))
			UActorComponent* newComponent = self->AddComponentByClass(UBloodMoonParticleSceneComponent::StaticClass(), false, FTransform(), false);
			particleActorComponent = Cast< UBloodMoonParticleSceneComponent>(newComponent);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::SetupPlayerInputComponent, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self, UInputComponent* PlayerInputComponent) {
		// TODO: Try switching AFGCharacterPlayer::BeginPlay hook to this
		UE_LOG(LogTemp, Warning, TEXT(">>>>> PLAYER INPUT COMPONENT CREATION"))
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::CrouchPressed, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self) {
		// DEV test action
		//ResetCreatureSpawners();
		//TriggerBloodMoonMidnight();
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::Tick, examplePlayerCharacter, [this](auto& scope, AFGCharacterPlayer* self, float deltaTime) {
		// DEV
		//UE_LOG(LogTemp, Warning, TEXT("Rotator: %s"), *self->GetCameraComponentForwardVector().ToCompactString())
		//UE_LOG(LogTemp, Warning, TEXT("Location: %s"), *self->GetTransform().GetLocation().ToCompactString())
	});

	AFGSkySphere* exampleSkySphere = GetMutableDefault<AFGSkySphere>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::BeginPlay, exampleSkySphere, [this](auto& scope, AFGSkySphere* self) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Registering SkySphere..."))
		if (self->mMoonLight != moonLight) {
			UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] New MoonLight detected"))
		}
		skySphere = self;
		moonLight = self->mMoonLight;

		UE_LOG(LogTemp, Warning, TEXT(">>>>> MOON ROTATION AXIS: %s"), *self->mMoonRotationAxis.ToCompactString())
		UE_LOG(LogTemp, Warning, TEXT(">>>>> MOON ROTATION ORIGIN: %s"), *self->mMoonOriginRotation.ToCompactString())
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::Tick, exampleSkySphere, [this](auto& scope, AFGSkySphere* self, float deltaTime) {
		if (IsValid(GetTimeSubsystem())) {
			//UE_LOG(LogTemp, Warning, TEXT(">>> DAY %d - %f"), this->GetDayNumber(), this->GetNightPercent());
		}
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
		} else if (GetDayNumber() % 7 == 0 && GetDayNumber() > 0 && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() < 0.5f) {
			TriggerBloodMoonPostMidnight();
		}
	} else if (isBloodMoonNight && timeSubsystem->IsDay()) {
		ResetToStandardMoon();
	}
}

void ABloodMoonSubsystem::TriggerBloodMoonEarlyNight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting blood moon night"))
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonPreMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting blood moon pre-midnight sequence"))
	isBloodMoonNight = true;
	isBloodMoonDone = false;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::TriggerBloodMoonMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting blood moon midnight sequence"))
	ResetCreatureSpawners();
	particleActorComponent->Start();
	//PauseTimeSubsystem();
	// TODO: Check that suspension of world composition updates here won't impact ability to manually load levels/tiles in sequence
	SuspendWorldCompositionUpdates();
	// TODO: Resume world composition updates after sequence has ended
	BuildMidnightSequence();
}

void ABloodMoonSubsystem::TriggerBloodMoonPostMidnight() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting blood moon post-midnight sequence"))
	isBloodMoonNight = true;
	isBloodMoonDone = true;
	particleActorComponent->Start();
	UnpauseTimeSubsystem();
}

void ABloodMoonSubsystem::ResetToStandardMoon() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Returning to standard moon"))
	isBloodMoonNight = false;
	isBloodMoonDone = false;
	particleActorComponent->End();
	UnpauseTimeSubsystem();
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
	if (UWorld* world = GetWorld()) {
		// TODO: Confirm that this value is time since last update, not a division of time since game start
		//	(In other words, test that changing this value to x will ensure the next update will be somewhere between x and x-5 seconds)
		//	If it's time since last update, we can set this to a smaller value, to reduce issues if the value doesn't get reset for some reason
		world->WorldComposition->TilesStreamingTimeThreshold = 9999999.0;
	}
}

void ABloodMoonSubsystem::ResumeWorldCompositionUpdates() {
	if (UWorld* world = GetWorld()) {
		world->WorldComposition->TilesStreamingTimeThreshold = 5.0;
	}
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