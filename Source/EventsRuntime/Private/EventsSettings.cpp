// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventsSettings.h"
#include "EventsRuntimeModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

UEventsList::UEventsList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No config filename, needs to be set at creation time
}

void UEventsList::SortTags()
{
	EventList.Sort();
}

URestrictedEventsList::URestrictedEventsList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No config filename, needs to be set at creation time
}

void URestrictedEventsList::SortTags()
{
	RestrictedEventList.Sort();
}

bool FEventRestrictedConfigInfo::operator==(const FEventRestrictedConfigInfo& Other) const
{
	if (RestrictedConfigName != Other.RestrictedConfigName)
	{
		return false;
	}

	if (Owners.Num() != Other.Owners.Num())
	{
		return false;
	}

	for (int32 Idx = 0; Idx < Owners.Num(); ++Idx)
	{
		if (Owners[Idx] != Other.Owners[Idx])
		{
			return false;
		}
	}

	return true;
}

bool FEventRestrictedConfigInfo::operator!=(const FEventRestrictedConfigInfo& Other) const
{
	return !(operator==(Other));
}

UEventsSettings::UEventsSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ConfigFileName = GetDefaultConfigFilename();
	ImportTagsFromConfig = true;
	WarnOnInvalidTags = true;
	FastReplication = false;
	InvalidTagCharacters = ("\"',");
	NumBitsForContainerSize = 6;
	NetIndexFirstBitSegment = 16;
}

#if WITH_EDITOR
void UEventsSettings::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(UEventsSettings, RestrictedConfigFiles))
	{
		RestrictedConfigFilesTempCopy = RestrictedConfigFiles;
	}
}

void UEventsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetName() == "RestrictedConfigName")
		{
			UEventsManager& Manager = UEventsManager::Get();
			for (FEventRestrictedConfigInfo& Info : RestrictedConfigFiles)
			{
				if (!Info.RestrictedConfigName.IsEmpty())
				{
					if (!Info.RestrictedConfigName.EndsWith(TEXT(".ini")))
					{
						Info.RestrictedConfigName.Append(TEXT(".ini"));
					}
					FEventSource* Source = Manager.FindOrAddTagSource(*Info.RestrictedConfigName, EEventSourceType::RestrictedTagList);
					if (!Source)
					{
						FNotificationInfo NotificationInfo(FText::Format(NSLOCTEXT("EventsSettings", "UnableToAddRestrictedEventSource", "Unable to add restricted event source {0}. It may already be in use."), FText::FromString(Info.RestrictedConfigName)));
						FSlateNotificationManager::Get().AddNotification(NotificationInfo);
						Info.RestrictedConfigName.Empty();
					}
				}
			}
		}


		// if we're adding a new restricted config file we will try to auto populate the owner
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UEventsSettings, RestrictedConfigFiles))
		{
			if (RestrictedConfigFilesTempCopy.Num() + 1 == RestrictedConfigFiles.Num())
			{
				int32 FoundIdx = RestrictedConfigFilesTempCopy.Num();
				for (int32 Idx = 0; Idx < RestrictedConfigFilesTempCopy.Num(); ++Idx)
				{
					if (RestrictedConfigFilesTempCopy[Idx] != RestrictedConfigFiles[Idx])
					{
						FoundIdx = Idx;
						break;
					}
				}

				ensure(FoundIdx < RestrictedConfigFiles.Num());
				RestrictedConfigFiles[FoundIdx].Owners.Add(FPlatformProcess::UserName());

			}
		}
		IEventsModule::OnTagSettingsChanged.Broadcast();
	}
}
#endif

// ---------------------------------

UEventsDeveloperSettings::UEventsDeveloperSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	
}
