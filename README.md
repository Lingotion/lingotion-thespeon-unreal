# lingotion-thespeon-unreal
<div align="center">

![](./Docs/data/lingotion_icon_ios_01_small.png)
  
</div>

<a target="_blank" href="https://discord.gg/9f2HFyu5gF"><img src="https://dcbadge.limes.pink/api/server/https://discord.gg/9f2HFyu5gF" alt="Join our Discord server" /></a>


**Lingotion Thespeon** is an on-device AI engine designed to generate real-time character acting and voiceovers.
The plugin runs entirely offline on the player’s device, eliminating cloud costs and network dependencies.

Please report any encountered [issues to the Issues page](https://github.com/Lingotion/lingotion-thespeon-unreal/issues) or in the Support section of the [Lingotion Discord](https://discord.gg/9f2HFyu5gF).

# Features
* Fully on-device, no usage cost, no internet connection required
* Real-time generation both on GPU and CPU
* Ethical and legally safe models, voice actors are compensated
* Syncing in-game events with generated audio
* Support for 33 emotions, with more on the way
* Custom pronunciation support with IPA notation
* Number and ordinal pronunciation
* PC and Mac platforms supported, with more coming soon
* Can be used both in Blueprint and C++

# Getting started

The process of getting started with Lingotion Thespeon consist of four main parts:
- Sign up in the Lingotion Developer Portal
- Download _.lingotion_ file(s) related to your chosen Character(s)
- Import the Thespeon Plugin to your Unreal Engine project
- Import _.lingotion_ file(s) into your Unreal Engine project

## **Developer Portal Setup**  
The _.lingotion_ file(s) are downloaded from the Lingotion developer portal. Before getting started with the plugin, we need to set up an account to download these files.
The following guide will step you through the process of creating an account at the Lingotion developer portal: 
[Get Started - Webportal](https://github.com/Lingotion/.github/blob/main/profile/portal-docs/get-started-webportal.md)  

## **Unreal Plugin Setup**  
Lingotion Thespeon is an Unreal Plugin, and easily integrated into an Unreal Engine project. The following guide will step you through the process of installing the plugin and importing the _.lingotion_ file(s) into your Unreal Project:
[Get Started - Unreal](./Docs/get-started-unreal.md)  

---

# Changelog
See [CHANGELOG.md](./CHANGELOG.md) for changes.


# Known Issues
See [known-issues.md](./Docs/known-issues.md) for a list of known issues.


# License
![License](https://img.shields.io/badge/license-Custom-blue.svg)

This project is licensed according to the Terms of Service found at [lingotion.com/terms-of-service/](https://lingotion.com/terms-of-service/).

# Uninstalling Lingotion Thespeon 
Simply uninstalling the plugin using the Plugin Manager will not remove all Thespeon related files. To completely remove Thespeon from your project you will also have to remove the following in the content browser: 

```
Folder: All > Content > LingotionThespeon
File: ProjectSettings > Lingotion.Thespeon.license
```
