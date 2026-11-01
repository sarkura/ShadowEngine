#pragma once

#include "Modules/ModuleManager.h"

class FShadowEngineExporterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
};
