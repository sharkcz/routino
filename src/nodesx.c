/***************************************
 Extented Node data type functions.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2008-2015, 2019, 2020, 2022 Andrew M. Bishop

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


#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "nodes.h"

#include "typesx.h"
#include "nodesx.h"
#include "segmentsx.h"
#include "waysx.h"

#include "files.h"
#include "logging.h"
#include "sorting.h"


/* Global variables */

/*+ The command line '--tmpdir' option or its default value. +*/
extern char *option_tmpdirname;

/* Local variables */

/*+ Temporary file-local variables for use by the sort functions (re-initialised for each sort). +*/
static NodesX *sortnodesx;
static latlong_t lat_min,lat_max,lon_min,lon_max;

/* Local functions */

static int sort_by_id(NodeX *a,NodeX *b);
static int deduplicate_and_index_by_id(NodeX *nodex,index_t index);

static int update_id(NodeX *nodex,index_t index);
static int sort_by_lat_long(NodeX *a,NodeX *b);
static int index_by_lat_long(NodeX *nodex,index_t index);


/*++++++++++++++++++++++++++++++++++++++
  Allocate a new node list (create a new file or open an existing one).

  NodesX *NewNodeList Returns a pointer to the node list.

  int append Set to 1 if the file is to be opened for appending.

  int readonly Set to 1 if the file is to be opened for reading.
  ++++++++++++++++++++++++++++++++++++++*/

