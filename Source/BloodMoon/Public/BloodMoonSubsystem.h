#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSkySphere.h"
#include "FGTimeSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "FGPlayerController.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "BloodMoon_ConfigStruct.h"
#include "BloodMoonParticleSceneComponent.h"
#include "BloodMoonSubsystem.generated.h"

UCLASS()
class BLOODMOON_API ABloodMoonSubsystem : public AModSubsystem {
	GENERATED_BODY()

public:
	ABloodMoonSubsystem();
	virtual void BeginPlay() override;
	virtual void Tick(float deltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	UFUNCTION()
	void OnNewDay();
	UFUNCTION()
	void OnDayStateChanged();
	UFUNCTION(BlueprintCallable)
	void OnMidnightSequenceStart();
	UFUNCTION(BlueprintCallable)
	void OnMidnightSequenceEnd();

private:
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	bool config_enableMod;
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	int32 config_daysBetweenBloodMoon;
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	bool config_enableCutscene;
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	bool config_enableRevive;
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	bool config_skipReviveNearBases;
	UPROPERTY(ReplicatedUsing = ApplyConfig)
	float config_distanceConsideredClose;
	bool config_enableParticleEffects;

	ADirectionalLight* moonLight;

	ULevelSequence* midnightSequence;
	ULevelSequence* midnightSequenceNoCut;
	ULevelSequencePlayer* midnightSequencePlayer;

	UClass* temporaryWorldStreamingSourceClass;
	AActor* temporaryWorldStreamingSource;

	bool isBloodMoonNight = false;
	bool isMidnightSequenceInProgress = false;
	bool isBloodMoonDone = false;

	double vanillaTilesStreamingTimeThreshold = 5.0;

	bool isSetup = false;
	bool isDestroyed = false;

	static ABloodMoonSubsystem* Get(UObject* WorldContext);
	UWorld* SafeGetWorld();
	bool IsHost();
	bool IsSafeToAccessWorld();
	AFGPlayerController* GetPlayerController();

	// Setup

	void CheckIfReadyForSetup();
	void Setup();
	void RegisterImmediateHooks();
	void RegisterDelayedHooks();
	void RegisterDelegates();
	void UpdateConfig();
	UFUNCTION()
	void ApplyConfig();
	void StartCreatureSpawnerBaseTrace();
	void CreateGroundParticleComponent();

	// Hooks

	void SaveMoonLight(ADirectionalLight* newMoonLight);
	void CheckConfigReload(const FConfigId& ConfigId);
	void SetupNewPlayer();
	bool CheckMoonLightColorOverride(ULightComponent* lightComponent);

	// Status Changes

	void UpdateBloodMoonNightStatus();
	void TriggerBloodMoonEarlyNight();
	void TriggerBloodMoonPreMidnight();
	void TriggerBloodMoonMidnight();
	void TriggerBloodMoonPostMidnight();
	void ResetToStandardMoon();

	// Particle Effects

	ACharacter* GetCharacter();
	UBloodMoonParticleSceneComponent* GetGroundParticleComponent();
	void StartGroundParticleSystem();
	void EndGroundParticleSystem();

	// Midnight

	void BuildMidnightSequence();
	void SuspendWorldCompositionUpdates();
	void ResumeWorldCompositionUpdates();
	void ResetCreatureSpawners();

	// Time Subystem

	AFGTimeOfDaySubsystem* GetTimeSubsystem();
	void PauseTimeSubsystem();
	void UnpauseTimeSubsystem();
	int32 GetDayNumber();
	float GetNightPercent();

	// Buildable Subsystem

	AFGBuildableSubsystem* GetBuildableSubsystem();
	void ChangeDistanceConsideredClose(float newDistanceConsideredClose);

	// UI

	UFGGameUI* GetGameUI();
	void SetUIVisibility(bool newVisibility);

};