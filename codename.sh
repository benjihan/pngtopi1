#!/bin/bash -fe
#
# by Benjamin Gerard
#
# Transform version number to codename.
#
# ----------------------------------------------------------------------
#

export LC_ALL=C

codelist=( TotalBlack NavyBlue DarkBlue MediumBlue DarkGreen WebGreen
	   DarkCyan DarkTurquoise X11Green SpringGreen MidnightBlue
	   DodgerBlue ForestGreen SeaGreen LimeGreen RoyalBlue
	   SteelBlue MediumTurquoise CadetBlue CornflowerBlue
	   RebeccaPurple MediumAquamarine DimGrey SlateBlue OliveDrab
	   SlateGrey LawnGreen WebMaroon WebPurple WebGrey SkyBlue
	   BlueViolet DarkRed DarkMagenta SaddleBrown LightGreen
	   MediumPurple DarkViolet PaleGreen DarkOrchid YellowGreen
	   X11Purple DarkGrey LightBlue GreenYellow PaleTurquoise
	   X11Maroon PowderBlue DarkGoldenrod MediumOrchid RosyBrown
	   DarkKhaki X11Grey IndianRed VioletRed LightGrey LightCyan
	   DarkSalmon LightGoldenrod PaleGoldenrod LightCoral
	   AliceBlue SandyBrown WhiteSmoke MintCream GhostWhite
	   AntiqueWhite OldLace DeepPink OrangeRed HotPink DarkOrange
	   LightSalmon LightPink PeachPuff NavajoWhite MistyRose
	   BlanchedAlmond PapayaWhip LavenderBlush LemonChiffon
	   FloralWhite LightYellow PureWhite )

max=${#codelist[@]}

if [ $# != 1 ]; then
    echo "codename.sh: this program require a single argument" >&2
    exit 1
elif [[ "$1" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)(\.[0-9]+)?$ ]]; then
    version="v${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.${BASH_REMATCH[3]}"
else
    echo "codename.sh: invalid argument -- '$1'" >&2
    exit 1
fi

n=$(git tag -l 'v[0-9]*'|grep -n "^${version}\$"|cut -d: -f1)

if [ -z "$n" ]; then
    echo "codename.sh: version not found -- '$1'" >&2
    exit 2
fi

echo "${codelist[$((n%max))]}"
