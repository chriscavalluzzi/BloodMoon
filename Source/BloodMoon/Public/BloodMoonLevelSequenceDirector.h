#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceDirector.h"
#include "BloodMoonSubsystem.h"
#include "BloodMoonLevelSequenceDirector.generated.h"

UCLASS()
class BLOODMOON_API UBloodMoonLevelSequenceDirector : public ULevelSequenceDirector
{
	GENERATED_BODY()

	ABloodMoonSubsystem* subsystem;
};