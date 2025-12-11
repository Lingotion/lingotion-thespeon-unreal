# lingotion-thespeon-unreal
<div align="center">

![](./Docs/data/lingotion_icon_ios_01_small.png)
  
</div>

<a target="_blank" href="https://discord.gg/9f2HFyu5gF"><img src="https://dcbadge.limes.pink/api/server/https://discord.gg/9f2HFyu5gF" alt="Join our Discord server" /></a>


**Lingotion Thespeon** is an on-device AI engine designed to generate real-time character acting and voiceovers.
The plugin runs entirely offline on the player’s device, eliminating cloud costs and network dependencies.

This is the beta version 0.1.0 of Lingotion Thespeon Unreal, and we appreciate any and all feedback on the plugin and its use. [Get in touch with the Lingotion developers and Thespeon users on our discord](https://discord.gg/9f2HFyu5gF)!

[Please report any encountered issues to the Issues page](https://github.com/Lingotion/lingotion-thespeon-unreal/issues) or in the Support section of the [Lingotion Discord](https://discord.gg/9f2HFyu5gF).

---
# Getting started

The process of getting started with Lingotion Thespeon consist of four main parts:
- Sign up in the Lingotion Developer Portal
- Download Lingotion Actor Pack(s) and Language Pack(s)
- Import the Thespeon Plugin to your Unreal Engine project
- Import Actor Pack(s) and Language Pack(s) into your Unreal Engine project

## **Developer Portal Setup**  
The Lingotion Packs are downloaded from the Lingotion developer portal. Before getting started with the plugin, we need to set up an account and download a pair of Actor and Language Packs.
The following guide will step you through the process of creating an account at the Lingotion developer portal: 
[Get Started - Webportal](./Docs/get-started-webportal.md)  

## **Unreal Plugin Setup**  
Lingotion Thespeon is an Unreal Plugin, and easily integrated into a Unreal Engine project. The following guide will step you through the process of installing the plugin and importing the Actor Packs into your Unreal Project:  
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
