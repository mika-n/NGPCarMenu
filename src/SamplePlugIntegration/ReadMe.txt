// Richard Burns Rally, Sample dummy plugin to demonstrate how to use NGPCarMenu API functions to draw custom images in a plugin menu.

Files
	- IPlugin.h / IRBRGame.h/ PluginHelpers.h / RBRTestPlugin.cpp - Standard RBR plugin headers and source files
	- TestPlugin.h - Standard RBR plugin sample file WITH few additional code lines to draw custom images
	- NGPCarMenuAPI.h - API integration class (include this file in your plugin sources) to draw custom images (requires that RBR has NGPCarMenu.dll plugin in use)
	
Instructions:

This sample is 99% identical with the "normal" RBR sample plugin sources. There are only additional NGPCarMenuAPI.h header and few extra lines in TestPlugin.h header file.

Include NGPCarMenuAPI.h header into a plugin implementation and you are ready to draw custom images in your own plugin menus.
The runtime RBR installation does require NGPCarMenu.dll (V1.10 or newer) plugin in use because the API interface in this sample plugin uses API services from the NGPCarMenu plugin.
If NGPCarMenu.dll plugin is missing then API interface does nothing (your own plugin works but without custom images).

See https://github.com/mika-n/NGPCarMenu for more info about the NGPCarMenu plugin.

This "SamplePlug1" plugin is provided free of charge. Feel free to use and copy the sample code in your own projects. Use at your own risk. No warranty given.
