#include "BloodMoonSubsystem.h"
#include "Patching/NativeHookManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "FGGameMode.h"
#include "FGCharacterPlayer.h"
#include "UI/FGGameUI.h"
#include "Components/SceneCaptureComponent2D.h"
#include "FGSkySphere.h"
#include "FGWorldSettings.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Creature/FGCreatureSpawner.h"
#include "Engine/WorldComposition.h"
#include "Net/UnrealNetwork.h"

ABloodMoonSubsystem::ABloodMoonSubsystem() {
#if !WITH_EDITOR
	PrimaryActorTick.bCanEverTick = true;
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<ULevelSequence> midnightSequenceFinder(TEXT("LevelSequence'/BloodMoon/BloodMoonMidnightSequence.BloodMoonMidnightSequence'"));
	midnightSequence = midnightSequenceFinder.Object;
	static ConstructorHelpers::FObjectFinder<ULevelSequence> midnightSequenceNoCutFinder(TEXT("LevelSequence'/BloodMoon/BloodMoonMidnightSequence_NoCut.BloodMoonMidnightSequence_NoCut'"));
	midnightSequenceNoCut = midnightSequenceNoCutFinder.Object;

	static ConstructorHelpers::FObjectFinder<UClass> temporaryWorldStreamingSourceFinder(TEXT("UClass'/BloodMoon/TemporaryWorldStreamingSource.TemporaryWorldStreamingSource_C'"));
	temporaryWorldStreamingSourceClass = temporaryWorldStreamingSourceFinder.Object;

	RegisterImmediateHooks();
#endif
}

void ABloodMoonSubsystem::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABloodMoonSubsystem, config_enableMod);
	DOREPLIFETIME(ABloodMoonSubsystem, config_daysBetweenBloodMoon);
	DOREPLIFETIME(ABloodMoonSubsystem, config_enableCutscene);
	DOREPLIFETIME(ABloodMoonSubsystem, config_enableRevive);
	DOREPLIFETIME(ABloodMoonSubsystem, config_skipReviveNearBases);
	DOREPLIFETIME(ABloodMoonSubsystem, config_distanceConsideredClose);
}

void ABloodMoonSubsystem::BeginPlay() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Starting up..."))

	Super::BeginPlay();

	CheckIfReadyForSetup();
}

void ABloodMoonSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	isDestroyed = true;
}

void ABloodMoonSubsystem::CheckIfReadyForSetup() {
	if (GetTimeSubsystem() && !isSetup) {
		Setup();
	}
}

void ABloodMoonSubsystem::Setup() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Running setup"))
	isSetup = true;

	TArray<AActor*> skySpheres;
	UGameplayStatics::GetAllActorsOfClass(SafeGetWorld(), AFGSkySphere::StaticClass(), skySpheres);
	for (int i = 0; i < skySpheres.Num(); i++) {
		AFGSkySphere* s = Cast<AFGSkySphere>(skySpheres[i]);
		if (s && s->mMoonLight) {
			moonLight = s->mMoonLight;
		}
	}

	CreateGroundParticleComponent();
	UpdateConfig();
	RegisterDelayedHooks();
	RegisterDelegates();
	UpdateBloodMoonNightStatus();

	SetActorTickEnabled(false); // Ticking no longer required, disable for rest of life
}

void ABloodMoonSubsystem::RegisterImmediateHooks() {
#if !WITH_EDITOR
	AFGSkySphere* exampleSkySphere = GetMutableDefault<AFGSkySphere>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGSkySphere::BeginPlay, exampleSkySphere, [](auto& scope, AFGSkySphere* self) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			bms->SaveMoonLight(self->mMoonLight);
		}
	});
#endif
}

void ABloodMoonSubsystem::CheckConfigReload(const FConfigId& ConfigId) {
	if (IsSafeToAccessWorld() && ConfigId == FConfigId{ "BloodMoon", "" }) {
		// The user config has been updated, reload it
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Config marked dirty, reloading...."))
		UpdateConfig();
	}
}

void ABloodMoonSubsystem::SaveMoonLight(ADirectionalLight* newMoonLight) {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Registering MoonLight..."))
	moonLight = newMoonLight;
}

