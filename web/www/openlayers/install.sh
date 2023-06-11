#!/bin/sh -x

# Openlayers version (https://openlayers.org/)

version=7.4.0

# Layer switcher version (https://github.com/walkermatt/ol-layerswitcher/)

layer_switcher_version=4.1.1


# Download the file.

wget https://github.com/openlayers/openlayers/releases/download/v$version/v$version-package.zip

# Uncompress it.

unzip v$version-package.zip -d v$version-package


# Move the files

mv v$version-package/dist/* .
mv v$version-package/ol.css .
rm -rf v$version-package


# Download the file

wget https://github.com/walkermatt/ol-layerswitcher/archive/v$layer_switcher_version.zip

# Uncompress it.

unzip v$layer_switcher_version.zip


# Move the files

mv ol-layerswitcher-$layer_switcher_version/dist/ol-layerswitcher.js .
mv ol-layerswitcher-$layer_switcher_version/src/ol-layerswitcher.css .

rm -rf ol-layerswitcher-$layer_switcher_version
