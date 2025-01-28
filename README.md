# XLINK Conversion Tool

## Usage

Make sure to have a recent version Microsoft Visual C++ Redistributable installed on Windows.

### Converting from XLNK to YAML

```sh
xlink_tool -e [path_to_xlnk_file] [output_yaml_path] [path_to_zsdic_pack] # last parameter is optional if the input file is not compressed
```

### Converting from YAML to XLNK

```sh
xlink_tool -i [path_to_yaml_file] [output_xlnk_path] [path_to_zsdic_pack] # last parameter is optional if the output shouldn't be compressed
```

## Building

Building from source is not required to use this tool, there are precompiled binaries in Releases.

###

Building requires CMake 3.23+ and a C++ 20 compiler

```sh
git clone https://github.com/dt-12345/xlink2.git
git submodule update --init --recursive
# Feel free to adjust these parameters to your liking
cmake -B build -DCMAKE_BUILD_TYPE=Release -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_PROGRAMS=OFF
cmake --build build
```

## the messy part of the README with hastily explained details

very experimental at the moment, there's not a good editing interface or anything

editing technically works but you'd need to do it all in code

serialization is almost byte perfect if I could just figure out how they sorted duplicate asset keys... (serialization is not byte perfect if loading from YAML however, there is extra unused data in the file that is unreference and that is not included in the YAML)

thanks shadow for your research most of this is based off of (https://github.com/shadowninja108/woomlink)

also thanks to leoetlino for the YAML parsing/writing utilities from oead [oead](https://github.com/zeldamods/oead)

anyways, how does this file work?

xlink2 is Nintendo EPD's library that handles the emission of sounds and effects (via SLink and ELink, respectively) - this file acts as a database that defines how the xlink2 system will behave (it's xlink2 because the old version of the library is called xlink or something)

xlink2 operates with the concept of "users" - users have a set of assets (sounds/effects) they can emit with various parameters controlling different parts of the process (such as color for effects or volume for sounds)

what parameters are possible is defined in the aptly named Param Define Table

xlink2 also has "properties" which can be either global or local - these properties are defined in `XLinkPropertyTable.Product.121.rstbl.byml` and can affect the behavior of users

users have a set of "asset call tables" that can either represent an "asset" or a "container" - an asset is simply the sound/effect in question while a container represents a more complex condition (such as a switch case or playing effects/sounds in sequence) that contains child containers/assets

each asset call table has a key which is what is searched for when emitting any sound or effect

asset call tables have a set of parameters that specify how exactly the asset should be emitted (such as the name of the asset to play or the name of the bone to emit the asset from)

asset call tables can also have conditions which cause them to only play if said condition is met

users also have "triggers" which are triggered automatically when a certain trigger condition is met - these come in three types: ActionTriggers, PropertyTriggers, and AlwaysTriggers

ActionTriggers are dependent on actions + action slots - an action slot is somewhat like an external property whose value is an action

ActionTriggers are triggered when the corresponding action slot's value is the specified action (these actions can be set by code or in asb files to occur in tandem with animations)

PropertyTriggers are dependent on a specific property (same properties as mentioned before) - they trigger whenever the corresponding property meets the specified condition value

AlwaysTriggers are, as the name suggests, always and continuously triggered

each of the triggers has an associated asset call table which determines what they actually trigger

triggers can also have associated "trigger overwrite parameter" which overwrite parameters associated with their asset call table when they are called through the trigger