#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSkySphere.h"
#include "FGTimeSubsystem.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
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

private:

	UBloodMoonParticleSceneComponent* particleActorComponent;

	ULevelSequence* midnightSequence;
	ULevelSequencePlayer* midnightSequencePlayer;

	bool isBloodMoonNight = false;
	bool isBloodMoonDone = false;
	int DaysBetweenBloodMoonNight = 7;

	void RegisterHooks();
	void RegisterDelegates();

	void UpdateBloodMoonNightStatus();

	void TriggerBloodMoonEarlyNight();
	void TriggerBloodMoonPreMidnight();
	void TriggerBloodMoonMidnight();
	void TriggerBloodMoonPostMidnight();
	void ResetToStandardMoon();

	void BuildMidnightSequence();
	void SuspendWorldCompositionUpdates();
	void ResumeWorldCompositionUpdates();

	void ResetCreatureSpawners();

	AFGTimeOfDaySubsystem* GetTimeSubsystem();
	void PauseTimeSubsystem();
	void UnpauseTimeSubsystem();
	int32 GetDayNumber();
	float GetNightPercent();

	int color_test = 0;
	AFGSkySphere* skySphere;
	ADirectionalLight* moonLight;

};