void ABloodMoonSubsystem::SetupNewPlayer() {
	if (IsSafeToAccessWorld() && GetWorld()) {
		CreateGroundParticleComponent();
		UpdateBloodMoonNightStatus();
	}
}

bool ABloodMoonSubsystem::CheckMoonLightColorOverride(ULightComponent* lightComponent) {
	return (isBloodMoonNight && IsSafeToAccessWorld() && lightComponent->GetOwner() == moonLight);
}

void ABloodMoonSubsystem::RegisterDelayedHooks() {
#if !WITH_EDITOR
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty, [](UConfigManager* self, const FConfigId& ConfigId) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			bms->CheckConfigReload(ConfigId);
		}
	});

	AFGCharacterPlayer* examplePlayerCharacter = GetMutableDefault<AFGCharacterPlayer>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterPlayer::SetupPlayerInputComponent, examplePlayerCharacter, [](auto& scope, AFGCharacterPlayer* self, UInputComponent* PlayerInputComponent) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			bms->SetupNewPlayer();
		}
	});

	SUBSCRIBE_METHOD(ULightComponent::SetLightColor, [](auto& scope, ULightComponent* self, FLinearColor color, bool bSRGB = true) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			if (bms->CheckMoonLightColorOverride(self)) {
				scope(self, FLinearColor(1, 0, 0), bSRGB);
				scope.Cancel();
			}
		}
	});

	// Midnight sequence overrides

	AFGCharacterBase* exampleCharacterBase = GetMutableDefault<AFGCharacterBase>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterBase::OnTakeRadialDamage, exampleCharacterBase, [](auto& scope, AFGCharacterBase* self, AActor* damagedActor, float damage, const class UDamageType* damageType, FVector hitLocation, FHitResult hitInfo, class AController* instigatedBy, AActor* damageCauser) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			if (bms->isMidnightSequenceInProgress) {
				scope.Cancel();
			}
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterBase::OnTakePointDamage, exampleCharacterBase, [](auto& scope, AFGCharacterBase* self, AActor* damagedActor, float damage, class AController* instigatedBy, FVector hitLocation, class UPrimitiveComponent* hitComponent, FName boneName, FVector shotFromDirection, const class UDamageType* damageType, AActor* damageCauser) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			if (bms->isMidnightSequenceInProgress) {
				scope.Cancel();
			}
		}
	});

	APlayerCameraManager* examplePlayerCameraManager = GetMutableDefault<APlayerCameraManager>();
	SUBSCRIBE_METHOD_VIRTUAL(APlayerCameraManager::StartCameraShake, examplePlayerCameraManager, [](auto& scope, APlayerCameraManager* self, TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f, ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			if (bms->isMidnightSequenceInProgress) {
				scope.Override(nullptr);
			}
		}
	});
	/*
	BMTODO SUBSCRIBE_METHOD_VIRTUAL(APlayerCameraManager::PlayCameraAnim, examplePlayerCameraManager, [](auto& scope, APlayerCameraManager* self, class UCameraAnim* Anim, float Rate = 1.f, float Scale = 1.f, float BlendInTime = 0.f, float BlendOutTime = 0.f, bool bLoop = false, bool bRandomStartTime = false, float Duration = 0.f, ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator) {
		if (this->isMidnightSequenceInProgress) {
			scope.Override(nullptr);
		}
	});
	*/

	UFGHealthComponent* exampleHealthComponent = GetMutableDefault<UFGHealthComponent>();
	SUBSCRIBE_METHOD_VIRTUAL(UFGHealthComponent::TakeDamage, exampleHealthComponent, [](auto& scope, UFGHealthComponent* self, AActor* damagedActor, float damageAmount, const class UDamageType* damageType, class AController* instigatedBy, AActor* damageCauser) {
		if (ABloodMoonSubsystem* bms = ABloodMoonSubsystem::Get(self)) {
			if (bms->isMidnightSequenceInProgress) {
				scope.Cancel();
			}
		}
	});
#endif
}

