/***************************************
 Merge sort functions.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2009-2015, 2017, 2019, 2023 Andrew M. Bishop

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_PTHREADS) && USE_PTHREADS
#include <pthread.h>
#endif

#include "types.h"

#include "logging.h"
#include "files.h"
#include "sorting.h"


/* Global variables */

/*+ The command line '--tmpdir' option or its default value. +*/
extern char *option_tmpdirname;

/*+ The amount of RAM to use for filesorting. +*/
extern size_t option_filesort_ramsize;

/*+ The number of filesorting threads allowed. +*/
extern int option_filesort_threads;


/* Thread data type definitions */

/*+ A data type for holding data for a thread. +*/
typedef struct _thread_data
 {
#if defined(USE_PTHREADS) && USE_PTHREADS

  pthread_t thread;             /*+ The thread identifier. +*/

  int       running;            /*+ A flag indicating the current state of the thread. +*/

#endif

  char     *data;               /*+ The main data array. +*/
  void    **datap;              /*+ An array of pointers to the data objects. +*/
  size_t    n;                  /*+ The number of pointers. +*/

  int       fd;                 /*+ The file descriptor of the file to write the results to. +*/

  size_t    itemsize;           /*+ The size of each item. +*/
  int     (*compare)(const void*,const void*); /*+ The comparison function. +*/
 }
 thread_data;

/* Thread variables */

#if defined(USE_PTHREADS) && USE_PTHREADS

static pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  running_cond  = PTHREAD_COND_INITIALIZER;

#endif

/* Thread helper functions */

static void *filesort_fixed_heapsort_thread(thread_data *thread);
static void *filesort_vary_heapsort_thread(thread_data *thread);


/*++++++++++++++++++++++++++++++++++++++
  A function to sort the contents of a file of fixed length objects using a
  limited amount of RAM.

  The data is sorted using a "Merge sort" http://en.wikipedia.org/wiki/Merge_sort
  and in particular an "external sort" http://en.wikipedia.org/wiki/External_sorting.
  The individual sort steps and the merge step both use a "Heap sort"
  http://en.wikipedia.org/wiki/Heapsort.  The combination of the two should work well
  if the data is already partially sorted.

  index_t filesort_fixed Returns the number of objects kept.

  int fd_in The file descriptor of the input file (opened for reading and at the beginning).

  int fd_out The file descriptor of the output file (opened for writing and empty).

  size_t itemsize The size of each item in the file that needs sorting.

  int (*pre_sort_function)(void *,index_t) If non-NULL then this function is called for
     each item before they have been sorted.  The second parameter is the number of objects
     previously read from the input file.  If the function returns 1 then the object is kept
     and it is sorted, otherwise it is ignored.

  int (*compare_function)(const void*, const void*) The comparison function.  This is identical
     to qsort if the data to be sorted is an array of things not pointers.

  int (*post_sort_function)(void *,index_t) If non-NULL then this function is called for
     each item after they have been sorted.  The second parameter is the number of objects
     already written to the output file.  If the function returns 1 then the object is written
     to the output file., otherwise it is ignored.
  ++++++++++++++++++++++++++++++++++++++*/

