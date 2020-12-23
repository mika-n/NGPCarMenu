NGPCarMenu - RaceStatDB database
--------------------------------

NGPCarMenu RBR plugin stores rally results with additional details for analysing purposes (tyre-weather-stage setups, penalties, split times, etc).
The plugin shows a summary of rally results on the RBR main menu (RBR/TM and BTB tracks), RBRTM main menu (RBR/TM tracks) and RBRRX stage list (BTB tracks).

The racestatDB has the following table structure for maps, cars and rally results (disclaimer: This is work-in-progress feature, so at some point there may be new features not listed here).
See Plugins\NGPCarMenu.ini file and RaceStatDB configuration option there for more details.

Even when NGPCarMenu plugin shows summary of this data within RBR game you can also access the detailed data using SQLiteStudio (https://sqlitestudio.pl/) query tool to do more detailed analysis.
If you know the basics of SQL query language then the table structure is self-explanatory.

TABLES:

* StatInfo             Version of the raceStatDB content

* D_Map                List of stages. NGPCarMenu adds new map details as soon you complete a rally.
    MapKey                Unique key
    MapID                 RBRTM/RSF/RBR track ID number (or -1 if RBRRX/BTB map)
    StageName
    Surface               Tarmac/Gravel/Snow track
    Length
    Format                Format of the track model data (classic RBR or BTB)
    RBRInstallType        RBRTM or RallysimFans RFS or vanilla RBR installation type using this map (note! In theory the same stage name may be in RBRTM/RSF/BTB with slight differences)
    ..other..

* D_Car                List of cars. NGPCarMenu adds new car details as soon you complete a rally.
    CarKey                Unique key
    ModelName             The name of the car model
    FIACategory           The FIA category (group)
    Revision              The revision of the NGP physics file
    NGPVersion            Version of the NGP library
    ..other..

* F_RallyResult        Rally results
     RaceDate              Date when the rally was driven (YYYYMMDD)
     RaceDateTime          Time when the rally was driven (HHMISS)
     MapKey                Reference key to D_Map.MapKey
     CarKey                Reference key to D_Car.CarKey
     Split1/Split2Time     Split1 and Split2 times (or NULL if the car was retired before reacing the split)
     FinishTime            The final finish time of the rally (NULL if the car retired or quit the race)
     FalseStartPenaltyTime Penalty time given from a false start (0 if no false start)
     OtherPenaltyTime      Total of call for help and cut penalties (penalty times are already included in FinishTime)   
     ..other car and stage attributes..  (transmission, tyre, weather, damage model, surface)

     Note! If the rally is retired "too early" then the data is not added here (the car didn't reach even the split1 or the stage was retired soon after starting line).


VIEWS:

* V_RallyResultSummary 
    View joining F_RallyResult/D_Car/D_Map tables to show a summary of rallies with stage/car/FIACategoryGroup records in the stage.

----------------------------------------------------------------------------------------
Copyright (C) 2020 MIKA-N. All rights reserved. See the license of NGPCarMenu plugin. 
https://github.com/mika-n/NGPCarMenu
