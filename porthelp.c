/*
 * porthelp.c
 *
 *  Created on: Jul 21, 2011
 *      Author: erich
 * Description: Implements some tools that might come in handy when writing a port
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "porthelp.h"
#include "api.h"

typedef struct
{
  handleType type;
  void *pointer;
  int Handle;

} HandleListEntry, *PHandleListEntry;

static HandleListEntry *HandleList = NULL;
static int HandleList_max = 0;
static pthread_mutex_t handle_mutex = PTHREAD_MUTEX_INITIALIZER;

int CreateHandleFromPointer(void *p, handleType type)
{
  pthread_mutex_lock(&handle_mutex);
  if (HandleList==NULL)
  {
    HandleList_max=256;
    HandleList=(PHandleListEntry)malloc(256*sizeof(HandleListEntry));
    if (!HandleList)
    {
      HandleList_max = 0;
      pthread_mutex_unlock(&handle_mutex);
      return 0;
    }
    memset((void *)HandleList, 0, sizeof(HandleListEntry)*256);
  }

  int i;
  for (i=1; i<HandleList_max; i++)
  {
    if (HandleList[i].type==htEmpty)
    {
      HandleList[i].pointer=p;
      HandleList[i].type=type;
      HandleList[i].Handle=i;
      pthread_mutex_unlock(&handle_mutex);
      return i;
    }
  }

  debug_log("Reached max amount of handles (%d). Expanding handle list.\n", HandleList_max);
  if (HandleList_max >= 1048576 || HandleList_max <= 0)
  {
    pthread_mutex_unlock(&handle_mutex);
    return 0;
  }
  int new_max = HandleList_max * 2;
  HandleListEntry *NewHandleList = (HandleListEntry *)realloc(HandleList, (size_t)new_max * sizeof(HandleListEntry));
  if (!NewHandleList)
  {
    pthread_mutex_unlock(&handle_mutex);
    return 0;
  }
  memset(&NewHandleList[HandleList_max], 0, (size_t)(new_max - HandleList_max) * sizeof(HandleListEntry));
  HandleList=NewHandleList;
  i=HandleList_max;
  HandleList_max=new_max;

  HandleList[i].pointer=p;
  HandleList[i].type=type;
  HandleList[i].Handle=i;
  pthread_mutex_unlock(&handle_mutex);
  return i;
}


void *GetPointerFromHandle(int handle)
{
  void *ptr = NULL;
  pthread_mutex_lock(&handle_mutex);
  if ((handle>0) && (handle<HandleList_max) && (HandleList[handle].type != htEmpty))
    ptr = HandleList[handle].pointer;
  pthread_mutex_unlock(&handle_mutex);
  return ptr;
}

handleType GetHandleType(int handle)
{
  handleType type = htEmpty;
  pthread_mutex_lock(&handle_mutex);
  if ((handle>0) && (handle<HandleList_max))
    type = HandleList[handle].type;
  pthread_mutex_unlock(&handle_mutex);
  return type;
}

void RemoveHandle(int handle)
{
  pthread_mutex_lock(&handle_mutex);
  if ((handle>0) && (handle<HandleList_max) && (HandleList[handle].type != htEmpty))
  {
    HandleList[handle].type=htEmpty;
    HandleList[handle].pointer=NULL;
  }
  pthread_mutex_unlock(&handle_mutex);
}

int SearchHandleList(int type, HANDLESEARCHCALLBACK cb, void *searchdata)
/*
 * go through the handle list and call cb(data, searchdata) for each handle of the specified type
 * if cb(data,searchdata) returns true then return that handle, else return 0
 */
{
  if (!cb)
    return 0;

  int i;

  pthread_mutex_lock(&handle_mutex);
  for (i=1; i<HandleList_max; i++)
  {
    if (HandleList[i].type==type)
    {
      if (cb(HandleList[i].pointer, searchdata))
      {
        pthread_mutex_unlock(&handle_mutex);
        return i;
      }
    }
  }
  pthread_mutex_unlock(&handle_mutex);

  return 0;
}