index_t filesort_fixed(int fd_in,int fd_out,size_t itemsize,int (*pre_sort_function)(void*,index_t),
                                                            int (*compare_function)(const void*,const void*),
                                                            int (*post_sort_function)(void*,index_t))
{
 int *fds=NULL,*heap=NULL;
 int nfiles=0,ndata=0;
 index_t count_out=0,count_in=0,total=0;
 size_t nitems,item;
 char *data;
 void **datap;
 thread_data *threads;
 int i,more=1;
 char *filename=(char*)malloc_logassert(strlen(option_tmpdirname)+24);
#if defined(USE_PTHREADS) && USE_PTHREADS
 int nthreads=0;
#endif

 /* Allocate the RAM buffer and other bits */

 nitems=(size_t)SizeFileFD(fd_in)/itemsize;

 if(nitems==0)
    return(0);

 if((nitems*(itemsize+sizeof(void*)))<(option_filesort_ramsize/option_filesort_threads))
    /* use one thread */;
 else if((nitems*(itemsize+sizeof(void*)))<option_filesort_ramsize)
    nitems=1+nitems/option_filesort_threads;
 else
    nitems=option_filesort_ramsize/(option_filesort_threads*(itemsize+sizeof(void*)));

 threads=(thread_data*)calloc_logassert(option_filesort_threads,sizeof(thread_data));

 for(i=0;i<option_filesort_threads;i++)
   {
    threads[i].fd=-1;

    threads[i].data=malloc_logassert(nitems*itemsize);
    threads[i].datap=malloc_logassert(nitems*sizeof(void*));

    log_malloc(threads[i].data ,nitems*itemsize);
    log_malloc(threads[i].datap,nitems*sizeof(void*));

    threads[i].itemsize=itemsize;
    threads[i].compare=compare_function;
   }

 /* Loop around, fill the buffer, sort the data and write a temporary file */

 do
   {
    int thread=0;

#if defined(USE_PTHREADS) && USE_PTHREADS

    if(option_filesort_threads>1)
      {
       /* If all threads are in use wait for an existing thread to finish */

       if(nthreads==option_filesort_threads)
         {
          pthread_mutex_lock(&running_mutex);

          while(nthreads==option_filesort_threads)
            {
             for(i=0;i<option_filesort_threads;i++)
                if(threads[i].running==2)
                  {
                   pthread_join(threads[i].thread,NULL);
                   threads[i].running=0;
                   CloseFileBuffered(threads[i].fd);
                   nthreads--;
                  }

             if(nthreads==option_filesort_threads)
                pthread_cond_wait(&running_cond,&running_mutex);
            }

          pthread_mutex_unlock(&running_mutex);
         }

       /* Find a spare slot */

       pthread_mutex_lock(&running_mutex);

       for(thread=0;thread<option_filesort_threads;thread++)
          if(!threads[thread].running)
             break;

       pthread_mutex_unlock(&running_mutex);
      }

#endif

    /* Read in the data and create pointers */

    for(item=0;item<nitems;)
      {
       threads[thread].datap[item]=threads[thread].data+item*itemsize;

       if(ReadFileBuffered(fd_in,threads[thread].datap[item],itemsize))
         {
          more=0;
          break;
         }

       if(!pre_sort_function || pre_sort_function(threads[thread].datap[item],count_in))
         {
          item++;
          total++;
         }

       count_in++;
      }

    threads[thread].n=item;

    /* Shortcut if there is no previous data and no more data (i.e. no data at all) */

    if(more==0 && total==0)
       goto tidy_and_exit;

    /* No new data read in this time round */

    if(threads[thread].n==0)
       break;

    /* Shortcut if only one file, don't write to disk */

    if(more==0 && nfiles==0)
       filesort_heapsort(threads[thread].datap,threads[thread].n,threads[thread].compare);

    else
      {
       /* Create the file descriptor (not thread-safe) */

       sprintf(filename,"%s/filesort.%d.tmp",option_tmpdirname,nfiles);

       threads[thread].fd=OpenFileBufferedNew(filename);

       if(option_filesort_threads==1)
         {
          filesort_fixed_heapsort_thread(&threads[thread]);

          CloseFileBuffered(threads[thread].fd);
         }

#if defined(USE_PTHREADS) && USE_PTHREADS

       else
         {
          pthread_mutex_lock(&running_mutex);

          threads[thread].running=1;

          pthread_mutex_unlock(&running_mutex);

          pthread_create(&threads[thread].thread,NULL,(void* (*)(void*))filesort_fixed_heapsort_thread,&threads[thread]);

          nthreads++;
         }

#endif

      }

    nfiles++;
   }
 while(more);

 /* Wait for all of the threads to finish */

#if defined(USE_PTHREADS) && USE_PTHREADS

 while(option_filesort_threads>1 && nthreads)
   {
    pthread_mutex_lock(&running_mutex);

    pthread_cond_wait(&running_cond,&running_mutex);

    for(i=0;i<option_filesort_threads;i++)
       if(threads[i].running==2)
         {
          pthread_join(threads[i].thread,NULL);
          threads[i].running=0;
          CloseFileBuffered(threads[i].fd);
          nthreads--;
         }

    pthread_mutex_unlock(&running_mutex);
   }

#endif

 /* Shortcut if there are no files */

 if(nfiles==0)
    goto tidy_and_exit;

 /* Shortcut if only one file, lucky for us we still have the data in RAM) */

 if(nfiles==1)
   {
    for(item=0;item<threads[0].n;item++)
      {
       if(!post_sort_function || post_sort_function(threads[0].datap[item],count_out))
         {
          WriteFileBuffered(fd_out,threads[0].datap[item],itemsize);
          count_out++;
         }
      }

    DeleteFile(filename);

    goto tidy_and_exit;
   }

 /* Check that number of files is less than file size */

 logassert((unsigned)nfiles<nitems,"Too many temporary files (use more sorting memory?)");

 /* Open all of the temporary files */

 fds=(int*)malloc_logassert(nfiles*sizeof(int));

 for(i=0;i<nfiles;i++)
   {
    sprintf(filename,"%s/filesort.%d.tmp",option_tmpdirname,i);

    fds[i]=ReOpenFileBuffered(filename);

    DeleteFile(filename);
   }

 /* Perform an n-way merge using a binary heap */

 heap=(int*)malloc_logassert((1+nfiles)*sizeof(int));

 data =threads[0].data;
 datap=threads[0].datap;

 /* Fill the heap to start with */

 for(i=0;i<nfiles;i++)
   {
    int index;

    datap[i]=data+i*itemsize;

    ReadFileBuffered(fds[i],datap[i],itemsize);

    index=i+1;

    heap[index]=i;

    /* Bubble up the new value */

    while(index>1)
      {
       int newindex;
       int temp;

       newindex=index/2;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])>=0)
          break;

       temp=heap[index];
       heap[index]=heap[newindex];
       heap[newindex]=temp;

       index=newindex;
      }
   }

 /* Repeatedly pull out the root of the heap and refill from the same file */

 ndata=nfiles;

 do
   {
    int index=1;

    if(!post_sort_function || post_sort_function(datap[heap[index]],count_out))
      {
       WriteFileBuffered(fd_out,datap[heap[index]],itemsize);
       count_out++;
      }

    if(ReadFileBuffered(fds[heap[index]],datap[heap[index]],itemsize))
      {
       heap[index]=heap[ndata];
       ndata--;
      }

    /* Bubble down the new value */

    while((2*index)<ndata)
      {
       int newindex;
       int temp;

       newindex=2*index;

       if(compare_function(datap[heap[newindex]],datap[heap[newindex+1]])>=0)
          newindex=newindex+1;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])<=0)
          break;

       temp=heap[newindex];
       heap[newindex]=heap[index];
       heap[index]=temp;

       index=newindex;
      }

    if((2*index)==ndata)
      {
       int newindex;
       int temp;

       newindex=2*index;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])<=0)
          ; /* break */
       else
         {
          temp=heap[newindex];
          heap[newindex]=heap[index];
          heap[index]=temp;
         }
      }
   }
 while(ndata>0);

 /* Tidy up */

 tidy_and_exit:

 if(fds)
   {
    for(i=0;i<nfiles;i++)
       CloseFileBuffered(fds[i]);
    free(fds);
   }

 if(heap)
    free(heap);

 for(i=0;i<option_filesort_threads;i++)
   {
    log_free(threads[i].data);
    log_free(threads[i].datap);

    free(threads[i].data);
    free(threads[i].datap);
   }

 free(threads);

 free(filename);

 return(count_out);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to sort the contents of a file of variable length objects (each
  preceded by its length in FILESORT_VARSIZE bytes) using a limited amount of RAM.

  The data is sorted using a "Merge sort" http://en.wikipedia.org/wiki/Merge_sort
  and in particular an "external sort" http://en.wikipedia.org/wiki/External_sorting.
  The individual sort steps and the merge step both use a "Heap sort"
  http://en.wikipedia.org/wiki/Heapsort.  The combination of the two should work well
  if the data is already partially sorted.

  index_t filesort_vary Returns the number of objects kept.

  int fd_in The file descriptor of the input file (opened for reading and at the beginning).

  int fd_out The file descriptor of the output file (opened for writing and empty).

  int (*pre_sort_function)(void *,index_t) If non-NULL then this function is called for
     each item before they have been sorted.  The second parameter is the number of objects
     previously read from the input file.  If the function returns 1 then the object is kept
     and it is sorted, otherwise it is ignored.

  int (*compare_function)(const void*, const void*) The comparison function.  This is identical
     to qsort if the data to be sorted is an array of things not pointers.

  int (*post_sort_function)(void *,index_t) If non-NULL then this function is called for
     each item after they have been sorted.  The second parameter is the number of objects
     already written to the output file.  If the function returns 1 then the object is written
     to the output file., otherwise it is ignored.
  ++++++++++++++++++++++++++++++++++++++*/

