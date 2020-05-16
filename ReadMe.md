# NGPCarMenu

Custom plugin for **Richard Burns Rally game (RBR v1.02 SSE)** to improve the "Select Car" in-game menu (ie. for those people who don't use external RBR game launchers). 

The original "Select Car" menu within the RBR game has various problems:
- The preview image of the car (garage) doesn't match with the custom car model.
- The car specification window shows information about original cars even when custom car models have been installed (engine, transmission, year).
- The car specification window doesn't show additional information potentially available in custom car models (FIA category, creator and version of 3D car model).
- The car menu name shows the original car name even when RBR would use custom cars published by WorkerBee (NGP models) or other custom 3D car models. 
- The menu and model name has room for only short car model names (number of characters).

This **NGPCarMenu RBR plugin solves these problems** by doing following enhancements to the built-in "Select Car" menu:
- Shows a custom car preview image using the actual 3D model of custom car models.
- Shows the actual car specs information from a NGP model (engine HP, number of gears, 4WD/FWD/RWD, FIA category, classification year, etc).
- Shows more information about installed 3D car models and authors (3D car model creators definitely deserve more attention).
- Generates new car preview images from the currently installed 3D car models (*Options/Plugins/NGPCarMenu/Create car image* in-game menu command).
- Customized car and camera position to take automatic screenshots of 3D car models (well, not yet implemented in INI config file. Car and cam position is pre-set in the plugin code. Maybe the next version moves this setting to INI file).
- Longer car model names in car specs window (as much there is room on the screen).
- Longer car menu names (up to 30 chars).

Note! This plugin supports only the latest RichardBurnsRally_SSE.exe v1.02 version for Windows OS (haven't tested this under Wine/Linux, but the plugin probably works there also).

![](misc/NGPCarMenu_SelectCarMenu.png)
![](misc/NGPCarMenu_PluginMenu.png)

## Download
- **[The latest build of NGPCarMenu](https://github.com/mika-n/NGPCarMenu/releases)**

## Installation
- Download the latest version from the link shown above (ZIP file).
- Unzip the NGPCarMenu-versionTag.zip file to the root folder of existing installation of RBR game (for example *c:\games\richardBurnsRally* ).
- Check *plugins\NGPCarMenu.ini* file settings, especially the *RBRCITCarListPath* option. This option should point to the carList.ini file from NGP physics plugin (by default *RBRCIT\carlist\carList.ini* which is a relative path under the RBR installation folder).
- Start *RichardBurnsRally_SSE.exe* game as usual and go to *"Options/Plugins/NGPCarMenu"* in-game menu if this is the first time you use this plugin or you have installed new NGP custom car models via RBRCIT tool.
- At first you probably don't have any car preview images under *Plugins\NGPCarMenu\preview* folder. Select *"Create car images"* menu command to generate new preview images. Sit down and wait while the plugin creates new preview images from all installed car models.
- After this you are ready to race! Go to *"Quick Rally/Multiplayer/Season"* in-game menu and verify that you can see the new information and car preview image in *"Select Car"* menu.

Note! The NGPCarMenu.ini file has various screen resolution specific options. There are default values for the most common resolutions, but if the resolution you use is not there then you should add new resolution entry and set a value to at least ScreenshotCropping option. This option defines the rectangle area used in a car preview image screenshot.

## Configuration
See rbr\plugins\NGPCarMenu.ini file for more details:
| Option                 | Comment                                                                                |
|------------------------|----------------------------------------------------------------------------------------|
| ScreenshotPath         | Location of car preview images (relative to RBR executable location or absolute path). |
| ScreenshotReplay       | Replay filename the plugin uses to generate preview images. |
| RBRCITCarListPath      | Path to RBRCIT carList.ini configuration file (NGP car specs) |
| ScreenshotCropping     | Cropping rectangle for the screenshot. Make it "big enough but not too big" to fill the bottom part of "Select Car" RBR menu screen (empty string or 0 0 0 0 uses full screen in a car preview images). |
| CarSelectLeftBlackBar  | Optional rectangle black bar to hide the stock RBR "left frame" image in "Select Car" menu (empty string or 0 0 0 0 disables the black side bar). |
| CarSelectRightBlackBar | (the same as above) |
| ScreenshotCarPosition  | Car position within the replay video while taking the car preview screenshot. *(not yet implemented)* |
| ScreenshotCamPosition  | Camera position while taking the screenshot. *(not yet implemented)* |

## Questions and Answers
- **Does "Create car image" command overwrite existing car preview images?** No, the plugin uses *"Create only missing images"* option by default. However, you can change this option in the plugin's in-game menu to *"All car images"* option. This option re-creates all car images.
- **Can I use my own PNG picture files in the preview subfolder?** Yes you can. If you don't like the preview images created by the plugin then you can use any PNG file as long the filename matches with the car model name used in NGP physics plugin (carList.ini name entries).
- **Why the "Create car image" command doesn't seem to generate any new files under Plugins\NGPCarMenu\preview folder?** Some PC setups and Windows UAC settings prevent applications to write anything below *c:\program files* or *c:\program files (x86)* folder. Try to use some other folder in NGPCarMenu.ini ScreenshotPath option or copy the whole RBR game folder structure to, for example, *c:\games\rbr* folder.
- **The plugin creates too big/too small car preview images. How to tweak the image size?** Tweak NGPCarMenu.ini ScreenshotCropping setting. The option uses screen pixel values to set a rectangle area used in a screenshot. Values are relative to the RBR in-game window (0 0 is the left-top pixel of RBR game window).
- **Why the plugin creates just empty black PNG preview image files?** You probably have *fullscreen=true* value in **rbr\RichardBurnsRally.ini** file. Set this to *fullscreen=false* while creating new car preview images. I recommend to use FALSE in this original RBR ini file anyway and to use WorkeBee's FixUp plugin to set the window mode as fullscreen.
- **Can I use this plugin even when I don't use RBRCIT tool to install custom car models?** Yes you can, but you still need to have carList.ini file from NGP physics plugin, but the ini file doesn't have to be the one downloaded and installed by RBRCIT tool. See NGPCarMenu.ini *RBRCITCarListPath* option. Also, each of the eight rbr\physics\carFolder\ subfolders should have a file named as the car model name (no file extension). This filename should match the carList.ini name entries. The content of this file is used to show additional information about the 3D car model (author name, version). Anyway, I recommend the RBRCIT tool because it setups these files automatically when ever you install a new custom car.
- **Can I use this plugin with the original RBR car models without NGP plugin?** Hmmm... Can't see a single reason why NGP car models are not used in RBR. Anyway, in that case you don't need this plugin because the original game has all the information needed for original car models.
- **What are rbr\plugins\NGPCarMenu\preview\1920x1080 and 1366x768 folders?** This plugin saves new car preview images per resolution (the same preview image may not be the best one for all native resolutions). The *preview* folder has a subfolder which matches the resolution you use (the plugin creates the folder if it is missing). Also, NGPCarMenu.ini file should have a configuration block per resolution.

- **What is the folder structure used by this plugin?** The DLL and INI plugin files must be in the Plugins directory under the RBR game root folder. Replays RBR folder should have a replay file set in *ScreenshotReplay* option. The file hierarchy is something like this:
   c:\apps\richardBurnsRally\plugins\NGPCarMenu.dll
   c:\apps\richardBurnsRally\plugins\NGPCarMenu.ini
   c:\apps\richardBurnsRally\plugins\NGPCarMenu\preview\
   c:\apps\richardBurnsRally\Replays\NGPCarMenu.rpl

Note! Some Windows OS setups limit writing new files under c:\program files\ or c:\program files (x86)\ folders. If creation of new car preview PNG image file fails then change the default ScreenshotPath option value or copy the whole RBR game to, for example, c:\apps\rbr\ folder.

## Additional links
- Next Generation Physics RBR plugin, NGP. (author: WorkerBee). [NGP Home page](http://www.ly-racing.de/viewtopic.php?t=7878)
- FixUp RBR plugin. (author: WorkerBee). [FixUp Home page](http://www.ly-racing.de/viewtopic.php?t=7878)
- Pacenote RBR plugin. (author: WorkerBee). [Pacenote Home page](http://www.ly-racing.de/viewtopic.php?t=6848)
- RBR Car Installation Tool, RBRCIT. (author: Zissakos). [RBRCIT Home page](https://github.com/zissakos/RBRCIT){:target="_blank"}
 
This NGPCarMenu plugin has been tested with these magnificent RBR plugins/tools and works great with those. In fact, this plugin should be compatible with any plugin because the plugin doesn't change how the RBR game handles racing and the actual car models.

Copyright (c) 2020 by MIKA-N. All rights reserved. See LicenseText.txt file for more information. https://github.com/mika-n/NGPCarMenu
