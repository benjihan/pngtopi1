#!/bin/sh -fe
#
# by Benjamin Gerard
#
# Count git commit since last tag (AKA TWEAK number)
#
# ----------------------------------------------------------------------
#

export LC_ALL=C

if [ -r ./VERSION ]; then
    cat ./VERSION
else
    cd "$(dirname $0)"
    if [ -r ./VERSION ]; then
	cat ./VERSION
    else
	which git >/dev/null
	tagname=$(git tag -l 'v[0-9]*' --sort=-v:refname | head -n1)
	if [ "$tagname" ]; then
	    tweaks=$(git rev-list --count HEAD ^${tagname})
	else
	    tweaks=$(git rev-list --count HEAD)
	    tagname='v0.0'
	fi
	if [ "${tweaks}" = 0 ]; then
	    tweaks=''
	else
	    tweaks=".${tweaks}"
	fi	    
	echo "$tagname" |
	    sed -e "s/^v\([0-9].*\)/\1${tweaks}/"
    fi
fi
