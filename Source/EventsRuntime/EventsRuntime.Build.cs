// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EventsRuntime : ModuleRules
	{
		public EventsRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"EventsRuntime/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "EventSystemRuntime"
                }
				);

            if (Target.Type == TargetType.Editor)
            {
                PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "SlateCore",
                    "Slate"
                }
                );
            }
        }
	}
}
