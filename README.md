# Hmapper

Developed by Eduard Roccatello

![3DGIS](http://www.3dgis.it/wp-content/themes/gis2013/images/3dgis.png)

## Overview

**Hmapper** is a useful converter from Grayscale GeoTIFFs to RGB GeoTIFFs.

It is extremely useful if you would like to distribute raster heightmaps using a WMS service, currently limited to 8 bits per channel.

Using Hmapper you are able to deliver a maximum height of about 167.7km (0xFFFFFF) with 1cm resolution.

If you need more resolution check out the next paragraph.


## Resolution - Good to know

Height data is multiplied by **OUT_FP_MULTIPLIER** (100 by default) in order to deliver a better precision (centimeters).

This is hard coded right now so... fork me and make it an option :)

## Requirements
* libtiff
* libgeotiff

On OS X just do:

`brew install libgeotiff`


### More?

Follow [@edurocc](http://twitter.com/edurocc) on Twitter.