index_t filesort_vary(int fd_in,int fd_out,int (*pre_sort_function)(void*,index_t),
                                           int (*compare_function)(const void*,const void*),
                                           int (*post_sort_function)(void*,index_t))
{
 int *fds=NULL,*heap=NULL;
 int nfiles=0,ndata=0;
 index_t count_out=0,count_in=0,total=0;
 size_t datasize,item;
 FILESORT_VARINT nextitemsize,largestitemsize=0;
 char *data;
 void **datap;
 thread_data *threads;
 int i,more=1;
 char *filename=(char*)malloc_logassert(strlen(option_tmpdirname)+24);
#if defined(USE_PTHREADS) && USE_PTHREADS
 int nthreads=0;
#endif

 /* Allocate the RAM buffer and other bits */

 datasize=(size_t)SizeFileFD(fd_in);

 if(datasize==0)
    return(0);

 /* We can not know in advance how many data items there are.  Each
    one will require RAM for data, FILESORT_VARALIGN and sizeof(void*)
    Assume that data+FILESORT_VARALIGN+sizeof(void*) is 4*data. */

 if((datasize*4)<option_filesort_ramsize/option_filesort_threads)
    /* use one thread */;
 else if((datasize*4)<option_filesort_ramsize)
    datasize=(datasize*4)/option_filesort_threads;
 else
    datasize=option_filesort_ramsize/option_filesort_threads;

 datasize=FILESORT_VARALIGN*((datasize+FILESORT_VARALIGN-1)/FILESORT_VARALIGN);

 threads=(thread_data*)calloc_logassert(option_filesort_threads,sizeof(thread_data));

 for(i=0;i<option_filesort_threads;i++)
   {
    threads[i].fd=-1;

    threads[i].data=malloc_logassert(datasize);
    threads[i].datap=NULL;

    log_malloc(threads[i].data,datasize);

    threads[i].compare=compare_function;
   }

 /* Loop around, fill the buffer, sort the data and write a temporary file */

 if(ReadFileBuffered(fd_in,&nextitemsize,FILESORT_VARSIZE))    /* Always have the next item size known in advance */
    goto tidy_and_exit;

 do
   {
    size_t ramused=FILESORT_VARALIGN-FILESORT_VARSIZE;
    int thread=0;

#if defined(USE_PTHREADS) && USE_PTHREADS

    if(option_filesort_threads>1)
      {
       /* If all threads are in use wait for an existing thread to finish */

       if(nthreads==option_filesort_threads)
         {
          pthread_mutex_lock(&running_mutex);

          while(nthreads==option_filesort_threads)
            {
             for(i=0;i<option_filesort_threads;i++)
                if(threads[i].running==2)
                  {
                   pthread_join(threads[i].thread,NULL);
                   threads[i].running=0;
                   CloseFileBuffered(threads[i].fd);
                   nthreads--;
                  }

             if(nthreads==option_filesort_threads)
                pthread_cond_wait(&running_cond,&running_mutex);
            }

          pthread_mutex_unlock(&running_mutex);
         }

       /* Find a spare slot */

       pthread_mutex_lock(&running_mutex);

       for(thread=0;thread<option_filesort_threads;thread++)
          if(!threads[thread].running)
             break;

       pthread_mutex_unlock(&running_mutex);
      }

#endif

    threads[thread].datap=(void**)(threads[thread].data+datasize);

    threads[thread].n=0;

    /* Read in the data and create pointers */

    while((ramused+FILESORT_VARSIZE+nextitemsize)<=(size_t)((char*)threads[thread].datap-sizeof(void*)-threads[thread].data))
      {
       FILESORT_VARINT itemsize=nextitemsize;

       *(FILESORT_VARINT*)(threads[thread].data+ramused)=itemsize;

       ramused+=FILESORT_VARSIZE;

       ReadFileBuffered(fd_in,threads[thread].data+ramused,itemsize);

       if(!pre_sort_function || pre_sort_function(threads[thread].data+ramused,count_in))
         {
          *--threads[thread].datap=threads[thread].data+ramused; /* points to real data */

          if(itemsize>largestitemsize)
             largestitemsize=itemsize;

          ramused+=itemsize;

          ramused =FILESORT_VARALIGN*((ramused+FILESORT_VARALIGN-1)/FILESORT_VARALIGN);
          ramused+=FILESORT_VARALIGN-FILESORT_VARSIZE;

          total++;
          threads[thread].n++;
         }
       else
          ramused-=FILESORT_VARSIZE;

       count_in++;

       if(ReadFileBuffered(fd_in,&nextitemsize,FILESORT_VARSIZE))
         {
          more=0;
          break;
         }
      }

    /* No new data read in this time round */

    if(threads[thread].n==0)
       break;

    /* Shortcut if only one file, don't write to disk */

    if(more==0 && nfiles==0)
       filesort_heapsort(threads[thread].datap,threads[thread].n,threads[thread].compare);

    else
      {
       /* Create the file descriptor (not thread-safe) */

       sprintf(filename,"%s/filesort.%d.tmp",option_tmpdirname,nfiles);

       threads[thread].fd=OpenFileBufferedNew(filename);

       if(option_filesort_threads==1)
         {
          filesort_vary_heapsort_thread(&threads[thread]);

          CloseFileBuffered(threads[thread].fd);
         }

#if defined(USE_PTHREADS) && USE_PTHREADS

       else
         {
          pthread_mutex_lock(&running_mutex);

          threads[thread].running=1;

          pthread_mutex_unlock(&running_mutex);

          pthread_create(&threads[thread].thread,NULL,(void* (*)(void*))filesort_vary_heapsort_thread,&threads[thread]);

          nthreads++;
         }

#endif

      }

    nfiles++;
   }
 while(more);

 /* Wait for all of the threads to finish */

#if defined(USE_PTHREADS) && USE_PTHREADS

 while(option_filesort_threads>1 && nthreads)
   {
    pthread_mutex_lock(&running_mutex);

    pthread_cond_wait(&running_cond,&running_mutex);

    for(i=0;i<option_filesort_threads;i++)
       if(threads[i].running==2)
         {
          pthread_join(threads[i].thread,NULL);
          threads[i].running=0;
          CloseFileBuffered(threads[i].fd);
          nthreads--;
         }

    pthread_mutex_unlock(&running_mutex);
   }

#endif

 /* Shortcut if there are no files */

 if(nfiles==0)
    goto tidy_and_exit;

 /* Shortcut if only one file, lucky for us we still have the data in RAM) */

 if(nfiles==1)
   {
    for(item=0;item<threads[0].n;item++)
      {
       if(!post_sort_function || post_sort_function(threads[0].datap[item],count_out))
         {
          FILESORT_VARINT itemsize=*(FILESORT_VARINT*)((char*)threads[0].datap[item]-FILESORT_VARSIZE);

          WriteFileBuffered(fd_out,(char*)threads[0].datap[item]-FILESORT_VARSIZE,itemsize+FILESORT_VARSIZE);
          count_out++;
         }
      }

    DeleteFile(filename);

    goto tidy_and_exit;
   }

 /* Check that number of files is less than file size */

 largestitemsize=FILESORT_VARALIGN*((largestitemsize+FILESORT_VARALIGN-1)/FILESORT_VARALIGN);

 logassert((unsigned)nfiles<((datasize-nfiles*sizeof(void*))/(FILESORT_VARALIGN+largestitemsize)),"Too many temporary files (use more sorting memory?)");

 /* Open all of the temporary files */

 fds=(int*)malloc_logassert(nfiles*sizeof(int));

 for(i=0;i<nfiles;i++)
   {
    sprintf(filename,"%s/filesort.%d.tmp",option_tmpdirname,i);

    fds[i]=ReOpenFileBuffered(filename);

    DeleteFile(filename);
   }

 /* Perform an n-way merge using a binary heap */

 heap=(int*)malloc_logassert((1+nfiles)*sizeof(int));

 data=threads[0].data;
 datap=(void**)(data+datasize-nfiles*sizeof(void*));

 /* Fill the heap to start with */

 for(i=0;i<nfiles;i++)
   {
    int index;
    FILESORT_VARINT itemsize;

    datap[i]=data+FILESORT_VARALIGN+i*(largestitemsize+FILESORT_VARALIGN);

    ReadFileBuffered(fds[i],&itemsize,FILESORT_VARSIZE);

    *(FILESORT_VARINT*)((char*)datap[i]-FILESORT_VARSIZE)=itemsize;

    ReadFileBuffered(fds[i],datap[i],itemsize);

    index=i+1;

    heap[index]=i;

    /* Bubble up the new value */

    while(index>1)
      {
       int newindex;
       int temp;

       newindex=index/2;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])>=0)
          break;

       temp=heap[index];
       heap[index]=heap[newindex];
       heap[newindex]=temp;

       index=newindex;
      }
   }

 /* Repeatedly pull out the root of the heap and refill from the same file */

 ndata=nfiles;

 do
   {
    int index=1;
    FILESORT_VARINT itemsize;

    if(!post_sort_function || post_sort_function(datap[heap[index]],count_out))
      {
       itemsize=*(FILESORT_VARINT*)((char*)datap[heap[index]]-FILESORT_VARSIZE);

       WriteFileBuffered(fd_out,(char*)datap[heap[index]]-FILESORT_VARSIZE,itemsize+FILESORT_VARSIZE);
       count_out++;
      }

    if(ReadFileBuffered(fds[heap[index]],&itemsize,FILESORT_VARSIZE))
      {
       heap[index]=heap[ndata];
       ndata--;
      }
    else
      {
       *(FILESORT_VARINT*)((char*)datap[heap[index]]-FILESORT_VARSIZE)=itemsize;

       ReadFileBuffered(fds[heap[index]],datap[heap[index]],itemsize);
      }

    /* Bubble down the new value */

    while((2*index)<ndata)
      {
       int newindex;
       int temp;

       newindex=2*index;

       if(compare_function(datap[heap[newindex]],datap[heap[newindex+1]])>=0)
          newindex=newindex+1;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])<=0)
          break;

       temp=heap[newindex];
       heap[newindex]=heap[index];
       heap[index]=temp;

       index=newindex;
      }

    if((2*index)==ndata)
      {
       int newindex;
       int temp;

       newindex=2*index;

       if(compare_function(datap[heap[index]],datap[heap[newindex]])<=0)
          ; /* break */
       else
         {
          temp=heap[newindex];
          heap[newindex]=heap[index];
          heap[index]=temp;
         }
      }
   }
 while(ndata>0);

 /* Tidy up */

 tidy_and_exit:

 if(fds)
   {
    for(i=0;i<nfiles;i++)
       CloseFileBuffered(fds[i]);
    free(fds);
   }

 if(heap)
    free(heap);

 for(i=0;i<option_filesort_threads;i++)
   {
    log_free(threads[i].data);

    free(threads[i].data);
   }

 free(threads);

 free(filename);

 return(count_out);
}


