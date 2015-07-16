/***************************************
 OSM router using libroutino library.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2008-2015 Andrew M. Bishop

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************/


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "routino.h"


/*+ The maximum distance from the specified point to search for a node or segment (in km). +*/
#define MAXSEARCH  1

/*+ The maximum number of waypoints +*/
#define NWAYPOINTS 99


/* Local functions */

static char *FileName(const char *dirname,const char *prefix, const char *name);
static void print_usage(int detail,const char *argerr,const char *err);


/*++++++++++++++++++++++++++++++++++++++
  The main program for the router.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 Routino_Database    *database;
 Routino_Profile     *profile;
 Routino_Translation *translation;
 Routino_Waypoint   **waypoints;
 int                  point_used[NWAYPOINTS+1]={0};
 double               point_lon[NWAYPOINTS+1],point_lat[NWAYPOINTS+1];
 char                *dirname=NULL,*prefix=NULL;
 char                *profiles=NULL,*profilename="motorcar";
 char                *translations=NULL,*language="en";
 int                  reverse=0,loop=0;
 int                  quickest=0;
 int                  html=0,gpx_track=0,gpx_route=0,text=0,text_all=0,none=0,stdout=0;
 int                  arg;
 int                  first_waypoint=NWAYPOINTS,last_waypoint=1,inc_dec_waypoint,waypoint,nwaypoints=0;

 /* Parse the command line arguments */

 if(argc<2)
    print_usage(0,NULL,NULL);

 /* Get the non-routing, general program options */

 for(arg=1;arg<argc;arg++)
   {
    if(!strcmp(argv[arg],"--help"))
       print_usage(1,NULL,NULL);
    else if(!strncmp(argv[arg],"--dir=",6))
       dirname=&argv[arg][6];
    else if(!strncmp(argv[arg],"--prefix=",9))
       prefix=&argv[arg][9];
    else if(!strncmp(argv[arg],"--profiles=",11))
       profiles=&argv[arg][11];
    else if(!strncmp(argv[arg],"--translations=",15))
       translations=&argv[arg][15];
    else if(!strcmp(argv[arg],"--reverse"))
       reverse=1;
    else if(!strcmp(argv[arg],"--loop"))
       loop=1;
    else if(!strcmp(argv[arg],"--output-html"))
       html=1;
    else if(!strcmp(argv[arg],"--output-gpx-track"))
       gpx_track=1;
    else if(!strcmp(argv[arg],"--output-gpx-route"))
       gpx_route=1;
    else if(!strcmp(argv[arg],"--output-text"))
       text=1;
    else if(!strcmp(argv[arg],"--output-text-all"))
       text_all=1;
    else if(!strcmp(argv[arg],"--output-none"))
       none=1;
    else if(!strcmp(argv[arg],"--output-stdout"))
      { stdout=1; }
    else if(!strncmp(argv[arg],"--profile=",10))
       profilename=&argv[arg][10];
    else if(!strncmp(argv[arg],"--language=",11))
       language=&argv[arg][11];
    else if(!strcmp(argv[arg],"--shortest"))
       quickest=0;
    else if(!strcmp(argv[arg],"--quickest"))
       quickest=1;
    else if(!strncmp(argv[arg],"--lon",5) && isdigit(argv[arg][5]))
      {
       int point;
       char *p=&argv[arg][6];

       while(isdigit(*p)) p++;
       if(*p++!='=')
          print_usage(0,argv[arg],NULL);
 
       point=atoi(&argv[arg][5]);
       if(point>NWAYPOINTS || point_used[point]&1)
          print_usage(0,argv[arg],NULL);
 
       point_lon[point]=atof(p);
       point_used[point]+=1;

       if(point<first_waypoint)
          first_waypoint=point;
       if(point>last_waypoint)
          last_waypoint=point;
      }
    else if(!strncmp(argv[arg],"--lat",5) && isdigit(argv[arg][5]))
      {
       int point;
       char *p=&argv[arg][6];

       while(isdigit(*p)) p++;
       if(*p++!='=')
          print_usage(0,argv[arg],NULL);
 
       point=atoi(&argv[arg][5]);
       if(point>NWAYPOINTS || point_used[point]&2)
          print_usage(0,argv[arg],NULL);
 
       point_lat[point]=atof(p);
       point_used[point]+=2;

       if(point<first_waypoint)
          first_waypoint=point;
       if(point>last_waypoint)
          last_waypoint=point;
      }
    else
       print_usage(0,argv[arg],NULL);

    argv[arg]=NULL;
   }

 /* Check the specified command line options */

 if(stdout && (html+gpx_track+gpx_route+text+text_all)!=1)
   {
    fprintf(stderr,"Error: The '--output-stdout' option requires exactly one other output option (but not '--output-none').\n");
    exit(EXIT_FAILURE);
   }

 /* Load in the selected profiles */

 if(profiles)
   {
    if(access(profiles,F_OK))
      {
       fprintf(stderr,"Error: The '--profiles' option specifies a file that does not exist.\n");
       exit(EXIT_FAILURE);
      }
   }
 else
   {
    profiles=FileName(dirname,prefix,"profiles.xml");

    if(access(profiles,F_OK))
      {
       free(profiles);

       profiles=FileName(ROUTINO_DATADIR,NULL,"profiles.xml");

       if(access(profiles,F_OK))
         {
          fprintf(stderr,"Error: The '--profiles' option was not used and the default 'profiles.xml' does not exist.\n");
          exit(EXIT_FAILURE);
         }
      }
   }

 if(!profilename)
   {
    fprintf(stderr,"Error: A profile name must be specified.\n");
    exit(EXIT_FAILURE);
   }

 if(Routino_ParseXMLProfiles(profiles))
   {
    fprintf(stderr,"Error: Cannot read the profiles in the file '%s'.\n",profiles);
    exit(EXIT_FAILURE);
   }

 profile=Routino_GetProfile(profilename);

 if(!profile)
   {
    fprintf(stderr,"Error: Cannot find a profile called '%s' in '%s'.\n",profilename,profiles);
    exit(EXIT_FAILURE);
   }

 /* Load in the selected translation */

  if(translations)
    {
     if(access(translations,F_OK))
       {
        fprintf(stderr,"Error: The '--translations' option specifies a file that does not exist.\n");
        exit(EXIT_FAILURE);
       }
    }
  else
    {
     translations=FileName(dirname,prefix,"translations.xml");

     if(access(translations,F_OK))
       {
        free(translations);

        translations=FileName(ROUTINO_DATADIR,NULL,"translations.xml");

        if(access(translations,F_OK))
          {
           fprintf(stderr,"Error: The '--translations' option was not used and the default 'translations.xml' does not exist.\n");
           exit(EXIT_FAILURE);
          }
       }
    }

  if(Routino_ParseXMLTranslations(translations))
    {
     fprintf(stderr,"Error: Cannot read the translations in the file '%s'.\n",translations);
     exit(EXIT_FAILURE);
    }

  translation=Routino_GetTranslation(language);

  if(!translation)
    {
     fprintf(stderr,"Warning: Cannot find a translation called '%s' in '%s'.\n",language,translations);
     exit(EXIT_FAILURE);
   }

 /* Check the waypoints are valid */

 for(waypoint=first_waypoint;waypoint<=last_waypoint;waypoint++)
    if(point_used[waypoint]==1 || point_used[waypoint]==2)
       print_usage(0,NULL,"All waypoints must have latitude and longitude.");
    else if(point_used[waypoint]==3)
       nwaypoints++;

 if(first_waypoint>=last_waypoint)
   {
    fprintf(stderr,"Error: At least two waypoints must be specified.\n");
    exit(EXIT_FAILURE);
   }

 waypoints=calloc(sizeof(Routino_Waypoint*),nwaypoints+2);

 /* Load in the routing database */

 database=Routino_LoadDatabase(dirname,prefix);

 /* Check the profile is valid for use with this database */

 Routino_ValidateProfile(database,profile);

 /* Check for reverse direction */

 if(reverse)
   {
    int temp;

    temp=first_waypoint;
    first_waypoint=last_waypoint;
    last_waypoint=temp;

    last_waypoint--;

    inc_dec_waypoint=-1;
   }
 else
   {
    last_waypoint++;

    inc_dec_waypoint=1;
   }

 /* Loop through all waypoints */

 nwaypoints=0;

 for(waypoint=first_waypoint;waypoint!=last_waypoint;waypoint+=inc_dec_waypoint)
    if(point_used[waypoint]==3)
      {
       waypoints[nwaypoints]=Routino_FindWaypoint(database,profile,point_lat[waypoint],point_lon[waypoint]);

       if(!waypoints[nwaypoints])
         {
          fprintf(stderr,"Error: Cannot find node close to specified point %d.\n",waypoint);
          exit(EXIT_FAILURE);
         }

       nwaypoints++;
      }

 if(loop)
    waypoints[nwaypoints++]=waypoints[0];

 /* Create the route */

 if(quickest)
    Routino_Quickest();
 else
    Routino_Shortest();

 if(Routino_CalculateRoute(database,profile,translation,waypoints,nwaypoints))
   {
    fprintf(stderr,"Error: Cannot find a route between specified waypoints.\n");
    exit(EXIT_FAILURE);
   }

 /* Tidy up and exit */

 Routino_UnloadDatabase(database);

 Routino_FreeXMLProfiles();

 Routino_FreeXMLTranslations();

 for(waypoint=0;waypoint<nwaypoints;waypoint++)
    free(waypoints[waypoint]);

 free(waypoints);

 exit(EXIT_SUCCESS);
}


