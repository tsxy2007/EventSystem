// Fill out your copyright notice in the Description page of Project Settings.
#include "Setting/EventSystemSettings.h"



UEventSystemSettings::UEventSystemSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UEventSystemSettings::SaveEventes()
{
	SaveConfig();
}

UEventSystemSettings* UEventSystemSettings::GetEventSystemSettings()
{
	return GetMutableDefault<UEventSystemSettings>();
}

