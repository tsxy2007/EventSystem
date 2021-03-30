// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EventsEditor : ModuleRules
	{
		public EventsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"AssetRegistry",
				}
			);


			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"AssetRegistry",
					"EventsRuntime",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"BlueprintGraph",
					"KismetCompiler",
					"GraphEditor",
					"ContentBrowser",
					"MainFrame",
					"UnrealEd",
					"SourceControl",
					"ToolMenus",
					"KismetWidgets",
					"PropertyEditor",
					"EventSystemRuntime",
					"BlueprintCompilerCppBackend",
                    "ContentBrowserData",
                "JsonUtilities",
                "Json",
                }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings"
				}
			);
		}
	}
}
