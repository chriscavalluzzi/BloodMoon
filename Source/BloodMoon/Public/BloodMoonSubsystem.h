#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSkySphere.h"
#include "FGTimeSubsystem.h"
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

	UFUNCTION()
	void OnNewDay();
	UFUNCTION()
	void OnDayStateChanged();
	UFUNCTION(BlueprintCallable)
	void OnMidnightSequenceEnd();

private:

	bool config_enableMod;
	int32 config_daysBetweenBloodMoon;
	bool config_enableCutscene;
	bool config_enableParticleEffects;
	bool config_enableRevive;
	bool config_enableReviveNearBases;

	AFGSkySphere* skySphere;
	ADirectionalLight* moonLight;

	ULevelSequence* midnightSequence;
	ULevelSequencePlayer* midnightSequencePlayer;

	bool isBloodMoonNight = false;
	bool isBloodMoonDone = false;

	double vanillaTilesStreamingTimeThreshold = 5.0;

	// Setup

	void RegisterImmediateHooks();
	void RegisterDelayedHooks();
	void RegisterDelegates();
	void UpdateConfig();
	void CreateGroundParticleComponent();

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

};