/*++++++++++++++++++++++++++++++++++++++
  Return a filename composed of the dirname, prefix and name.

  char *FileName Returns a pointer to memory allocated to the filename.

  const char *dirname The directory name.

  const char *prefix The file prefix.

  const char *name The main part of the name.
  ++++++++++++++++++++++++++++++++++++++*/

static char *FileName(const char *dirname,const char *prefix, const char *name)
{
 char *filename=(char*)malloc((dirname?strlen(dirname):0)+1+(prefix?strlen(prefix):0)+1+strlen(name)+1);

 sprintf(filename,"%s%s%s%s%s",dirname?dirname:"",dirname?"/":"",prefix?prefix:"",prefix?"-":"",name);

 return(filename);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the usage information.

  int detail The level of detail to use - 0 = low, 1 = high.

  const char *argerr The argument that gave the error (if there is one).

  const char *err Other error message (if there is one).
  ++++++++++++++++++++++++++++++++++++++*/

static void print_usage(int detail,const char *argerr,const char *err)
{
 fprintf(stderr,
         "Usage: router [--help ]\n"
         "              [--dir=<dirname>] [--prefix=<name>]\n"
         "              [--profiles=<filename>] [--translations=<filename>]\n"
         "              [--language=<lang>]\n"
         "              [--output-html]\n"
         "              [--output-gpx-track] [--output-gpx-route]\n"
         "              [--output-text] [--output-text-all]\n"
         "              [--output-none] [--output-stdout]\n"
         "              [--profile=<name>]\n"
         "              [--shortest | --quickest]\n"
         "              --lon1=<longitude> --lat1=<latitude>\n"
         "              --lon2=<longitude> --lon2=<latitude>\n"
         "              [ ... --lon99=<longitude> --lon99=<latitude>]\n"
         "              [--reverse] [--loop]\n");

 if(argerr)
    fprintf(stderr,
            "\n"
            "Error with command line parameter: %s\n",argerr);

 if(err)
    fprintf(stderr,
            "\n"
            "Error: %s\n",err);

 if(detail)
    fprintf(stderr,
            "\n"
            "--help                  Prints this information.\n"
            "\n"
            "--dir=<dirname>         The directory containing the routing database.\n"
            "--prefix=<name>         The filename prefix for the routing database.\n"
            "--profiles=<filename>   The name of the XML file containing the profiles\n"
            "                        (defaults to 'profiles.xml' with '--dir' and\n"
            "                         '--prefix' options or the file installed in\n"
            "                         '" ROUTINO_DATADIR "').\n"
            "--translations=<fname>  The name of the XML file containing the translations\n"
            "                        (defaults to 'translations.xml' with '--dir' and\n"
            "                         '--prefix' options or the file installed in\n"
            "                         '" ROUTINO_DATADIR "').\n"
            "\n"
            "--language=<lang>       Use the translations for specified language.\n"
            "--output-html           Write an HTML description of the route.\n"
            "--output-gpx-track      Write a GPX track file with all route points.\n"
            "--output-gpx-route      Write a GPX route file with interesting junctions.\n"
            "--output-text           Write a plain text file with interesting junctions.\n"
            "--output-text-all       Write a plain test file with all route points.\n"
            "--output-none           Don't write any output files or read any translations.\n"
            "                        (If no output option is given then all are written.)\n"
            "--output-stdout         Write to stdout instead of a file (requires exactly\n"
            "                        one output format option, implies '--quiet').\n"
            "\n"
            "--profile=<name>        Select the loaded profile with this name.\n"
            "\n"
            "--shortest              Find the shortest route between the waypoints.\n"
            "--quickest              Find the quickest route between the waypoints.\n"
            "\n"
            "--lon<n>=<longitude>    Specify the longitude of the n'th waypoint.\n"
            "--lat<n>=<latitude>     Specify the latitude of the n'th waypoint.\n"
            "\n"
            "--reverse               Find a route between the waypoints in reverse order.\n"
            "--loop                  Find a route that returns to the first waypoint.\n"
            "\n");

 exit(!detail);
}
