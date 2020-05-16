
 NGPCarMenu. Custom plugin to improve the "Select Car" menu in stock RBR game menus (ie. for those people who don't use external RBR game launchers).

 Copyright (c) 2020 by MIKA-N. https://github.com/mika-n

 This plugin supports following features
 - Longer car menu names (up to 30 chars)
 - Shows the actual car specs information from NGP plugin instead of showing the old default car specs (which are not valid with NGP physics and car models)
 - Shows more information about installed 3D car models and authors (3D car model creators definitely deserve more attention).
 - Shows a custom car preview image to show the actual 3D model of custom car model (instead of showing the non-relevant original car garage image)
 - Generates new car preview images from the currently installed 3D car models
 - Customized car and camera position to take automatic screenshots of 3D car models (well, not yet implemented in INI config file. Car and cam position is pre-set in the plugin code).

 Note! This plugin supports only the RichardBurnsRally_SSE.exe v1.02 version (the NO_SSE and old patch versions are probably not used by anyone anymore).

Configuration (see NGPCarMenu.ini file for more details):

 NGPCarMenu.ini (the file needs to be in UTF8 format)
   ScreenshotPath        = Location of car preview images (relative to RBR executable location or absolute path).
   ScreenshotReplay      = Replay filename the plugin uses to generate preview images.

   RBRCITCarListPath     = Path to RBRCIT carList.ini configuration file (NGP car specs)

   ScreenshotCarPosition = Car position within the replay video while taking the car preview screenshot. (not yet implemented)
   ScreenshotCamPosition = Camera position while taking the screenshot. (not yet implemented)

   ScreenshotCropping    = Cropping rectangle for the screenshot (make it "big enough but not too big" to fill the bottom part of "Select Car" RBR menu).
   CarSelectLeftBlackBar = Optional rectangle black bar to hide the stock RBR "left frame" image in "Select Car" menu (empty string or 0 0 0 0 disables).
   CarSelectRightBlackBar= (the same as above)

Instructions:
  Use Options/Plugins/NGPCarMenu menu commands to generate new car preview images from the currently installed 3D car models (custom models, NGP physics).
  Car preview images are stored under the path set in ScreenshotPath INI option and resolution specific subfolder. For example something like
     c:\games\richardBurnsRally\plugings\NGPCarMenu\preview\1920x1080\

  The car preview image is shown in "Select Car" menu to show an actual image of the custom 3D car model. The plugin also shows car specs from NGP physics properties.
  Use RBRCIT tool to manage NGP car models and physics. By default this plugin expects to find RBRCIT carList.ini file from rbr\RBRCIT\carlist\ folde. Also, RBRCIT app is expected to copy model description file to rbr\physics\<car subfolders>\ folders (the file with car model name and without extension).

  The plugin must be in the Plugins directory under the game root folder. The file hierarchy is something like this:
     c:\apps\richardBurnsRally\plugins\NGPCarMenu.dll
     c:\apps\richardBurnsRally\plugins\NGPCarMenu.ini
     c:\apps\richardBurnsRally\plugins\NGPCarMenu\preview\
     c:\apps\richardBurnsRally\Replays\NGPCarMenu.rpl

Note! Some Windows OS versions limit writing new files under c:\program files\ or c:\program files (x86)\ folders. If writing of new car preview
      PNG image files fail then change the default ScreenshotPath option value or copy the whole RBR game to, for example, c:\app\rbr\ folder.