void ABloodMoonSubsystem::UpdateConfig() {
	if (IsValid(this) && SafeGetWorld()) {
		FBloodMoon_ConfigStruct config = FBloodMoon_ConfigStruct::GetActiveConfig(SafeGetWorld());

		if (IsHost()) {
			config_enableMod = config.enableMod;
			config_daysBetweenBloodMoon = config.daysBetweenBloodMoon;
			config_enableCutscene = config.enableCutscene;
			config_enableRevive = config.enableRevive;
			config_skipReviveNearBases = config.skipReviveNearBases;
			config_distanceConsideredClose = config.distanceConsideredClose * 100.0f;
		}
		config_enableParticleEffects = config.enableParticleEffects;

		ApplyConfig();
	}
}

void ABloodMoonSubsystem::ApplyConfig() {
	if (IsSafeToAccessWorld()) {
		ChangeDistanceConsideredClose(config_distanceConsideredClose);
		UpdateBloodMoonNightStatus();
		if (config_enableMod && config_enableParticleEffects && isBloodMoonNight) {
			StartGroundParticleSystem();
		} else {
			EndGroundParticleSystem();
		}
	}
}

void ABloodMoonSubsystem::StartCreatureSpawnerBaseTrace() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Running TraceForNearbyBase on all spawners (async)"))
	TArray<AActor*> creatureSpawners;
	UGameplayStatics::GetAllActorsOfClass(SafeGetWorld(), AFGCreatureSpawner::StaticClass(), creatureSpawners);
	for (int i = 0; i < creatureSpawners.Num(); i++) {
		AFGCreatureSpawner* spawner = Cast<AFGCreatureSpawner>(creatureSpawners[i]);
		spawner->TraceForNearbyBase();
	}
}

