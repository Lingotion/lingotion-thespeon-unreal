# Get Started - Unreal

## Table of Contents
- [**Overview**](#overview)
- [**Install the Thespeon Unreal Plugin**](#install-the-thespeon-unreal-plugin)
- [**Add the developer license key**](#add-the-developer-license-key)
- [**Import the downloaded _.lingotion_ pack**](#import-the-downloaded-lingotion-pack)
- [**Run the _GUISample_ level**](#run-the-guisample-level)
- [**Optimization guide**](#optimization-guide)

---

## Overview
This document details a step-by-step guide on how to install Lingotion Thespeon in your Unreal project, as well as how to import packs downloaded from the Lingotion Developer Portal.
This process has four main steps:
1. Install the Thespeon Plugin
2. Add the developer license key
3. Import the downloaded _.lingotion_ files
4. Run the _GUISample_ level 
> [!TIP]
> If you have not downloaded any _.lingotion_ files, please follow the [Get Started - Webportal](./get-started-webportal.md) guide before proceeding.
> 

## Install the Thespeon Unreal Plugin
> [!IMPORTANT]
> The Lingotion Thespeon Plugin only works in C++ projects. If your project is created as a Blueprint project, follow the instructions at [https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-cpp-quick-start#1requiredsetup](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-cpp-quick-start#1requiredsetup) before continuing.
> 
1. Clone the repository into the **Plugins** folder of your project, or download your desired release and extract it into the **Plugins** folder. If the folder does not exist, create it.

2. Regenerate the Visual Studio Solution files, and recompile your project. 

If the following popup occurs, press **Yes**:

![Missing modules](./data/missing-modules.png)

3. When starting the project, verify that the **Lingotion Thespeon** plugin is enabled in the plugin window found in `Edit > Plugins`:

![Plugin Window](./data/plugin-window.png)

## Add the developer license key
Navigate to `Edit > Project Settings > Plugins > Lingotion Thespeon` and enter the license key from the Lingotion Developer Portal project.

![Copy license](./data/license-copy.png)

![Paste license](./data/paste-license.png)

> [!IMPORTANT]
> Lingotion Thespeon generates audio in 44100 Hz - please make sure that all platforms you target have their sample rate set to 44100 in the `Edit > Project Settings > Platforms > ... > Audio Mixer Sample Rate` setting. If you don't do this, the audio will sound pitched.
> 

## Import the downloaded _.lingotion_ pack
Now that the plugin is installed, we can import the _.lingotion_ pack from the Lingotion developer portal.

Thespeon has its own information window that displays an overview of installed actors and languages, tools for importing and deleting modules from the project.

1. To find the _Lingotion Thespeon Info_ window, go to `Window > Lingotion Thespeon Info` from the top menu.


![Lingotion Thespeon Info Window](./data/thespeon-info.png)

2. Press `Import pack` and select your downloaded *.lingotion* file. If the import is successful, the actor(s) and language(s) should now be visible in the window:

![Imported packs](./data/imported-packs.png)

Now everything is set up to start using Lingotion Thespeon to generate voices!

## Run the _GUISample_ level
The quickest way to test Lingotion Thespeon is to try the GUISample Level included in the plugin. Navigate to `Content Browser > Plugins > Lingotion Thespeon > Samples > GUISample > GUISample Scene` and open the level. 

Press play, and you should see a simple UI where you can generate audio from an input text. 

> [!IMPORTANT]
> The first time each character is synthesized from will have significantly slower performance due to buffer allocations. We recommend pre-loading characters with a mock-synthesis before regular use - the see how it is done in the Level Blueprint of the GUISample scene.

## Optimization guide
For inference on the CPU backend, it is possible to fine-tune the performance impact by controlling the number of threads that the backend runtime uses.
Under `Edit > Project Settings > NNERuntimeORT` there exists settings called _Intra-op thread count_ and _Inter-op thread count_. 

These settings influence the number of threads the ONNX Runtime may spawn during inference.
A value of 0 means unconstrained use of spawned threads, which will lead to FPS drops in Unreal Engine due to resource contention.
Anything higher than 0 sets an actual number of threads. If the number is too low, some users will experience choppy audio due to generation not being fast enough. If the number is too high, the ONNX Runtime will compete too much with Unreal Engine, leading to frame drops.

We recommend to start with 8 threads for both settings and adjust as needed according to the rule of thumb above.
This plugin contains a `Config/DefaultEngine.ini` that can be imported in the Project settings window to set these values.

## Next steps
Check the Level Blueprint to see how to use the Thespeon Component in your own game. A guide on how to use the blueprint classes exists in the Level Blueprint of the MinimalCharacterSample in the folder adjecent to the GUISample level. An example C++ Actor example class can also be found in `Plugins > lingotion-thespeon-unreal > Source > LingotionThespeon > ASimpleThespeonActor.cpp`.
