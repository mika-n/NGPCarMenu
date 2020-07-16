// RBRTestPlugin

Files
	- IPlugin.h : The plugin interface use by RBR
	- IRBRGame.h : Game interface that the plugin
	- PluginHelpers.h : Some help functions for the plugins
	
	- TestPlugin.h - Implementation of the rbr test plugin
	
Instructions:
Created plugins must be put in the Plugins directory under the game root ( i.e C:\Program Files\SCi Games\Richard Burns Rally\Plugins ).
The plugins that are found under the are listed under Options\Plugins menu in RBR. When a stage is started from the plugin,
only the will the plugin recieve the callbacks for stage started, checkpoints etc. When the players finished the stage, 
or retires, he's taken back to the plugins menupage. 
