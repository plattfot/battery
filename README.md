# Battery - battery monitor for [i3blocks](https://github.com/vivien/i3blocks)

Battery is a small program written in C++, that is using [Font Awesome](http://fontawesome.io/)
to show info about the battery. It is designed to be used with
i3blocks and based on the battery monitor for [i3status](https://github.com/i3/i3status).

## Table of content
- [Features](#features)
- [Installation](#installation)
  - [From source](#from-source)
- [Usage](#usage)
- [Limitations](#limitations)

## Features
It shares some of the key features of i3status' battery monitor, but
also has some unqiue features.
- Using Font Awesome for a more compact view.
- Option to show the battery charge with battery icon, hearts or user
  specified.
- Better support for multiple batteries.
- Better time estimation when charging if using a worn battery.
- Clicking on the block with the left mouse button will show the
  charge in percent.
- If using multiple batteries and option to combine the capacities is
  selected, you can use the middle and right mouse button to get info
  about the current battery in use.

It lacks some of the features that i3status has, for example no option
to specify if to use the design battery capacity or current max
capacity when computing the current charge. It will always use the
design battery capacity.

## Installation

### From source
Clone the repo, then simply run this in the root directory:
```bash
make install PREFIX=<install dir>
```
Were <install dir> is where you want to place the executable; Default
is ~/.i3/custom/.  In order to build the executable you will need a
compiler with C++14 support.  The code only depends on STL and cstdlib
which should ship with your compiler.

## Usage
In your i3blocks.conf file add this:
```conf
[battery]
command=$HOME/.i3/custom/battery
interval=10
min_width= 100% 00:00
```
Replace the "$HOME/.i3/custom" with the path you specified when you
installed the executable.

Battery's default options are:

- Show total battery capacity i.e. combine all batteries it finds in
  /sys/class/power_supply.
- Warn when battery capacity goes below 10%, by color the block red.
- Use battery icon to display battery capacity.

Run "battery --help" for more info.

## Limitations

This program is only tested on a lenovo x220 with an extra battery
pack, running Arch linux 2015-11-22. 


