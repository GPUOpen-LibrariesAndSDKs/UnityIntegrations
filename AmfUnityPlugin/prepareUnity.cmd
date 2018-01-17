echo off

set unitytemp=unity-tmp

where HG
IF %ERRORLEVEL% EQU 0 (
	echo HG found
	if exist "%unitytemp%" (
		echo Removing temporary directory %unitytemp%
		rd /s /q %unitytemp%
	)
	mkdir %unitytemp%
	pushd %unitytemp%
	echo Cloning https://bitbucket.org/Unity-Technologies/graphicsdemos
	hg clone https://bitbucket.org/Unity-Technologies/graphicsdemos
	popd
	echo Copying
	copy "%unitytemp%\graphicsdemos\NativeRenderingPlugin\PluginSource\source\Unity\*.h" source\Unity
	echo Removing %unitytemp%
	rd /s /q "%unitytemp%"
) else (
	echo Aborting, HG binary not found.
)
