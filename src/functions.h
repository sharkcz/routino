/***************************************
 $Header: /home/amb/CVS/routino/src/functions.h,v 1.1 2008-12-31 12:20:17 amb Exp $

 Header file for function prototypes
 ******************/ /******************
 Written by Andrew M. Bishop

 This file Copyright 2008 Andrew M. Bishop
 It may be distributed under the GNU Public License, version 2, or
 any higher version.  See section COPYING of the GNU Public license
 for conditions under which this file may be redistributed.
 ***************************************/


#ifndef FUNCTIONS_H
#define FUNCTIONS_H    /*+ To stop multiple inclusions. +*/

#include <stdio.h>

#include "types.h"


/* In osmparser.c */

int ParseXML(FILE *file);


/* In nodes.c */

int NewNodeList(void);

int LoadNodeList(const char *filename);
int SaveNodeList(const char *filename);

Node *FindNode(node_t id);

void AppendNode(node_t id,latlong_t latitude,latlong_t longitude);
void SortNodeList(void);


/* In segments.c */

int NewSegmentList(void);

int LoadSegmentList(const char *filename);
int SaveSegmentList(const char *filename);

Segment *FindFirstSegment(node_t node);
Segment *FindNextSegment(Segment *segment);

void AppendSegment(node_t node1,node_t node2,way_t way,distance_t distance,duration_t duration);
void SortSegmentList(void);

distance_t SegmentLength(Node *node1,Node *node2);


/* In files.c */

void *MapFile(const char *filename);

void UnMapFile(void *address);

int WriteFile(const char *filename,void *address,size_t length);


/* In optimiser.c */

void FindRoute(node_t start,node_t finish);
void PrintRoute(node_t start,node_t finish);

#endif /* FUNCTIONS_H */
