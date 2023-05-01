#pragma once
#include "CoreMinimal.h"
#include "Configuration/ConfigManager.h"
#include "Engine/Engine.h"
#include "BloodMoon_ConfigStruct.generated.h"

/* Struct generated from Mod Configuration Asset '/BloodMoon/BloodMoon_Config' */
USTRUCT(BlueprintType)
struct FBloodMoon_ConfigStruct {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadWrite)
    bool enableMod;

    UPROPERTY(BlueprintReadWrite)
    int32 daysBetweenBloodMoon;

    UPROPERTY(BlueprintReadWrite)
    bool enableCutscene;

    UPROPERTY(BlueprintReadWrite)
    bool enableParticleEffects;

    UPROPERTY(BlueprintReadWrite)
    bool enableRevive;

    UPROPERTY(BlueprintReadWrite)
    bool enableReviveNearBases;

    /* Retrieves active configuration value and returns object of this struct containing it */
    static FBloodMoon_ConfigStruct GetActiveConfig() {
        FBloodMoon_ConfigStruct ConfigStruct{};
        FConfigId ConfigId{"BloodMoon", ""};
        UConfigManager* ConfigManager = GEngine->GetEngineSubsystem<UConfigManager>();
        ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{FBloodMoon_ConfigStruct::StaticStruct(), &ConfigStruct});
        return ConfigStruct;
    }
};

