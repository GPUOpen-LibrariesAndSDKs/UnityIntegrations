echo off

set arg1=%1

set assetsDir=%arg1%\Assets\
set pluginDir=%assetsDir%\Plugins

if exist "%assetsDir%" (
	if exist "%pluginDir%" (
		echo Found plugin directory %pluginDir%
	) else (
		mkdir "%pluginDir%"
	)
	
	copy dist\*.cs  "%assetsDir%"
	copy dist\AmfUnityPlugin.dll "%assetsDir%"

	copy dist\amf-component-ffmpeg64.dll "%pluginDir%"
	copy dist\avcodec-57.dll "%pluginDir%"
	copy dist\avformat-57.dll "%pluginDir%"
	copy dist\avresample-3.dll "%pluginDir%"
	copy dist\avutil-55.dll "%pluginDir%"
	copy dist\swresample-2.dll "%pluginDir%"
	copy dist\avresample-3.dll "%pluginDir%"
) else (
	echo Unity project assets directory does not exist: %assetsDir%
)