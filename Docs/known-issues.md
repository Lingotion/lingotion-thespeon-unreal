# Known Issues and Limitations  
**These issues are known and are currently being addressed by the Lingotion development team. If you find any other issues, please create a new issue through the [GitHub repository](https://github.com/Lingotion/lingotion-thespeon-unreal/issues/new).**
* IPA support is not implemented yet.
* Only a single input segment is supported as of now.
* Multiple parallel inferences are not supported yet and will result in a crash.
* The first synthesis has higher latency and performance impact than subsequent synthetizations due to buffer initializations. It is advised to utilize `TryPreloadCharacter` to load the models into memory before synthesis if possible.
* Very short syntheses - shorter than about 1/3 of a second - are not supported yet and will result in a crash.
* Once selected, it is not possible to change the inference backend during runtime.