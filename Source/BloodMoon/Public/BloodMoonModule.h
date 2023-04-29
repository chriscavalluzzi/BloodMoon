	#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBloodMoonModule : public FDefaultGameModuleImpl {

public:
	FBloodMoonModule();
	virtual void StartupModule() override;
	virtual bool IsGameModule() const override { return true; }

private:

};
