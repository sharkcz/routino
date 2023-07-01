                  Routino : OpenStreetMap Routing Software
                  ========================================


   Routino is an application for finding a route between two points using
   the dataset of topographical information collected by
   http://www.OpenStreetMap.org.

   Starting from the raw OpenStreetMap data (in the form of the '.osm' XML
   files available on the internet) a custom database is generated that
   contains the information useful for routing. With this database and two
   points specified by latitude and longitude an optimum route (either
   shortest or quickest) is determined. The route is calculated for
   OpenStreetMap highways (roads, paths etc) using one of the common forms
   of transport defined in OpenStreetMap (foot, bicycle, horse, motorcar,
   motorcycle etc).

   When processing the OpenStreetMap data the types of highways are
   recorded and these set default limits on the types of traffic allowed.
   More specific information about permissions for different types of
   transport are also recorded as are maximum speed limits. Further
   restrictions like one-way streets, weight, height, width and length
   limits are also included where specified. Additionally a set of
   properties of each highway are also recorded. The processing of the
   input file is controlled by a configuration file which determines the
   information that is used.

   When calculating a route the type of transport to be used is taken into
   account to ensure that the known restrictions are followed. Each of the
   different highway types can further be allowed or disallowed depending
   on preferences. For each type of highway a default speed limit is
   defined (although the actual speed used will be the lowest of the
   default and any specified in the original data). To make use of the
   information about restrictions the weight, height, width and length of
   the transport can also be specified. Further preferences about road
   properties (e.g. paved or not) can also be selected. The simplest type
   of turn restrictions (those formed from an initial way, a node and a
   second way) are also obeyed.

   The result of calculating the route can be presented in several
   different ways. An HTML file can be produced that contains a
   description of the route to take with instructions for each of the
   important junctions. The contents of the file are created based on a
   set of translations specified in a configuration file. The route is
   also available in a GPX (GPS eXchange) XML format. format file
   containing either every point and highway segment (a track file) or
   just a waypoint and translated instructions for the important junctions
   (a route file). Additionally there are two plain text files that
   contain all data points or just the important ones (intended for
   debugging and further processing).

   One of the design aims of Routino was to make the software are flexible
   as possible in selecting routing preferences but also have a sensible
   set of default values. Another design aim was that finding the optimum
   route should be very fast and most of the speed increases come from the
   carefully chosen and optimised data format.


Disclaimer
----------

   The route that is calculated by this software is only as good as the
   input data.

   Routino comes with ABSOLUTELY NO WARRANTY for the software itself or
   the route that is calculated by it.


Demonstration
-------------

   A live demonstration of the router for the UK is available on the
   internet in both OpenLayers and Leaflet versions:

   http://www.routino.org/uk-leaflet/
   http://www.routino.org/uk-openlayers2/
   http://www.routino.org/uk-openlayers/

   The source code download available below also includes a set of files
   that can be used to create your own interactive map.

   The interactive map is made possible by use of the OpenLayers or
   Leaflet Javascript library from http://www.openlayers.org/ or
   http://www.openlayers.org/two/ or http://leafletjs.com/.


Documentation
-------------

   The algorithm used is described in the file ALGORITHM.txt with some notes
   about the input data in DATA.txt and numerical limitations in LIMITS.txt.

   The configuration files and in particular the default set of rules for
   processing the OpenStreetMap data tags are described in detail in
   CONFIGURATION.txt and TAGGING.txt.  The format of the output files
   generated are described in OUTPUT.txt.

   Detailed information about how to use the programs is available in the
   file USAGE.txt and how to install it is in INSTALL.txt.


Status
------

   Version 1.0 of Routino was released on 8th April 2009.
   Version 2.0 of Routino was released on 30th May 2011.
   Version 3.0 of Routino was released on 12th September 2015.
   Version 3.1 of Routino was released on 5th March 2016.
   Version 3.1.1 of Routino was released on 6th March 2016.
   Version 3.2 of Routino was released on 12th March 2017.
   Version 3.3 of Routino was released on 7th September 2019.
   Version 3.3.1 of Routino was released on 8th September 2019.
   Version 3.3.2 of Routino was released on 18th September 2019.
   Version 3.3.3 of Routino was released on 30th December 2020.
   Version 3.4 of Routino was released on 11th June 2023.
   Version 3.4.1 of Routino was released on 1st July 2023.

   The full version history is available in the NEWS.txt file.


Other Versions
- - - - - - -

   There is a version of Routino (in subversion, on the branch called
   "destination-access") that allows the first and last waypoint of a
   route to be on highways with access="destination" or access="private".
   The database is not compatible with this version of Routino.


License
-------

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU Affero General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   It is important to note that for this program I have decided to use the
   Affero GPLv3 instead of just using the GPL. This license adds
   additional requirements to anybody who provides a networked service
   using this software.


Copyright
---------

   Routino is copyright Andrew M. Bishop 2008-2023.


Homepage
--------

   The Routino homepage has the latest news about the program:

   http://www.routino.org/


Download
--------

   The program can be downloaded from:

   http://www.routino.org/download/

Subversion
- - - - -

   The source code can also be downloaded from the Subversion repository
   with a command like the following:

   svn co http://routino.org/svn/trunk routino

   The source code can also be browsed in the Subversion viewer which also
   has a list of the latest changes:

   http://www.routino.org/viewvc/trunk/
   http://www.routino.org/viewvc/trunk/?view=log


--------

Copyright 2008-2023 Andrew M. Bishop.