/*++++++++++++++++++++++++++++++++++++++
  A wrapper function that can be run in a thread for fixed data.

  void *filesort_fixed_heapsort_thread Returns NULL (required to return void*).

  thread_data *thread The data to be processed in this thread.
  ++++++++++++++++++++++++++++++++++++++*/

static void *filesort_fixed_heapsort_thread(thread_data *thread)
{
 size_t item;

 /* Sort the data pointers using a heap sort */

 filesort_heapsort(thread->datap,thread->n,thread->compare);

 /* Write the result to the given temporary file */

 if(thread->fd > 0)
    for(item=0;item<thread->n;item++)
       WriteFileBuffered(thread->fd,thread->datap[item],thread->itemsize);

#if defined(USE_PTHREADS) && USE_PTHREADS

 if(option_filesort_threads>1)
   {
    pthread_mutex_lock(&running_mutex);

    thread->running=2;

    pthread_cond_signal(&running_cond);

    pthread_mutex_unlock(&running_mutex);
   }

#endif

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  A wrapper function that can be run in a thread for variable data.

  void *filesort_vary_heapsort_thread Returns NULL (required to return void*).

  thread_data *thread The data to be processed in this thread.
  ++++++++++++++++++++++++++++++++++++++*/

static void *filesort_vary_heapsort_thread(thread_data *thread)
{
 size_t item;

 /* Sort the data pointers using a heap sort */

 filesort_heapsort(thread->datap,thread->n,thread->compare);

 /* Write the result to the given temporary file */

 if(thread->fd > 0)
    for(item=0;item<thread->n;item++)
      {
       FILESORT_VARINT itemsize=*(FILESORT_VARINT*)((char*)thread->datap[item]-FILESORT_VARSIZE);

       WriteFileBuffered(thread->fd,(char*)thread->datap[item]-FILESORT_VARSIZE,itemsize+FILESORT_VARSIZE);
      }

#if defined(USE_PTHREADS) && USE_PTHREADS

 if(option_filesort_threads>1)
   {
    pthread_mutex_lock(&running_mutex);

    thread->running=2;

    pthread_cond_signal(&running_cond);

    pthread_mutex_unlock(&running_mutex);
   }

#endif

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to sort an array of pointers efficiently.

  The data is sorted using a "Heap sort" http://en.wikipedia.org/wiki/Heapsort,
  in particular, this is good because it can operate in-place and doesn't
  allocate more memory like using qsort() does.

  void **datap A pointer to the array of pointers to sort.

  size_t nitems The number of items of data to sort.

  int (*compare_function)(const void*, const void*) The comparison function.  This is identical
     to qsort if the data to be sorted is an array of things not pointers.
  ++++++++++++++++++++++++++++++++++++++*/

void filesort_heapsort(void **datap,size_t nitems,int(*compare_function)(const void*, const void*))
{
 void **datap1=&datap[-1];
 size_t item;

 /* Fill the heap by pretending to insert the data that is already there */

 for(item=2;item<=nitems;item++)
   {
    size_t index=item;

    /* Bubble up the new value (upside-down, put largest at top) */

    while(index>1)
      {
       int newindex;
       void *temp;

       newindex=index/2;

       if(compare_function(datap1[index],datap1[newindex])<=0) /* reversed comparison to filesort_fixed() above */
          break;

       temp=datap1[index];
       datap1[index]=datap1[newindex];
       datap1[newindex]=temp;

       index=newindex;
      }
   }

 /* Repeatedly pull out the root of the heap and swap with the bottom item */

 for(item=nitems;item>1;item--)
   {
    size_t index=1;
    void *temp;

    temp=datap1[index];
    datap1[index]=datap1[item];
    datap1[item]=temp;

    /* Bubble down the new value (upside-down, put largest at top) */

    while((2*index)<(item-1))
      {
       int newindex;
       void **temp;

       newindex=2*index;

       if(compare_function(datap1[newindex],datap1[newindex+1])<=0) /* reversed comparison to filesort_fixed() above */
          newindex=newindex+1;

       if(compare_function(datap1[index],datap1[newindex])>=0) /* reversed comparison to filesort_fixed() above */
          break;

       temp=datap1[newindex];
       datap1[newindex]=datap1[index];
       datap1[index]=temp;

       index=newindex;
      }

    if((2*index)==(item-1))
      {
       int newindex;
       void *temp;

       newindex=2*index;

       if(compare_function(datap1[index],datap1[newindex])>=0) /* reversed comparison to filesort_fixed() above */
          ; /* break */
       else
         {
          temp=datap1[newindex];
          datap1[newindex]=datap1[index];
          datap1[index]=temp;
         }
      }
   }
}
