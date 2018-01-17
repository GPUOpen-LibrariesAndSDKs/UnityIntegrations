echo off

set dist=.\dist
if exist "%dist%" (
	echo Removing distribution directory %dist%
	rd /s /q "%dist%"
)

mkdir %dist%

set plugin=.\bin\x64\Release\AmfUnityPlugin.dll
set dbgplugin=.\bin\x64\Debug\AmfUnityPlugin.dll

if exist "%plugin%" (
	echo Copying Release plugin
	copy .\bin\x64\Release\AmfUnityPlugin.dll %dist%
) else (
	if exist "%dbgplugin%" ( 
		echo Copying Debug plugin
		copy .\bin\x64\Debug\AmfUnityPlugin.dll %dist%
	) else (
		echo Plugin has not been built.
	)
)

set dlldir=.\source\AMF\amf\bin\vs2013x64Release\
if exist "%dlldir%" (
	copy "%dlldir%\amf-component-ffmpeg64.dll" %dist%
	copy "%dlldir%\avcodec-57.dll" %dist%
	copy "%dlldir%\avformat-57.dll" %dist%
	copy "%dlldir%\avresample-3.dll" %dist%
	copy "%dlldir%\avutil-55.dll" %dist%
	copy "%dlldir%\swresample-2.dll" %dist%
	copy "%dlldir%\avresample-3.dll" %dist%
) else (
	echo FFMPEG DLL directory has not been created.
)

copy .\source\scripts\Amf360UnityPlugin.cs %dist%
copy .\source\scripts\AmfUnityPlugin.cs %dist%