NodesX *NewNodeList(int append,int readonly)
{
 NodesX *nodesx;

 nodesx=(NodesX*)calloc_logassert(1,sizeof(NodesX));

 nodesx->filename    =(char*)malloc_logassert(strlen(option_tmpdirname)+32);
 nodesx->filename_tmp=(char*)malloc_logassert(strlen(option_tmpdirname)+40); /* allow %p to be up to 20 bytes */

 sprintf(nodesx->filename    ,"%s/nodesx.parsed.mem",option_tmpdirname);
 sprintf(nodesx->filename_tmp,"%s/nodesx.%p.tmp"    ,option_tmpdirname,(void*)nodesx);

 if(append || readonly)
    if(ExistsFile(nodesx->filename))
      {
       offset_t size;

       size=SizeFile(nodesx->filename);

       nodesx->number=size/sizeof(NodeX);

       RenameFile(nodesx->filename,nodesx->filename_tmp);
      }

 if(append)
    nodesx->fd=OpenFileBufferedAppend(nodesx->filename_tmp);
 else if(!readonly)
    nodesx->fd=OpenFileBufferedNew(nodesx->filename_tmp);
 else
    nodesx->fd=-1;

#if SLIM
 nodesx->cache=NewNodeXCache();
 log_malloc(nodesx->cache,sizeof(*nodesx->cache));
#endif

 nodesx->ifilename_tmp=(char*)malloc_logassert(strlen(option_tmpdirname)+40); /* allow %p to be up to 20 bytes */

 sprintf(nodesx->ifilename_tmp,"%s/nodesx.%p.idx.tmp",option_tmpdirname,(void*)nodesx);

 return(nodesx);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a node list.

  NodesX *nodesx The set of nodes to be freed.

  int keep If set then the results file is to be kept.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeNodeList(NodesX *nodesx,int keep)
{
 if(keep)
    RenameFile(nodesx->filename_tmp,nodesx->filename);
 else
    DeleteFile(nodesx->filename_tmp);

 free(nodesx->filename);
 free(nodesx->filename_tmp);

 DeleteFile(nodesx->ifilename_tmp);

 free(nodesx->ifilename_tmp);

 if(nodesx->gdata)
   {
    log_free(nodesx->gdata);
    free(nodesx->gdata);
   }

 if(nodesx->pdata)
   {
    log_free(nodesx->pdata);
    free(nodesx->pdata);
   }

 if(nodesx->super)
   {
    log_free(nodesx->super);
    free(nodesx->super);
   }

#if SLIM
 log_free(nodesx->cache);
 DeleteNodeXCache(nodesx->cache);
#endif

 free(nodesx);
}


/*++++++++++++++++++++++++++++++++++++++
  Append a single node to an unsorted node list.

  NodesX *nodesx The set of nodes to modify.

  node_t id The node identifier from the original OSM data.

  double latitude The latitude of the node.

  double longitude The longitude of the node.

  transports_t allow The allowed traffic types through the node.

  nodeflags_t flags The flags to set for this node.
  ++++++++++++++++++++++++++++++++++++++*/

void AppendNodeList(NodesX *nodesx,node_t id,double latitude,double longitude,transports_t allow,nodeflags_t flags)
{
 NodeX nodex={0};

 nodex.id=id;
 nodex.latitude =radians_to_latlong(latitude);
 nodex.longitude=radians_to_latlong(longitude);
 nodex.allow=allow;
 nodex.flags=flags;

 WriteFileBuffered(nodesx->fd,&nodex,sizeof(NodeX));

 nodesx->number++;

 logassert(nodesx->number<NODE_FAKE,"Too many nodes (change index_t to 64-bits?)"); /* NODE_FAKE marks the high-water mark for real nodes. */
}


/*++++++++++++++++++++++++++++++++++++++
  Finish appending nodes and change the filename over.

  NodesX *nodesx The nodes that have been appended.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishNodeList(NodesX *nodesx)
{
 if(nodesx->fd!=-1)
    nodesx->fd=CloseFileBuffered(nodesx->fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Find a particular node index.

  index_t IndexNodeX Returns the index of the extended node with the specified id.

  NodesX *nodesx The set of nodes to use.

  node_t id The node id to look for.
  ++++++++++++++++++++++++++++++++++++++*/

index_t IndexNodeX(NodesX *nodesx,node_t id)
{
 index_t start=0;
 index_t end=nodesx->number-1;
 index_t mid;

 if(nodesx->number==0)          /* No nodes */
    return(NO_NODE);

 /* Binary search - search key exact match only is required.
  *
  *  # <- start  |  Check mid and exit if it matches else move start or end.
  *  #           |
  *  #           |  Since an exact match is wanted we can set end=mid-1
  *  # <- mid    |  or start=mid+1 if we find that mid doesn't match.
  *  #           |
  *  #           |  Eventually either end=start or end=start+1 and one of
  *  # <- end    |  start or end is the wanted one or neither is.
  */

 while((end-start)>1)
   {
    mid=start+(end-start)/2;       /* Choose mid point (avoid overflow) */

    if(nodesx->idata[mid]<id)      /* Mid point is too low */
       start=mid+1;
    else if(nodesx->idata[mid]>id) /* Mid point is too high */
       end=mid-1;
    else                           /* Mid point is correct */
       return(mid);
   }

 if(nodesx->idata[start]==id)      /* Start is correct */
    return(start);

 if(nodesx->idata[end]==id)        /* End is correct */
    return(end);

 return(NO_NODE);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the node list.

  NodesX *nodesx The set of nodes to modify.
  ++++++++++++++++++++++++++++++++++++++*/

void SortNodeList(NodesX *nodesx)
{
 int fd;
 index_t xnumber;

 /* Print the start message */

 printf_first("Sorting Nodes");

 /* Re-open the file read-only and a new file writeable */

 fd=ReplaceFileBuffered(nodesx->filename_tmp,&nodesx->fd);

 /* Open a file for the index */

 nodesx->ifd=OpenFileBufferedNew(nodesx->ifilename_tmp);

 /* Sort the nodes by ID and index them */

 xnumber=nodesx->number;

 sortnodesx=nodesx;

 nodesx->number=filesort_fixed(nodesx->fd,fd,sizeof(NodeX),NULL,
                                                           (int (*)(const void*,const void*))sort_by_id,
                                                           (int (*)(void*,index_t))deduplicate_and_index_by_id);

 nodesx->knumber=nodesx->number;

 /* Close the files */

 nodesx->fd=CloseFileBuffered(nodesx->fd);
 CloseFileBuffered(fd);

 nodesx->ifd=CloseFileBuffered(nodesx->ifd);

 /* Print the final message */

 printf_last("Sorted Nodes: Nodes=%"Pindex_t" Duplicates=%"Pindex_t,xnumber,xnumber-nodesx->number);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the nodes into id order.

  int sort_by_id Returns the comparison of the id fields.

  NodeX *a The first extended node.

  NodeX *b The second extended node.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_id(NodeX *a,NodeX *b)
{
 node_t a_id=a->id;
 node_t b_id=b->id;

 if(a_id<b_id)
    return(-1);
 else if(a_id>b_id)
    return(1);
 else
    return(-FILESORT_PRESERVE_ORDER(a,b)); /* latest version first */
}


/*++++++++++++++++++++++++++++++++++++++
  Create the index of identifiers and discard duplicate nodes.

  int deduplicate_and_index_by_id Return 1 if the value is to be kept, otherwise 0.

  NodeX *nodex The extended node.

  index_t index The number of sorted nodes that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int deduplicate_and_index_by_id(NodeX *nodex,index_t index)
{
 static node_t previd; /* internal variable (reset by first call in each sort; index==0) */

 if(index==0 || nodex->id!=previd)
   {
    previd=nodex->id;

    if(nodex->flags&NODE_DELETED)
       return(0);
    else
      {
       WriteFileBuffered(sortnodesx->ifd,&nodex->id,sizeof(node_t));

       return(1);
      }
   }
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove any nodes that are not part of a highway.

  NodesX *nodesx The set of nodes to modify.

  WaysX *waysx The set of ways to use.

  int keep If set to 1 then keep the old data file otherwise delete it.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveNonHighwayNodes(NodesX *nodesx,WaysX *waysx,int keep)
{
 BitMask *usednode;
 NodeX nodex;
 index_t i,total=0,highway=0,nothighway=0;
 node_t bitmasklength;
 int fd;

 /* Print the start message */

 printf_first("Checking Ways for unused Nodes: Ways=0 Highway Nodes=0");

 /* Re-open the file read-only */

 waysx->fd=ReOpenFileBuffered(waysx->filename_tmp);

 /* Map the index into memory */

 nodesx->idata=MapFile(nodesx->ifilename_tmp);

 /* Allocate the node usage bitmask */

#if SLIM
 bitmasklength=nodesx->number;                     /* The number of nodes in the database */
#else
 bitmasklength=nodesx->idata[nodesx->number-1]+1;  /* One more than the highest OSM node number in the database */
#endif

 usednode=AllocBitMask(bitmasklength);
 log_malloc(usednode,SizeBitMask(bitmasklength));

 /* Loop through the ways and mark the used nodes */

 for(i=0;i<waysx->number;i++)
   {
    WayX wayx;
    FILESORT_VARINT waysize;
    node_t node;

    ReadFileBuffered(waysx->fd,&waysize,FILESORT_VARSIZE);

    ReadFileBuffered(waysx->fd,&wayx,sizeof(WayX));

    while(!ReadFileBuffered(waysx->fd,&node,sizeof(node_t)) && node!=NO_NODE_ID)
      {
#if SLIM
       index_t index=IndexNodeX(nodesx,node); /* Index bitmap by node number in the database */
#else
       node_t index=node;                     /* Index bitmap by OSM node number */
#endif

       waysize-=sizeof(node_t);

#if SLIM
       if(index==NO_NODE)
          continue;
#endif

       if(!IsBitSet(usednode,index))
          highway++;

       SetBit(usednode,index);
      }

    waysize-=sizeof(node_t)+sizeof(WayX);

    SkipFileBuffered(waysx->fd,waysize);

    if(!((i+1)%1000))
       printf_middle("Checking Ways for unused Nodes: Ways=%"Pindex_t" Highway Nodes=%"Pindex_t,i+1,highway);
   }

 /* Close the file */

 waysx->fd=CloseFileBuffered(waysx->fd);

 /* Unmap the index from memory */

 nodesx->idata=UnmapFile(nodesx->idata);

 /* Print the final message */

 printf_last("Checked Ways for unused Nodes: Ways=%"Pindex_t" Highway Nodes=%"Pindex_t,waysx->number,highway);


 /* Print the start message */

 printf_first("Removing unused Nodes: Nodes=0");

 /* Open a file for the index */

 nodesx->ifd=OpenFileBufferedNew(nodesx->ifilename_tmp);

 highway=0;

 /* Re-open the file read-only and a new file writeable */

 if(keep)
   {
    RenameFile(nodesx->filename_tmp,nodesx->filename);

    nodesx->fd=ReOpenFileBuffered(nodesx->filename);

    fd=OpenFileBufferedNew(nodesx->filename_tmp);
   }
 else
    fd=ReplaceFileBuffered(nodesx->filename_tmp,&nodesx->fd);

 /* Modify the on-disk image */

 while(!ReadFileBuffered(nodesx->fd,&nodex,sizeof(NodeX)))
   {
#if SLIM
    index_t node=total;         /* Index by node number in the database */
#else
    node_t node=nodex.id;       /* Index by OSM node number */
#endif

    if(!IsBitSet(usednode,node))
       nothighway++;
    else
      {
       WriteFileBuffered(fd,&nodex,sizeof(NodeX));

       WriteFileBuffered(nodesx->ifd,&nodex.id,sizeof(node_t));

       highway++;
      }

    total++;

    if(!(total%10000))
       printf_middle("Removing unused Nodes: Nodes=%"Pindex_t" Highway=%"Pindex_t" not-Highway=%"Pindex_t,total,highway,nothighway);
   }

 nodesx->number=highway;

 /* Close the files */

 nodesx->fd=CloseFileBuffered(nodesx->fd);
 CloseFileBuffered(fd);

 nodesx->ifd=CloseFileBuffered(nodesx->ifd);

 /* Free the now-unneeded index */

 log_free(usednode);
 free(usednode);

 /* Print the final message */

 printf_last("Removed unused Nodes: Nodes=%"Pindex_t" Highway=%"Pindex_t" not-Highway=%"Pindex_t,total,highway,nothighway);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove any nodes that have been pruned.

  NodesX *nodesx The set of nodes to prune.

  SegmentsX *segmentsx The set of segments to use.
  ++++++++++++++++++++++++++++++++++++++*/

void RemovePrunedNodes(NodesX *nodesx,SegmentsX *segmentsx)
{
 NodeX nodex;
 index_t total=0,pruned=0,notpruned=0;
 int fd;

 if(nodesx->number==0)
    return;

 /* Print the start message */

 printf_first("Deleting Pruned Nodes: Nodes=0 Pruned=0");

 /* Allocate the array of indexes */

 nodesx->pdata=(index_t*)malloc_logassert(nodesx->number*sizeof(index_t));
 log_malloc(nodesx->pdata,nodesx->number*sizeof(index_t));

 /* Re-open the file read-only and a new file writeable */

 fd=ReplaceFileBuffered(nodesx->filename_tmp,&nodesx->fd);

 /* Modify the on-disk image */

 while(!ReadFileBuffered(nodesx->fd,&nodex,sizeof(NodeX)))
   {
    if(segmentsx->firstnode[total]==NO_SEGMENT)
      {
       pruned++;

       nodesx->pdata[total]=NO_NODE;
      }
    else
      {
       nodesx->pdata[total]=notpruned;

       WriteFileBuffered(fd,&nodex,sizeof(NodeX));

       notpruned++;
      }

    total++;

    if(!(total%10000))
       printf_middle("Deleting Pruned Nodes: Nodes=%"Pindex_t" Pruned=%"Pindex_t,total,pruned);
   }

 nodesx->number=notpruned;

 /* Close the files */

 nodesx->fd=CloseFileBuffered(nodesx->fd);
 CloseFileBuffered(fd);

 /* Free the no-longer required memory */

 if(segmentsx->firstnode)
   {
    log_free(segmentsx->firstnode);
    free(segmentsx->firstnode);
    segmentsx->firstnode=NULL;
   }

 /* Print the final message */

 printf_last("Deleted Pruned Nodes: Nodes=%"Pindex_t" Pruned=%"Pindex_t,total,pruned);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the node list geographically.

  NodesX *nodesx The set of nodes to modify.
  ++++++++++++++++++++++++++++++++++++++*/

void SortNodeListGeographically(NodesX *nodesx)
{
 int fd;
 ll_bin_t lat_min_bin,lat_max_bin,lon_min_bin,lon_max_bin;

 if(nodesx->number==0)
    return;

 /* Print the start message */

 printf_first("Sorting Nodes Geographically");

 /* Work out the range of data */

 lat_min=radians_to_latlong( 2);
 lat_max=radians_to_latlong(-2);
 lon_min=radians_to_latlong( 4);
 lon_max=radians_to_latlong(-4);

 /* Allocate the memory for the geographical index array */

 nodesx->gdata=(index_t*)malloc_logassert(nodesx->number*sizeof(index_t));
 log_malloc(nodesx->gdata,nodesx->number*sizeof(index_t));

 /* Re-open the file read-only and a new file writeable */

 fd=ReplaceFileBuffered(nodesx->filename_tmp,&nodesx->fd);

 /* Sort nodes geographically and index them */

 sortnodesx=nodesx;

 filesort_fixed(nodesx->fd,fd,sizeof(NodeX),(int (*)(void*,index_t))update_id,
                                            (int (*)(const void*,const void*))sort_by_lat_long,
                                            (int (*)(void*,index_t))index_by_lat_long);

 /* Close the files */

 nodesx->fd=CloseFileBuffered(nodesx->fd);
 CloseFileBuffered(fd);

 /* Work out the number of bins */

 if(nodesx->super)
   {
    lat_min_bin=latlong_to_bin(lat_min);
    lon_min_bin=latlong_to_bin(lon_min);
    lat_max_bin=latlong_to_bin(lat_max);
    lon_max_bin=latlong_to_bin(lon_max);

    nodesx->latzero=lat_min_bin;
    nodesx->lonzero=lon_min_bin;

    nodesx->latbins=(lat_max_bin-lat_min_bin)+1;
    nodesx->lonbins=(lon_max_bin-lon_min_bin)+1;
   }

 /* Free the memory */

 if(nodesx->super)
   {
    log_free(nodesx->super);
    free(nodesx->super);
    nodesx->super=NULL;
   }

 /* Print the final message */

 printf_last("Sorted Nodes Geographically: Nodes=%"Pindex_t,nodesx->number);
}


/*++++++++++++++++++++++++++++++++++++++
  Update the node ids.

  int update_id Return 1 if the value is to be kept, otherwise 0.

  NodeX *nodex The extended node.

  index_t index The number of unsorted nodes that have been read from the input file.
  ++++++++++++++++++++++++++++++++++++++*/

static int update_id(NodeX *nodex,index_t index)
{
 nodex->id=index;

 if(sortnodesx->super && IsBitSet(sortnodesx->super,index))
    nodex->flags|=NODE_SUPER;

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the nodes into latitude and longitude order (first by longitude bin
  number, then by latitude bin number and then by exact longitude and then by
  exact latitude).

  int sort_by_lat_long Returns the comparison of the latitude and longitude fields.

  NodeX *a The first extended node.

  NodeX *b The second extended node.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_lat_long(NodeX *a,NodeX *b)
{
 ll_bin_t a_lon=latlong_to_bin(a->longitude);
 ll_bin_t b_lon=latlong_to_bin(b->longitude);

 if(a_lon<b_lon)
    return(-1);
 else if(a_lon>b_lon)
    return(1);
 else
   {
    ll_bin_t a_lat=latlong_to_bin(a->latitude);
    ll_bin_t b_lat=latlong_to_bin(b->latitude);

    if(a_lat<b_lat)
       return(-1);
    else if(a_lat>b_lat)
       return(1);
    else
      {
       if(a->longitude<b->longitude)
          return(-1);
       else if(a->longitude>b->longitude)
          return(1);
       else
         {
          if(a->latitude<b->latitude)
             return(-1);
          else if(a->latitude>b->latitude)
             return(1);
         }

       return(FILESORT_PRESERVE_ORDER(a,b));
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Create the index between the sorted and unsorted nodes.

  int index_by_lat_long Return 1 if the value is to be kept, otherwise 0.

  NodeX *nodex The extended node.

  index_t index The number of sorted nodes that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int index_by_lat_long(NodeX *nodex,index_t index)
{
 sortnodesx->gdata[nodex->id]=index;

 if(sortnodesx->super)
   {
    if(nodex->latitude<lat_min)
       lat_min=nodex->latitude;
    if(nodex->latitude>lat_max)
       lat_max=nodex->latitude;
    if(nodex->longitude<lon_min)
       lon_min=nodex->longitude;
    if(nodex->longitude>lon_max)
       lon_max=nodex->longitude;
   }

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Save the final node list database to a file.

  NodesX *nodesx The set of nodes to save.

  const char *filename The name of the file to save.

  SegmentsX *segmentsx The set of segments to use.
  ++++++++++++++++++++++++++++++++++++++*/

void SaveNodeList(NodesX *nodesx,const char *filename,SegmentsX *segmentsx)
{
 index_t i;
 int fd;
 NodesFile nodesfile={0};
 index_t super_number=0;
 ll_bin2_t latlonbin=0,maxlatlonbins;
 index_t *offsets;

 /* Print the start message */

 printf_first("Writing Nodes: Nodes=0");

 /* Allocate the memory for the geographical offsets array */

 offsets=(index_t*)malloc_logassert((nodesx->latbins*nodesx->lonbins+1)*sizeof(index_t));

 latlonbin=0;

 /* Re-open the file */

 nodesx->fd=ReOpenFileBuffered(nodesx->filename_tmp);

 /* Write out the nodes data */

 fd=OpenFileBufferedNew(filename);

 SeekFileBuffered(fd,sizeof(NodesFile)+(nodesx->latbins*nodesx->lonbins+1)*sizeof(index_t));

 for(i=0;i<nodesx->number;i++)
   {
    NodeX nodex;
    Node node={0};
    ll_bin_t latbin,lonbin;
    ll_bin2_t llbin;

    ReadFileBuffered(nodesx->fd,&nodex,sizeof(NodeX));

    /* Create the Node */

    node.latoffset=latlong_to_off(nodex.latitude);
    node.lonoffset=latlong_to_off(nodex.longitude);
    node.firstseg=segmentsx->firstnode[i];
    node.allow=nodex.allow;
    node.flags=nodex.flags;

    if(node.flags&NODE_SUPER)
       super_number++;

    /* Work out the offsets */

    latbin=latlong_to_bin(nodex.latitude )-nodesx->latzero;
    lonbin=latlong_to_bin(nodex.longitude)-nodesx->lonzero;
    llbin=lonbin*nodesx->latbins+latbin;

    for(;latlonbin<=llbin;latlonbin++)
       offsets[latlonbin]=i;

    /* Write the data */

    WriteFileBuffered(fd,&node,sizeof(Node));

    if(!((i+1)%10000))
       printf_middle("Writing Nodes: Nodes=%"Pindex_t,i+1);
   }

 /* Close the file */

 nodesx->fd=CloseFileBuffered(nodesx->fd);

 /* Finish off the offset indexing and write them out */

 maxlatlonbins=nodesx->latbins*nodesx->lonbins;

 for(;latlonbin<=maxlatlonbins;latlonbin++)
    offsets[latlonbin]=nodesx->number;

 SeekFileBuffered(fd,sizeof(NodesFile));
 WriteFileBuffered(fd,offsets,(nodesx->latbins*nodesx->lonbins+1)*sizeof(index_t));

 free(offsets);

 /* Write out the header structure */

 nodesfile.number=nodesx->number;
 nodesfile.snumber=super_number;

 nodesfile.latbins=nodesx->latbins;
 nodesfile.lonbins=nodesx->lonbins;

 nodesfile.latzero=nodesx->latzero;
 nodesfile.lonzero=nodesx->lonzero;

 SeekFileBuffered(fd,0);
 WriteFileBuffered(fd,&nodesfile,sizeof(NodesFile));

 CloseFileBuffered(fd);

 /* Free the memory in the segments */

 log_free(segmentsx->firstnode);
 free(segmentsx->firstnode);
 segmentsx->firstnode=NULL;

 /* Print the final message */

 printf_last("Wrote Nodes: Nodes=%"Pindex_t,nodesx->number);
}