void ABloodMoonSubsystem::CreateGroundParticleComponent() {
	if (GetCharacter() && !GetGroundParticleComponent()) {
		UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Creating ParticleSceneComponent..."))
		// TODO: See about connecting this to the camera instead of the character (for vehicles and other 3rd-person camera situations)
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
	CheckIfReadyForSetup();
}

void ABloodMoonSubsystem::ResetCreatureSpawners() {
	if (config_enableRevive) {
		TArray<AActor*> creatureSpawners;
		UGameplayStatics::GetAllActorsOfClass(SafeGetWorld(), AFGCreatureSpawner::StaticClass(), creatureSpawners);
		for (int i = 0; i < creatureSpawners.Num(); i++) {
			AFGCreatureSpawner* spawner = Cast<AFGCreatureSpawner>(creatureSpawners[i]);
			if (spawner && (!config_skipReviveNearBases || !spawner->IsNearBase())) {
				for (int j = 0; j < spawner->mSpawnData.Num(); j++) {
					FSpawnData* spawnData = &spawner->mSpawnData[j];
					if (spawnData->NumTimesKilled >= 1 && !IsValid(spawnData->Creature)) {
						spawnData->WasKilled = false;
						spawnData->NumTimesKilled = 0;
						spawnData->KilledOnDayNr = -1;
						UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Reviving creature! (Spawner %d, Creature %d/%d)"), i, j+1, spawner->mSpawnData.Num())
					}
					spawner->PopulateSpawnData();
				}
			}
		}
	}
}

void ABloodMoonSubsystem::OnMidnightSequenceStart() {
	ResetCreatureSpawners();
	SetUIVisibility(false);
}

void ABloodMoonSubsystem::OnMidnightSequenceEnd() {
	ResumeWorldCompositionUpdates();
	TriggerBloodMoonPostMidnight();
	SetUIVisibility(true);
	isMidnightSequenceInProgress = false;
}

void ABloodMoonSubsystem::UpdateBloodMoonNightStatus() {
	if (config_enableMod) {
		AFGTimeOfDaySubsystem* timeSubsystem = GetTimeSubsystem();
		if (timeSubsystem) {
			if (GetDayNumber() % config_daysBetweenBloodMoon == (config_daysBetweenBloodMoon - 1) && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() >= 0.5f) {
				TriggerBloodMoonEarlyNight();
			} else if (GetDayNumber() % config_daysBetweenBloodMoon == 0 && GetDayNumber() > 0 && timeSubsystem->IsNight() && timeSubsystem->GetNormalizedTimeOfDay() < 0.5f) {
				TriggerBloodMoonPostMidnight();
			} else {
				ResetToStandardMoon();
			}
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
		isMidnightSequenceInProgress = true;
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

//either freeze creatures or disable camera shake on damgage during sequence

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

	ULevelSequence* sequence;
	if (IsHost()) {
		sequence = midnightSequence;
	} else {
		sequence = midnightSequenceNoCut;
	}

	ALevelSequenceActor* midnightSequenceActorPtr = NewObject<ALevelSequenceActor>(this);
	midnightSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, sequence, sequenceSettings, midnightSequenceActorPtr);
}

void ABloodMoonSubsystem::SuspendWorldCompositionUpdates() {
	UE_LOG(LogTemp, Warning, TEXT("[BloodMoon] Suspending world partition streaming"));

	// Add temporary world partition streaming source at player's current location
	FTransform characterTransform;
	AFGCharacterBase* controlledCharacter = GetPlayerController()->GetControlledCharacter();
	if (controlledCharacter) {
		// Player is in a vehicle or something
		characterTransform = controlledCharacter->GetTransform();
	} else {
		characterTransform = GetCharacter()->GetTransform();
	}
	temporaryWorldStreamingSource = GetWorld()->SpawnActor(temporaryWorldStreamingSourceClass, &characterTransform);

	// Temporarily disable player world partition streaming
	GetPlayerController()->bEnableStreamingSource = false;
}

void ABloodMoonSubsystem::ResumeWorldCompositionUpdates() {
	UE_LOG(LogTemp, Display, TEXT("[BloodMoon] Resuming world partition streaming"));

	// Re-enable player world partition streaming
	GetPlayerController()->bEnableStreamingSource = true;

	// Remove temporary world partition streaming source
	if (IsValid(temporaryWorldStreamingSource)) {
		temporaryWorldStreamingSource->Destroy();
	} else {
		UE_LOG(LogTemp, Error, TEXT("[BloodMoon] Temporary world streaming source was invalid!"));
	}
}

AFGTimeOfDaySubsystem* ABloodMoonSubsystem::GetTimeSubsystem() {
	return AFGTimeOfDaySubsystem::Get(SafeGetWorld());
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

AFGBuildableSubsystem* ABloodMoonSubsystem::GetBuildableSubsystem() {
	return AFGBuildableSubsystem::Get(SafeGetWorld());
}

void ABloodMoonSubsystem::ChangeDistanceConsideredClose(float newDistanceConsideredClose) {
	if (AFGBuildableSubsystem* buildableSubsystem = GetBuildableSubsystem()) {
		buildableSubsystem->mDistanceConsideredClose = newDistanceConsideredClose;
		StartCreatureSpawnerBaseTrace();
	}
}

ABloodMoonSubsystem* ABloodMoonSubsystem::Get(UObject* WorldContext) {
	return Cast<ABloodMoonSubsystem>(UGameplayStatics::GetActorOfClass(WorldContext, StaticClass()));
}

UWorld* ABloodMoonSubsystem::SafeGetWorld() {
	if (!isDestroyed) {
		return GetWorld();
	} else {
		return nullptr;
	}
}

bool ABloodMoonSubsystem::IsHost() {
	AGameModeBase* gm = UGameplayStatics::GetGameMode(GetWorld());
	return (gm && gm->HasAuthority());
}

bool ABloodMoonSubsystem::IsSafeToAccessWorld() {
	return isSetup && !isDestroyed;
}

AFGPlayerController* ABloodMoonSubsystem::GetPlayerController() {
	if (APlayerController* playerController = UGameplayStatics::GetPlayerController(this, 0)) {
		return Cast<AFGPlayerController>(playerController);
	}
	return nullptr;
}

UFGGameUI* ABloodMoonSubsystem::GetGameUI() {
	if (AFGPlayerController* playerController = GetPlayerController()) {
		if (AFGHUD* hud = playerController->GetHUD<AFGHUD>()) {
			return hud->GetGameUI();
		}
	}
	return nullptr;
}

void ABloodMoonSubsystem::SetUIVisibility(bool newVisibility) {
	if (UFGGameUI* ui = GetGameUI()) {
		ui->SetVisibility(newVisibility ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}