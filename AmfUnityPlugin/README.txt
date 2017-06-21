Instructions for using AMF plugin for Unity.

AmfUnityPlugin is a plugin using the Advanced Media Framework (AMF) SDK for hardware accelerated video and audio playback on AMD GPUs in Unity engine.

--Follow directions in both the source/AMF folder for including AMF and the source/Unity folder README.txt files to
acquire the dependent source.

--Build AMF in Release x64 with the source\AMF\amf\public\samples\CPPSamples_vs2013.sln solution file.
--Build the plugin in Release x64 using the top level solution file.

--Copy the following DLLs into the Unity Assets folder.

From AMF:
amf-component-ffmpeg64.dll
avformat-57.dll
avutil-55.dll
avcodec-57.dll
avresample-3.dll
swresample-2.dll

From plugin:
AmfUnityPlugin.dll

--Attach source/Scripts/AmfUnityPlugin.cs to the game object that will display the video in Unity. This object must have it's own material, mesh renderer and mesh filter.

If the video displays inverted then the material's texture coordinate will need to be modified.


==========================================================================================================================

360 video is supported by using the source/Scripts/Amf360UnityPlugin.cs script instead of the source/Scripts/AmfUnityPlugin.cs. The 360 script
contains detailed instructions on how to set up geometry, materials and lights Assets.  Follow the build instructions above to build AMF and the plugin and also
which DLLs need to be put into the Asset folder.





