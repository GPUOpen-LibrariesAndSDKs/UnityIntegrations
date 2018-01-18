# Instructions for using the AMF plugin for Unity

The AmfUnityPlugin uses the Advanced Media Framework (AMF) SDK for hardware accelerated video and audio playback on AMD GPUs in the Unity engine.

The Unity engine we have tested with is version 2017.3.

The steps required to setup this plugin are described next.

## AMD Driver 

Install the AMD driver version 17.7.2 or later

## Unity header files 

Run the prepareUnity.cmd to clone the Unity bitbucket repo and copy the header files into the source/Unity directory.

- The hg Mercurial binary must be available in the Windows path for this script to work.
- Use a Windows command console rather than a Powershell to run this command.

If Mercurial is not available then download the files manually and copy the headers from "GraphicsDemos / NativeRenderingPlugin / PluginSource / source / Unity /" to the source/Unity directory.

## Building AMF 

Open the source\AMF\amf\public\samples\CPPSamples_vs2013.sln solution file and build AMF in Release x64 mode.

- AMF is now a subtree of this repo so it does not need to be downloaded.

## Build the plugin

Open the Plugin solution file AmfUnityPlugin.sln.

- Build the plugin in Release x64.
	
## DLLS 

The following DLLs should now be in the build tree:

- From the plugin: AmfUnityPlugin.dll
		
- From AMF: amf-component-ffmpeg64.dll avcodec-57.dll avformat-57.dll avresample-3.dll avutil-55.dll swresample-2.dll avresample-3.dll
		
## Distribution 

Make a distribution directory by running the makeDist.cmd script.

- Use a Windows command console rather than a Powershell to run this command.

## Using with Unity

Do the following to build a project that can display the video:

- Create a Quad
- Create a Material
- Assign the Material to the Quad
- Change the Material tiling to 1,-1 so the video is not inverted
- Attach the source/Scripts/AmfUnityPlugin.cs to the Quad
- Copy a mp4 video into the Assets/StreamingAssets folder of the project
- Set the name of the video in the Inspector of the Quad(plugin will look in Assets/StreamingAssets directory)
- Save the Project

## Copying to a Unity project

Copy the DLLs from the ./dist directory into the Unity project Assets/Plugins folder.

- The dist2Proj.cmd script can be used to perform this copy instead of a manual copy.
- Pass in the path to the Unity project as the first parameter such as: dist2Proj.cmd C:\dev\files\unity\5.6.amf\AMFVideoScene\

## Running the game

Running the game will display the video on the Quad surface along with audio.


# 360 Video

360 video is supported by using the source/Scripts/Amf360UnityPlugin.cs script instead of the source/Scripts/AmfUnityPlugin.cs. The 360 script
contains detailed instructions on how to set up geometry, materials and lights Assets.  Follow the build instructions above to build AMF and the plugin and also which DLLs need to be put into the Asset/Plugins folder.


# Notes for repository maintainers

Use the following commands to update the AMF subtree:

- git subtree pull --prefix AmfUnityPlugin/source/AMF https://github.com/GPUOpen-LibrariesAndSDKs/AMF.git master --squash
- git commit --amend
	



