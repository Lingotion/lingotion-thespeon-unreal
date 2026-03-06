using UnrealBuildTool;
using System.IO;

public class Protobuf : ModuleRules
{
    public Protobuf(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        // Path to protobuf source
        string ProtobufSrcPath = Path.Combine(ModuleDirectory, "protobuf", "src");
        
        // Add include paths so protobuf headers can be found
        PublicSystemIncludePaths.Add(ProtobufSrcPath);
        
        // Required defines for protobuf
        PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI");
        PublicDefinitions.Add("GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL=0");
        
        // Platform-specific configuration
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("_SCL_SECURE_NO_WARNINGS");
            PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
            
            // System libraries needed by protobuf on Windows
            PublicSystemLibraries.Add("ws2_32.lib");
            PublicSystemLibraries.Add("bcrypt.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicDefinitions.Add("HAVE_PTHREAD");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicDefinitions.Add("HAVE_PTHREAD");
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicDefinitions.Add("HAVE_PTHREAD");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicDefinitions.Add("HAVE_PTHREAD");
        }
        
        // Disable RTTI and exceptions to match Unreal's build settings
        bUseRTTI = false;
        bEnableExceptions = false;
    }
}