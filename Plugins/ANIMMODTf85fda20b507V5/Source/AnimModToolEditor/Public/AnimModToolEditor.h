// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"

class FSlateStyleSet;

class IContentBrowserSelectionMenuExtender
{
public:
	virtual void Extend() = 0;
};

class FAnimModToolEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FName GetStyleSetName();
	static FName GetContextMenuIconName();
	static FName GetReverseIconName();
	static FName GetMirrorIconName();

	const ISlateStyle& GetStyleSet() const { return *StyleSet; }

protected:
	void StartupStyle();
	void ShutdownStyle();

protected:
	TSharedPtr<FSlateStyleSet> StyleSet;
	TArray<TSharedPtr<IContentBrowserSelectionMenuExtender>> ContentBrowserSelectionMenuExtenders;
};