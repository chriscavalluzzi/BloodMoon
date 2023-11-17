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
    bool enableMod{};

    UPROPERTY(BlueprintReadWrite)
    int32 daysBetweenBloodMoon{};

    UPROPERTY(BlueprintReadWrite)
    bool enableCutscene{};

    UPROPERTY(BlueprintReadWrite)
    bool enableParticleEffects{};

    UPROPERTY(BlueprintReadWrite)
    bool enableRevive{};

    UPROPERTY(BlueprintReadWrite)
    bool skipReviveNearBases{};

    UPROPERTY(BlueprintReadWrite)
    float distanceConsideredClose{};

    /* Retrieves active configuration value and returns object of this struct containing it */
    static FBloodMoon_ConfigStruct GetActiveConfig(UObject* WorldContext) {
        FBloodMoon_ConfigStruct ConfigStruct{};
        FConfigId ConfigId{"BloodMoon", ""};
        if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)) {
          UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
          ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{ FBloodMoon_ConfigStruct::StaticStruct(), &ConfigStruct });
        }
        return ConfigStruct;
    }
};

