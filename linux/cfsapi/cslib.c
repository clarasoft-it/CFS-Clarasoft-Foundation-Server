/* ==========================================================================
  Clarasoft Core Tools
  cslib.c
  Version 1.0.0

  Distributed under the MIT license

  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
  THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  CRTCMOD MODULE(CSSTR) SRCFILE(QCSRC) DBGVIEW(*ALL)
========================================================================== */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#define CS_SUCCESS                     (0x00000000)
#define CS_FAILURE                     (0x80000000)

#define CS_MASK_OPER                   (0x0FFF0000)
#define CS_MASK_DIAG                   (0x0000FFFF)

#define CS_SUCCEED(x)                  (((x) & (0xF0000000)) == (0))
#define CS_FAIL(x)                     ((x) & (CS_FAILURE))
#define CS_OPER(x)                     ((x) & (CS_MASK_OPER))
#define CS_DIAG(x)                     ((x) & (CS_MASK_DIAG))

#define CS_OPER_NOVALUE                (0x00000000)
#define CS_DIAG_NOVALUE                (0x00000000)
#define CS_DIAG_UNKNOWN                (0x0000FFFF)

typedef long CSRESULT;

#define CSLIST_TOP                     (0x00000000)   // at the beginning
#define CSLIST_BOTTOM                  (0x7FFFFFFF)   // at the end

#define B64_MASK_11                    (0xFC)
#define B64_MASK_12                    (0x03)
#define B64_MASK_21                    (0xF0)
#define B64_MASK_22                    (0x0F)
#define B64_MASK_31                    (0xC0)
#define B64_MASK_32                    (0x3F)

#define CSSTR_B64_MODE_STRICT          (0x00000000)
#define CSSTR_B64_MODE_ACCEPTLINEBREAK (0x00000100)

#define CSSTR_B64_LINEBREAK_NONE       (0x00000000)
#define CSSTR_B64_LINEBREAK_LF         (0x00000001)
#define CSSTR_B64_LINEBREAK_CRLF       (0x00000002)
#define CSSTR_B64_LINEBREAK_OFFSET     (0x0000004D)

#define CSSTR_B64_MASK_LINEBREAK       (0x0000000F)
#define CSSTR_B64_IGNOREINVALIDCHAR    (0x00000100)

#define CSSTR_URLENCODE_SPACETOPLUS    (0x00000100)
#define CSSTR_URLENCODE_CONVERTALL     (0x00000200)

#define CSSYS_UUID_BUFFERSIZE          (37)
#define CSSYS_UUID_UPPERCASE           (0x00000000)
#define CSSYS_UUID_LOWERCASE           (0x00000001)
#define CSSYS_UUID_DASHES              (0x00000002)

typedef struct tagCSLISTNODE {

  struct tagCSLISTNODE * previous;
  struct tagCSLISTNODE * next;

  void* data;
  long dataSize;

} CSLISTNODE;

typedef CSLISTNODE* PCSLISTNODE;

typedef struct tagCSLIST {

  PCSLISTNODE first;
  PCSLISTNODE last;
  PCSLISTNODE current;

  long numItems;
  long curIndex;

} CSLIST;

typedef struct tagCSMAPNODE {

  /* data item */

  char* key;
  char* value;
  int   keySize;
  int   valueSize;

  /* Links */

  struct tagCSMAPNODE* parent;
  struct tagCSMAPNODE* left;
  struct tagCSMAPNODE* right;

  /* To manage balancing */

  int   height;

} CSMAPNODE;

typedef CSMAPNODE* pAVLTREE;

typedef struct tagCSMAP {

   pAVLTREE tree;

   /* For iterator */

   CSLIST* keys;
   long NextKey;

}CSMAP;

/* ---------------------------------------------------------------------------
  private prototypes
--------------------------------------------------------------------------- */

CSRESULT
  CSLIST_PRIVATE_Goto
    (CSLIST* This,
     long    index);

pAVLTREE
  CSMAP_PRIVATE_Insert
     (pAVLTREE Tree,
       pAVLTREE parent,
       char*    key,
       void*    value,
       long     valueSize);

pAVLTREE
  CSMAP_PRIVATE_InsertKeyRef
     (pAVLTREE Tree,
       pAVLTREE parent,
       char*    key,
       void*    value,
       long     valueSize);

pAVLTREE
  CSMAP_PRIVATE_Remove
    (pAVLTREE Tree,
       char* key);

CSRESULT
  CSMAP_PRIVATE_Lookup
     (pAVLTREE Tree,
       char*    key,
       void**   value,
       long*    valueSize);

pAVLTREE
  CSMAP_PRIVATE_SingleLeftRotation
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_SingleRightRotation
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_DoubleLeftRotation
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_DoubleRightRotation
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_Successor
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_Rebalance
     (pAVLTREE Tree);

void
  CSMAP_PRIVATE_ResetTreeHeight
     (pAVLTREE Tree);

void
  CSMAP_PRIVATE_Clear
     (pAVLTREE Tree);

long
  CSMAP_PRIVATE_Height
     (pAVLTREE Tree);

long
  CSMAP_PRIVATE_ComputeLeftHeight
     (pAVLTREE Tree);

long
  CSMAP_PRIVATE_ComputeRightHeight
     (pAVLTREE Tree);

pAVLTREE
  CSMAP_PRIVATE_Traverse
     (pAVLTREE Tree, CSLIST* keys);

/* --------------------------------------------------------------------------
   CSLIST_Constructor
   Creates an instance of type CSLIST.
-------------------------------------------------------------------------- */

CSLIST*
  CSLIST_Constructor
    (void) {

  CSLIST* pList;

  pList = (CSLIST*)malloc(sizeof(CSLIST));

  pList->first   = 0;
  pList->last    = 0;
  pList->current = 0;

  pList->numItems = 0;
  pList->curIndex = 0;

  return pList;
}

/* --------------------------------------------------------------------------
   CSLIST_Destructor
   Releases the resources of an instance of type CSLIST.
-------------------------------------------------------------------------- */

void
  CSLIST_Destructor
    (CSLIST** This) {

  long i;
  CSLISTNODE* pCurNode;
  CSLISTNODE* pNextNode;

  pCurNode = (*This)->first;
  for (i=0; i<(*This)->numItems; i++) {

    pNextNode = pCurNode->next;

    if (pCurNode->dataSize > 0)
    {
      free(pCurNode->data);
    }

    free(pCurNode);
    pCurNode = pNextNode;
  }

  free(*This);
  *This = 0;
}

/* --------------------------------------------------------------------------
   CSLIST_Clear
   Removes all items from a CSLIST.
-------------------------------------------------------------------------- */

void
  CSLIST_Clear
    (CSLIST* This)
{
  long i;
  CSLISTNODE* pCurNode;
  CSLISTNODE* pNextNode;

  if (This->first != 0) {

    pCurNode = This->first;
    for (i=0; i<This->numItems; i++) {

      pNextNode = pCurNode->next;

      if (pCurNode->dataSize > 0)
      {
        free(pCurNode->data);
      }

      free(pCurNode);
      pCurNode = pNextNode;
    }
  }

  This->numItems = 0;
  This->curIndex = 0;
  This->current  = 0;
  This->first    = 0;
  This->last     = 0;

  return;
}

/* --------------------------------------------------------------------------
   CSLIST_Insert
   Inserts a new item in a CSLIST at a specified index.
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_Insert
    (CSLIST* This,
     void*   value,
     long valueSize,
     long index) {

  CSLISTNODE* NewNode;

  if (This == 0) {
    return CS_FAILURE;
  }

  if (This->numItems == CSLIST_BOTTOM)
  {
    // Very unlikely but who knows!
    return CS_FAILURE;
  }

  // Make sure index makes sense

  if (index >= This->numItems)
  {
    // index is zero-based; if it is
    // equal to at least the number
    // of items, we assume caller
    // wants to insert at the end of
    // the list

    index = CSLIST_BOTTOM;
  }
  else {

    if (index < 0)
    {
      index = CSLIST_BOTTOM;
    }
  }

  // Allocate new node

  NewNode = (CSLISTNODE*)malloc(sizeof(CSLISTNODE));

  NewNode->next     = 0;
  NewNode->previous = 0;
  NewNode->dataSize = valueSize;

  if (NewNode->dataSize > 0)
  {
    NewNode->data = (void*)malloc(NewNode->dataSize);
    memcpy(NewNode->data, value, NewNode->dataSize);
  }
  else
  {
    NewNode->data = 0;
  }

  // insert new node

  if (This->numItems > 0)
  {
    switch(index)
    {
      case CSLIST_TOP:  // We want a new first item

        This->first->previous = NewNode;
        NewNode->next         = This->first;
        This->first           = NewNode;
        This->curIndex        = 0;

        break;

      case CSLIST_BOTTOM: // insert at the end of the list

        CSLIST_PRIVATE_Goto(This, This->numItems-1);

        This->last->next = NewNode;
        NewNode->previous = This->last;

        This->last = NewNode;

        This->curIndex = This->numItems; // because it's zero-based
                                         // we will increment the
                                         // item count later
        break;

      default:

        CSLIST_PRIVATE_Goto(This, index);

        This->current->previous->next = NewNode;
        NewNode->previous = This->current->previous;

        NewNode->next = This->current;
        This->current->previous = NewNode;

        // Note that we need not update the current index
        // because we are merely replacing the current index node

        break;
    }
  }
  else
  {
    // This is the first item in the list
    This->first    = NewNode;
    This->last     = NewNode;
    This->curIndex = 0;
  }

  This->current = NewNode;
  This->numItems++;

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
   CSLIST_Remove
   Removes an item in a CSLIST at a specified index.
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_Remove
    (CSLIST* This,
     long    index) {

  PCSLISTNODE Temp;

  if (index >= This->numItems) {
    return CS_FAILURE;
  }

  if (This->numItems > 0)
  {
    CSLIST_PRIVATE_Goto(This, index);

    Temp = This->current;

    if (This->current == This->first)
    {
      This->first = This->current->next;

      if (This->first != 0) {
        //This is the case where there is only one node
        This->first->previous = 0;
      }

      This->current = This->first;
    }
    else
    {
      if (This->current == This->last)
      {
        This->last = This->last->previous;

        // NOTE: we need not check if there is only one node
        //       because it would have been the first node also
        //       and it would have been taken care of above

        This->last->next = 0;

        This->current = This->last;
        This->curIndex--;
      }
      else
      {
        This->current->previous->next
                  = This->current->next;
        This->current->next->previous
                  = This->current->previous;
        This->current = This->current->next;
      }
    }

    if (Temp->dataSize > 0)
    {
      free(Temp->data);
    }

    free(Temp);

    This->numItems--;

    if (This->numItems == 0)
    {
      This->first    = 0;
      This->last     = 0;
      This->current  = 0;
      This->curIndex = 0;
    }
  }
  else
  {
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
   CSLIST_Count
   Removes the number of items in a CSLIST.
-------------------------------------------------------------------------- */

long
  CSLIST_Count
    (CSLIST* This)
{
    return This->numItems;
}

/* --------------------------------------------------------------------------
   CSLIST_Get
   Retrieves a copy of an item from the CSLIST.
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_Get
    (CSLIST* This,
     void*   value,
     long index)
{
  if (index >= This->numItems) {
    if (index == CSLIST_BOTTOM) {
      index = This->numItems-1;
    }
    else {
      return CS_FAILURE;
    }
  }

  if (This->numItems > 0)
  {
    CSLIST_PRIVATE_Goto(This, index);
    memcpy(value, This->current->data,
                This->current->dataSize);
    return CS_SUCCESS;
  }
  else
  {
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
   CSLIST_GetDataRef
   Returns the address of a specified item; the caller
   can directly access the CSLIST item. Use with care!!!
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_GetDataRef
    (CSLIST* This,
     void**  value,
     long    index)
{
  if (index >= This->numItems) {
    if (index == CSLIST_BOTTOM) {
      index = This->numItems-1;
    }
    else {
      return CS_FAILURE;
    }
  }

  if (This->numItems > 0)
  {
    CSLIST_PRIVATE_Goto(This, index);

    if (This->current->dataSize > 0)
    {
      *value = This->current->data;
    }
    else {
      *value = 0;
    }
  }
  else
  {
    *value = 0;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
   CSLIST_Set
   Replaces an existing item at a specified index location.
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_Set
    (CSLIST* This,
     void*   value,
     long    valueSize,
     long    index)
{
  if (This->numItems > 0)
  {
    CSLIST_PRIVATE_Goto(This, index);

    if (This->current->dataSize > 0)
    {
      free(This->current->data);
    }

    if (valueSize > 0)
    {
      This->current->data = (void*)malloc(valueSize);
      memcpy(This->current->data, value, valueSize);
    }
    else
    {
      This->current->data = 0;
    }

    This->current->dataSize = valueSize;
  }
  else
  {
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
   CSLIST_ItemSize
   returns the size in bytes of an existing item at a
   specified index location.
-------------------------------------------------------------------------- */

long
  CSLIST_ItemSize
    (CSLIST* This,
     long    index) {

  if (This->numItems > 0)
  {
    CSLIST_PRIVATE_Goto(This, index);

    return This->current->dataSize;
  }
  else
  {
    return -1;
  }
}

/* --------------------------------------------------------------------------
   CSLIST_PRIVATE_Goto
   For internal use by a CSLIST; positions the current node
   pointer to a specified item based on its index.
-------------------------------------------------------------------------- */

CSRESULT
  CSLIST_PRIVATE_Goto
    (CSLIST* This,
     long index)
{
  // This method is to speed up positioning on average

  if (index < 0)
  {
    // assume we want to go at the end of the list
    index = This->numItems - 1;
  }

  if (index != This->curIndex)
  {
    if (index < This->curIndex)
    {
      while (index < This->curIndex)
      {
        This->current = This->current->previous;
        This->curIndex--;
      }
    }
    else
    {
      if (index > This->curIndex)
      {
        if (index > (This->numItems - 1))
        {
          index = This->numItems-1;
        }

        while (index > This->curIndex)
        {
          This->current = This->current->next;
          This->curIndex++;
        }
      }
    }
  }

  return CS_SUCCESS;
}

CSMAP*
  CSMAP_Constructor
     (void) {

   CSMAP* pInstance;

   pInstance = (CSMAP*)malloc(sizeof(CSMAP));

   pInstance->tree = 0;

   pInstance->keys = CSLIST_Constructor();

   return pInstance;
}

CSRESULT
  CSMAP_Clear
     (CSMAP* This)
{
  CSLIST_Clear(This->keys);
  CSMAP_PRIVATE_Clear(This->tree);

  This->tree = 0;

  return CS_SUCCESS;
}

void
  CSMAP_Destructor
     (CSMAP** This) {

   if ((*This)->tree != 0)
   {
      CSMAP_Clear(*This);
   }

   CSLIST_Destructor(&(((*This)->keys)));
   free(*This);

   *This = 0;
}

CSRESULT
  CSMAP_Insert
     (CSMAP* This,
       char*  key,
       void*  value,
       long   valueSize) {

   This->tree = CSMAP_PRIVATE_Insert(This->tree,
                             0, key,  value, valueSize);
   return CS_SUCCESS;
}

CSRESULT
  CSMAP_InsertKeyRef
     (CSMAP* This,
       char*  key,
       void*  value,
       long   valueSize) {

   This->tree = CSMAP_PRIVATE_InsertKeyRef(This->tree,
                               0, key,  value, valueSize);
   return CS_SUCCESS;
}

CSRESULT
  CSMAP_Remove
     (CSMAP* This,
       char*  key) {

   This->tree = CSMAP_PRIVATE_Remove(This->tree, key);
   return CS_SUCCESS;
}

CSRESULT
  CSMAP_Lookup
     (CSMAP* This,
       char*  key,
       void** value,
       long*  valueSize) {

  return CSMAP_PRIVATE_Lookup(This->tree,
                               key, value, valueSize);
}

CSRESULT
  CSMAP_IterStart
     (CSMAP* This)
{
   CSLIST_Clear(This->keys);

   CSMAP_PRIVATE_Traverse(This->tree, This->keys);

   This->NextKey = 0;
   return CS_SUCCESS;
}

CSRESULT
  CSMAP_IterNext
     (CSMAP* This,
       char** key,
       void** value,
       long*  valueSize)
{
   long count;
   pAVLTREE pNode;
   count = CSLIST_Count((void*)This->keys);

   if (This->NextKey < count)
   {
      CSLIST_Get(This->keys, (void*)&pNode, This->NextKey);

      *key = (char*)(pNode->key);
      *value = (void*)(pNode->value);
      *valueSize = (long)(pNode->valueSize);

      This->NextKey++;
   }
   else
   {
      *key = 0;
      *value = 0;
      return CS_FAILURE;
   }

   return CS_SUCCESS;
}

CSLIST*
  CSMAP_FRD_GetIter
     (CSMAP* This)
{
   CSLIST_Clear(This->keys);

   CSMAP_PRIVATE_Traverse(This->tree, This->keys);

   return This->keys;
}

void
  CSMAP_PRIVATE_Clear
     (pAVLTREE Tree) {

   if (Tree == 0)
      return;

   if (Tree->left != 0)
   {
      CSMAP_PRIVATE_Clear(Tree->left);
   }

   if (Tree->right != 0)
   {
      CSMAP_PRIVATE_Clear(Tree->right);
   }

   if (Tree->valueSize > 0)
   {
      free(Tree->value);
   }

   // Since we can refernce keys outside the MAP,
   // we must not free outer data; an outside key has
   // a size of zero

   if (Tree->keySize > 0) {
     free(Tree->key);
   }

   free(Tree);

   return;
}

CSRESULT
  CSMAP_PRIVATE_Lookup
     (pAVLTREE This,
       char*    key,
       void**   value,
       long*    valueSize) {

   long compare;

   if (This == 0)
   {
      *valueSize = 0;
      *value = 0;
      return CS_FAILURE;
   }
   else
   {
      compare = strcmp(This->key, key);

      if (compare == 0)  // Keys match
      {
         *valueSize = This->valueSize;
         *value = (void*)This->value;
         return CS_SUCCESS;
      }
      else
      {
         if (compare < 0)  // specified key is greater than current node key
         {
            return CSMAP_PRIVATE_Lookup(This->right, key, value, valueSize);
         }
         else
         {
            return CSMAP_PRIVATE_Lookup(This->left, key, value, valueSize);
         }
      }
   }
}

pAVLTREE
  CSMAP_PRIVATE_Insert
     (pAVLTREE This,
       pAVLTREE parent,
       char*    key,
       void*    value,
       long     valueSize) {

   CSMAPNODE* root;

   long compare;
   long iKeySize;

   if (This == 0)
   {
      if (key == 0)
      {
         return 0;
      }

      /* ---------------------------------------------------------------
       This is the insertion point
      --------------------------------------------------------------- */

      root = (pAVLTREE)malloc(sizeof(CSMAPNODE));

      /* ---------------------------------------------------------------
       * Set the node's key and value
      --------------------------------------------------------------- */

      iKeySize = strlen(key) + 1;
      root->key = (char*)malloc(sizeof(char) * iKeySize);
      strcpy(root->key, key);

      root->keySize = iKeySize;
      root->valueSize = valueSize;

      if (root->valueSize > 0)
      {
         root->value = (char*)malloc(root->valueSize);
         memcpy(root->value, value, root->valueSize);
      }
      else
      {
         root->value = 0;  /* NULL value for this node */
      }

      /* ---------------------------------------------------------------
       * Set the node's relations
      --------------------------------------------------------------- */

      root->height = 1;
      root->parent = parent;
      root->left   = 0;
      root->right  = 0;
   }
   else
   {
      compare = strcmp(This->key, key);
      if (compare == 0)  // Keys match
      {

         /* ---------------------------------------------------------------
          * The tree already has this key ... we will update it's value
         --------------------------------------------------------------- */

         free(This->value);

         This->valueSize = valueSize;

         if (This->valueSize > 0)
         {
            This->value = (char*)malloc(This->valueSize);
            memcpy(This->value, value, This->valueSize);
         }
         else
         {
            This->value = 0;  /* NULL value for this node */
         }

         root = This;
      }
      else
      {
         if (compare < 0)  // specified key is higher than current node key
         {
            /* ------------------------------------------------------------
             * The node will have to be inserted in the
             *  right tree from this node
            ------------------------------------------------------------ */

            This->right  = CSMAP_PRIVATE_Insert(This->right,
                                           This, key, value, valueSize);
         }
         else {  // specified key is lower than current node key

            /* ------------------------------------------------------------
             * The node will have to be inserted in
             * the left tree from this node.
            ------------------------------------------------------------ */

            This->left  = CSMAP_PRIVATE_Insert(This->left,
                                         This, key, value, valueSize);
         }

         // Update root node height and rebalance

         CSMAP_PRIVATE_ResetTreeHeight(This);
         This = CSMAP_PRIVATE_Rebalance(This);

         root = This;
      }
   }

   return root;
}

pAVLTREE
  CSMAP_PRIVATE_InsertKeyRef
     (pAVLTREE This,
       pAVLTREE parent,
       char*    key,
       void*    value,
       long     valueSize) {

   CSMAPNODE* root;

   long compare;

   if (This == 0)
   {
      if (key == 0)
      {
         return 0;
      }

      /* ---------------------------------------------------------------
       This is the insertion point
      --------------------------------------------------------------- */

      root = (pAVLTREE)malloc(sizeof(CSMAPNODE));

      /* ---------------------------------------------------------------
       * Set the node's key and value
      --------------------------------------------------------------- */

      root->key = key;

      root->keySize = 0;
      root->valueSize = valueSize;

      if (root->valueSize > 0)
      {
         root->value = (char*)malloc(root->valueSize);
         memcpy(root->value, value, root->valueSize);
      }
      else
      {
         root->value = 0;  /* NULL value for this node */
      }

      /* ---------------------------------------------------------------
       * Set the node's relations
      --------------------------------------------------------------- */

      root->height = 1;
      root->parent = parent;
      root->left   = 0;
      root->right  = 0;
   }
   else
   {
      compare = strcmp(This->key, key);
      if (compare == 0)  // Keys match
      {

         /* ---------------------------------------------------------------
          * The tree already has this key ... we will update it's value
         --------------------------------------------------------------- */

         free(This->value);

         This->valueSize = valueSize;

         if (This->valueSize > 0)
         {
            This->value = (char*)malloc(This->valueSize);
            memcpy(This->value, value, This->valueSize);
         }
         else
         {
            This->value = 0;  /* NULL value for this node */
         }

         root = This;
      }
      else
      {
         if (compare < 0)  // specified key is higher than current node key
         {
            /* -------------------------------------------------------------
             * The node will have to be inserted in
             * the right tree from this node
            ------------------------------------------------------------- */

            This->right  = CSMAP_PRIVATE_InsertKeyRef(This->right,
                                                This, key, value, valueSize);
         }
         else {  // specified key is lower than current node key

            /* -------------------------------------------------------------
             * The node will have to be inserted
             * in the left tree from this node.
            ------------------------------------------------------------- */

            This->left  = CSMAP_PRIVATE_InsertKeyRef(This->left,
                                         This, key, value, valueSize);
         }

         // Update root node height and rebalance

         CSMAP_PRIVATE_ResetTreeHeight(This);
         This = CSMAP_PRIVATE_Rebalance(This);

         root = This;
      }
   }

   return root;
}

pAVLTREE
  CSMAP_PRIVATE_Remove
     (pAVLTREE This,
       char*    key) {

   long compare;

   pAVLTREE pSuccessor;
   pAVLTREE pNewRoot;

   if (This == 0)
   {
      return 0;
   }
   else
   {
      compare = strcmp(This->key, key);

      if (compare == 0)  // Keys match
      {
         /* -----------------------------------------------------------
          * We remove the node like any BST.
          * Three cases:
          *
          * 1) Node has no children (it is a leaf)
          * 2) Node has a single child
          * 3) Node has two children
          *
          * Case 1)
          *             Just delete the node
          *
          * -------------------------------------------------------- */

         if (This->left == 0 && This->right == 0)
         {
            // Case 1): We are removing a leaf

            if (This->valueSize > 0)
            {
               free(This->value);
            }

            free(This->key);
            free(This);
            return 0;
         }
         else
         {
            if (This->left != 0 && This->right != 0)
            {
               /* ------------------------------------------------------------
                case 3): Node has a left and right tree. In that case,
                we must find the successor node
                (the next node in keyed sequence).

                A few facts about the successor:

                  The successor's left tree is always a null tree
                  The successor has a right tree (could be a null tree or not)

                  The successor's parent must adopt the
                  successor's right tree as its left tree The successor's
                  parent's height could change as a
                  result so we may have to re-balance parent

                  The successor node could be the root's immediate right tree;
                   we must consider this case
               ------------------------------------------------------------ */

               pSuccessor = CSMAP_PRIVATE_Successor(This);

               // We will replace the root node's key and
               // value with those of the successor node

               if (This->valueSize > 0)
               {
                  free(This->value);
               }

               free(This->key);

               This->key       = pSuccessor->key;
               This->value     = pSuccessor->value;
               This->keySize   = pSuccessor->keySize;
               This->valueSize = pSuccessor->valueSize;

               if (pSuccessor == This->right)
               {
                  // This is the case of the successor being the root's
                  // right tree; the root must now adopt the successor's
                  // right tree

                  This->right = pSuccessor->right;

                  if (pSuccessor->right != 0)
                  {
                     pSuccessor->right->parent = This;
                  }

                  // No need to re-balance successor's right tree because by
                  // definition, the successor's right tree is an AVL tree
               }
               else
               {
                  // The successor's parent node must adopt the
                  // successor's right tree as it's left tree

                  pSuccessor->parent->left = pSuccessor->right;

                  if (pSuccessor->right != 0)
                  {
                     pSuccessor->right->parent = pSuccessor->parent;
                  }

                  // successor parent height could change ... recompute

                  CSMAP_PRIVATE_ResetTreeHeight(pSuccessor->parent);

                  // Re-balance successor's parent if necessary

                  pSuccessor->parent =
                               CSMAP_PRIVATE_Rebalance(pSuccessor->parent);
               }

               // We are now done with the successor node itself, we delete it

               free(pSuccessor);

               // The root node may now have a new height ... recompute

               CSMAP_PRIVATE_ResetTreeHeight(This);
               This = CSMAP_PRIVATE_Rebalance(This);

               return This;
            }
            else
            {
               // case 2): only one child
               // connect child to parent parent

               if (This->left != 0)
               {
                  This->left->parent = This->parent;
                  pNewRoot = This->left;
               }
               else
               {
                  This->right->parent = This->parent;
                  pNewRoot = This->right;
               }

               /* let's delete the former root */

               if (This->valueSize > 0)
               {
                  free(This->value);
               }

               free(This->key);
               free(This);

               // return new root to parent; no need to re-balance

               return pNewRoot;
            }
         }
      }
      else
      {
         if (compare < 0)  // specified key is greater than current node key
         {
            This->right = CSMAP_PRIVATE_Remove(This->right, key);
         }
         else
         {
            This->left = CSMAP_PRIVATE_Remove(This->left, key);
         }

         /* Compute new root's height */

         CSMAP_PRIVATE_ResetTreeHeight(This);
         This = CSMAP_PRIVATE_Rebalance(This);

         return This;
      }
   }

   // if we get here, then something is VERY wrong
   // since we considered all cases!
   return 0;
}

void
  CSMAP_PRIVATE_ResetTreeHeight
     (pAVLTREE Tree)
{
   long LeftHeight;
   long RightHeight;

   LeftHeight = CSMAP_PRIVATE_Height(Tree->left);
   RightHeight = CSMAP_PRIVATE_Height(Tree->right);

   if (LeftHeight > RightHeight)
   {
      Tree->height = LeftHeight + 1;
   }
   else
   {
      Tree->height = RightHeight + 1;
   }
}

pAVLTREE
  CSMAP_PRIVATE_Rebalance
     (pAVLTREE root)
{
   long BalanceFactor;
   long LeftHeight;
   long RightHeight;

   LeftHeight = CSMAP_PRIVATE_Height(root->left);
   RightHeight = CSMAP_PRIVATE_Height(root->right);

   BalanceFactor = LeftHeight - RightHeight;

   if (BalanceFactor > 1)  // left heavy
   {
      LeftHeight  = CSMAP_PRIVATE_Height(root->left->left);
      RightHeight = CSMAP_PRIVATE_Height(root->left->right);

      if ( (RightHeight - LeftHeight) > 0 )
      {
         // left subtree is right-heavy
         root = CSMAP_PRIVATE_DoubleRightRotation(root);
      }
      else
      {
         root = CSMAP_PRIVATE_SingleRightRotation(root);
      }
   }
   else
   {
      if (BalanceFactor < -1)  // right heavy
      {
         LeftHeight  = CSMAP_PRIVATE_Height(root->right->left);
         RightHeight = CSMAP_PRIVATE_Height(root->right->right);

         if ( (LeftHeight - RightHeight) > 0 )
         {
            // left subtree is right-heavy
            root = CSMAP_PRIVATE_DoubleLeftRotation(root);
         }
         else
         {
            root = CSMAP_PRIVATE_SingleLeftRotation(root);
         }
      }
   }

   return root;
}

long
  CSMAP_PRIVATE_Height
     (pAVLTREE Tree) {

   if (Tree == 0) {
      return 0;
   }

   return Tree->height;
}

long
  CSMAP_PRIVATE_ComputeRightHeight
     (pAVLTREE Tree) {

   if (Tree == 0)
   {
      return 0;
   }
   else
   {
      if (Tree->right == 0) {
         return 0;
      }
      else {
         return CSMAP_PRIVATE_ComputeRightHeight(Tree->right) + 1;
      }
   }
}

long
  CSMAP_PRIVATE_ComputeLeftHeight
     (pAVLTREE Tree) {


   if (Tree == 0)
   {
      return 0;
   }
   else
   {
      if (Tree->left)
      {
         return CSMAP_PRIVATE_ComputeLeftHeight(Tree->left) + 1;
      }
      else
      {
         return 0;
      }
   }
}

pAVLTREE
  CSMAP_PRIVATE_SingleLeftRotation
     (pAVLTREE This) {

   /* ------------------------------------------------------------------------
    * 1) In this case, the root will become the left child of it's
    *    right child.
    *
    * 2) The root's right child will become the root
    *
    * 3) The left child of the root's right child will become
    *    the right child of the root
    * --------------------------------------------------------------------- */

   pAVLTREE Temp;

   long LeftHeight;
   long RightHeight;

   /* ------------------------------------------------------------------------
    * We now perform the rotation
    * --------------------------------------------------------------------- */

   Temp                = This->right;
   This->right         = This->right->left;
   Temp->left          = This;
   Temp->parent        = This->parent;
   This->parent        = Temp;

   if (This->right)
   {
      This->right->parent = This;
   }

   /* ------------------------------------------------------------------------
    * The rotation might affect the node's height
    *
    * Compute height of former root first.
    * --------------------------------------------------------------------- */

   LeftHeight  = CSMAP_PRIVATE_Height(This->left);
   RightHeight = CSMAP_PRIVATE_Height(This->right);

   if (LeftHeight > RightHeight)
   {
      This->height = LeftHeight + 1;
   }
   else
   {
      This->height = RightHeight + 1;
   }

   /* ------------------------------------------------------------------------
    * Compute height of new root.
    * --------------------------------------------------------------------- */

   LeftHeight  = This->height;
   RightHeight = CSMAP_PRIVATE_Height(Temp->right);

   if (LeftHeight > RightHeight)
   {
      Temp->height = LeftHeight + 1;
   }
   else
   {
      Temp->height = RightHeight + 1;
   }

   /* ------------------------------------------------------------------------
    * The new root is the former root's right child
    * --------------------------------------------------------------------- */

   return Temp;
}

pAVLTREE
  CSMAP_PRIVATE_SingleRightRotation
     (pAVLTREE This) {

   /* ------------------------------------------------------------------------
    * 1) In this case, the root will become the right child of it's
    *    left child.
    *
    * 2) The root's left child will become the root
    *
    * 3) The right child of the root's left child will become
    *    the left child of the root
    * --------------------------------------------------------------------- */

   pAVLTREE Temp;

   long LeftHeight;
   long RightHeight;

   /* ------------------------------------------------------------------------
    * We now perform the rotation
    * --------------------------------------------------------------------- */

   Temp               = This->left;
   This->left         = This->left->right;
   Temp->right        = This;
   Temp->parent       = This->parent;
   This->parent       = Temp;

   if (This->left) {
      This->left->parent = This;
   }

   /* ------------------------------------------------------------------------
    * The rotation might affect the node's height
    *
    * Compute height of former root first.
    * --------------------------------------------------------------------- */

   LeftHeight  = CSMAP_PRIVATE_Height(This->left);
   RightHeight = CSMAP_PRIVATE_Height(This->right);

   if (LeftHeight > RightHeight)
   {
      This->height = LeftHeight + 1;
   }
   else
   {
      This->height = RightHeight + 1;
   }

   /* ------------------------------------------------------------------------
    * We now compute the new root's height
    * --------------------------------------------------------------------- */

   LeftHeight = CSMAP_PRIVATE_Height(Temp->left);
   RightHeight  = This->height;

   if (LeftHeight > RightHeight)
   {
      Temp->height = LeftHeight + 1;
   }
   else
   {
      Temp->height = RightHeight + 1;
   }

   /* ------------------------------------------------------------------------
    * The new root is the former root's right child
    * --------------------------------------------------------------------- */

   return Temp;
}

pAVLTREE
  CSMAP_PRIVATE_DoubleLeftRotation
     (pAVLTREE This) {

  This->right = CSMAP_PRIVATE_SingleRightRotation(This->right);
  return CSMAP_PRIVATE_SingleLeftRotation(This);

}

pAVLTREE
  CSMAP_PRIVATE_DoubleRightRotation
     (pAVLTREE This) {

  This->left = CSMAP_PRIVATE_SingleLeftRotation(This->left);
  return CSMAP_PRIVATE_SingleRightRotation(This);

}

pAVLTREE
  CSMAP_PRIVATE_Successor
     (pAVLTREE Tree) {

   pAVLTREE cur;

   if (Tree->right == 0)
   {
      return 0;
   }

   cur = Tree->right;

   while (cur->left != 0)
   {
      cur = cur->left;
   }

   return cur;
}

pAVLTREE
  CSMAP_PRIVATE_Traverse
     (pAVLTREE Tree,
       CSLIST*   keys) {

      pAVLTREE pNode;

      if (Tree == 0)
         return 0;

      if (Tree->left != 0)
      {
         CSMAP_PRIVATE_Traverse(Tree->left, keys);
      }

      //insert the node in the list rather than copy the key
      pNode = Tree;

      CSLIST_Insert(keys, &pNode, sizeof(pAVLTREE), CSLIST_BOTTOM);

      if (Tree->right != 0)
      {
         CSMAP_PRIVATE_Traverse(Tree->right, keys);
      }

      return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_FromBase64
//
// Converts an Base64 string to its ASCII equivalent.
//
//////////////////////////////////////////////////////////////////////////////

long
  CSSTR_FromBase64
    (unsigned char* in,
     long inSize,
     unsigned char* outBuffer,
     long flags) {

  long i;
  long j;
  long trailing;

  unsigned char* inBuffer;

  ///////////////////////////////////////////////////////////
  //
  // The following table holds the indices of the
  // B64EncodeTable values that are valid B64 characters.
  // All other indices are set to -1, which is an
  // invalid array index. If a character in the B64
  // string resolves to an index value of -1, this means
  // that the B64 string is actually not a B64 string
  // because it has a character that falls outside the
  // values in the B64EncodeTable table.
  //
  ///////////////////////////////////////////////////////////

  char B64DecodeTable[256] = {
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  62,  -1,  -1,  -1,  63,
      52,  53,  54,  55,  56,  57,  58,  59,
      60,  61,  -1,  -1,  -1,  64,  -1,  -1,
      -1,   0,   1,   2,   3,   4,   5,   6,
       7,   8,   9,  10,  11,  12,  13,  14,
      15,  16,  17,  18,  19,  20,  21,  22,
      23,  24,  25,  -1,  -1,  -1,  -1,  -1,
      -1,  26,  27,  28,  29,  30,  31,  32,
      33,  34,  35,  36,  37,  38,  39,  40,
      41,  42,  43,  44,  45,  46,  47,  48,
      49,  50,  51,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1
  };

  inBuffer = (unsigned char*)malloc((inSize * sizeof(unsigned char) * 4) + 1);

  memcpy(inBuffer, in, inSize);

  inBuffer[inSize] = 0;

  // Determine actual size by ignoring padding characters

  //////////////////////////////////////////////////////////////////
  // We could have an overflow... the loop
  // increments the input buffer index within
  // the loop, which holds the potential to
  // overflow the actual input buffer size.
  // We know for a fact that we at most process
  // 3 byte blocks; so to insure that we don't
  // overflow beyond, we will set the loop
  // condition on a 4 byte boundary. We can then
  // have leftover bytes that need to be processed
  // so we will compute how many extra bytes (no
  // more than 2) after the main loop.
  //////////////////////////////////////////////////////////////////

  if (inBuffer[inSize-1] == 61) {

    inSize--;

   if (inBuffer[inSize-1] == 61) {
      inSize--;
    }
  }

  trailing = inSize % 4;
  inSize = inSize - trailing;

  if (flags & CSSTR_B64_MODE_ACCEPTLINEBREAK) {

    // This means we accept linebreks (\r\n)

    for (i=0, j=0; i<inSize; i++, j++) {

      if (B64DecodeTable[inBuffer[i]] == -1) {
        if ((inBuffer[i] != 0x0D) ||
            (inBuffer[i] != 0x0A)) {
          continue;
        }
      }

      //byte 0
      outBuffer[j] = B64DecodeTable[inBuffer[i]] << 2;
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]  >> 4);

      //byte 1
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 4);
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]) >> 2;

      // byte 2
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 6);
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]);
    }
  }
  else {

    for (i=0, j=0; i<inSize; i++, j++) {

      if (B64DecodeTable[inBuffer[i]] == -1) {
        // invalid character
        outBuffer[j] = 0;
        return -1;
      }

      //byte 0
      outBuffer[j] = B64DecodeTable[inBuffer[i]] << 2;
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]] >> 4);

      //byte 1
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]] << 4);
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]] >> 2);

      // byte 2
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 6);
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]);
    }
  }

  switch(trailing) {

    case 0:

      outBuffer[j] = 0;

      break;

    case 1:

      outBuffer[j] = B64DecodeTable[inBuffer[i]] << 2;
      j++;
      outBuffer[j] = 0;

      break;

    case 2:

      outBuffer[j] = B64DecodeTable[inBuffer[i]] << 2;
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]  >> 4);
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 4);
      j++;
      outBuffer[j] = 0;

      break;

    case 3:

      outBuffer[j] = B64DecodeTable[inBuffer[i]] << 2;
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]  >> 4);
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 4);
      i++;
      outBuffer[j] |= (B64DecodeTable[inBuffer[i]]) >> 2;
      j++;
      outBuffer[j] = (B64DecodeTable[inBuffer[i]]  << 6);
      j++;
      outBuffer[j] = 0;

      break;
  }

  free(inBuffer);

  return j;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_ToBase64
//
// Converts an ASCII string to its Base64 equivalent.
//
//////////////////////////////////////////////////////////////////////////////

long
  CSSTR_ToBase64
    (unsigned char* in,
     long inSize,
     unsigned char* outBuffer,
     long flags) {

  long i;
  long j;
  long trailing;

  unsigned char* inBuffer;

  char tempByte;

  /////////////////////////////////////////////////////////////////////////
  // These are the ASCII codes for Base 64 encoding
  /////////////////////////////////////////////////////////////////////////

  char B64EncodeTable[64] = {

      65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
      75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
      85,  86,  87,  88,  89,  90,  97,  98,  99, 100,
     101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
     111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
     121, 122,  48,  49,  50,  51,  52,  53,  54,  55,
     56,  57,  43,  47

  };

  inBuffer = (unsigned char*)malloc((inSize * sizeof(unsigned char) * 4) + 1);

  memcpy(inBuffer, in, inSize);

  inBuffer[inSize] = 0;

  // Determine how many trailing bytes

  trailing = inSize % 3;

  inSize = inSize - trailing;

  if (flags & CSSTR_B64_MODE_ACCEPTLINEBREAK) {

    for (i=0, j=0; i<inSize; i++, j++) {

       if ((j%CSSTR_B64_LINEBREAK_OFFSET)
          == (CSSTR_B64_LINEBREAK_OFFSET-1)) {

         switch(flags & CSSTR_B64_MASK_LINEBREAK) {

           case CSSTR_B64_LINEBREAK_LF:

             outBuffer[j] = 0x0A; // \n
             j++;
             break;

           case CSSTR_B64_LINEBREAK_CRLF:

             outBuffer[j] = 0x0D;  // \r
             j++;
             outBuffer[j] = 0x0A;  // \n
             j++;
             break;
         }
       }

       outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
       tempByte = (inBuffer[i] & B64_MASK_12) << 4;
       j++; i++;

       if ((j%CSSTR_B64_LINEBREAK_OFFSET)
          == (CSSTR_B64_LINEBREAK_OFFSET-1)) {

         switch(flags & CSSTR_B64_MASK_LINEBREAK) {

           case CSSTR_B64_LINEBREAK_LF:

             outBuffer[j] = 0x0A; // \n;
             j++;
             break;

           case CSSTR_B64_LINEBREAK_CRLF:

             outBuffer[j] = 0x0D; // '\r';
             j++;
             outBuffer[j] = 0x0A; // '\n';
             j++;
             break;
         }
       }

       outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_21) >> 4)
                      | tempByte];
       tempByte = (inBuffer[i] & B64_MASK_22) << 2;
       j++; i++;

       if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

         switch(flags & CSSTR_B64_MASK_LINEBREAK) {

           case CSSTR_B64_LINEBREAK_LF:

             outBuffer[j] = 0x0A; //'\n';
             j++;
             break;

           case CSSTR_B64_LINEBREAK_CRLF:

             outBuffer[j] = 0x0D; //'\r';
             j++;
             outBuffer[j] = 0x0A; //'\n';
             j++;
             break;
         }
       }

       outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_31) >> 6)
                      | tempByte];
       j++;

       if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
           (CSSTR_B64_LINEBREAK_OFFSET-1)) {

         switch(flags & CSSTR_B64_MASK_LINEBREAK) {

           case CSSTR_B64_LINEBREAK_LF:

             outBuffer[j] = 0x0A; //'\n';
             j++;
             break;

           case CSSTR_B64_LINEBREAK_CRLF:

             outBuffer[j] = 0x0D; //'\r';
             j++;
             outBuffer[j] = 0x0A; //'\n';
             j++;
             break;
         }
       }

       outBuffer[j] = B64EncodeTable[inBuffer[i] & B64_MASK_32];
    }

    switch(trailing) {

      case 1:

        outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
        tempByte = (inBuffer[i] & B64_MASK_12) << 4;
        j++; i++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;
          }
        }

        outBuffer[j] = B64EncodeTable[tempByte | 0];
        j++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;
          }
        }

        outBuffer[j] = 61; //'=';
        j++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
             j++;
              break;
          }
        }

        outBuffer[j] = 61; //'=';
        j++;

        break;

      case 2:

        outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
        tempByte = (inBuffer[i] & B64_MASK_12) << 4;
        j++; i++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;
          }
        }

        outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_21) >> 4)
                      | tempByte];

        tempByte = (inBuffer[i] & B64_MASK_22) << 2;
        j++; i++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;
          }
        }

        outBuffer[j] = B64EncodeTable[tempByte | 0];
        j++;

        if ((j%CSSTR_B64_LINEBREAK_OFFSET) ==
            (CSSTR_B64_LINEBREAK_OFFSET-1)) {

          switch(flags & CSSTR_B64_MASK_LINEBREAK) {

            case CSSTR_B64_LINEBREAK_LF:

              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;

            case CSSTR_B64_LINEBREAK_CRLF:

              outBuffer[j] = 0x0D; //'\r';
              j++;
              outBuffer[j] = 0x0A; //'\n';
              j++;
              break;
          }
        }

        outBuffer[j] = 61; //'=';
        j++;

        break;
    }
  }
  else {

    for (i=0, j=0; i<inSize; i++, j++) {

      outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
       tempByte = (inBuffer[i] & B64_MASK_12) << 4;
      j++; i++;

      outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_21) >> 4)
       | tempByte];
      tempByte = (inBuffer[i] & B64_MASK_22) << 2;
      j++; i++;

      outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_31) >> 6)
       | tempByte];
      j++;
      outBuffer[j] = B64EncodeTable[inBuffer[i] & B64_MASK_32];
    }

    switch(trailing) {

      case 1:

        outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
        tempByte = (inBuffer[i] & B64_MASK_12) << 4;
        j++; i++;

        outBuffer[j] = B64EncodeTable[tempByte | 0];
        j++;

        outBuffer[j] = 61; //'=';
        j++;
        outBuffer[j] = 61; //'=';
        j++;

        break;

      case 2:

        outBuffer[j] = B64EncodeTable[(inBuffer[i] & B64_MASK_11) >> 2];
        tempByte = (inBuffer[i] & B64_MASK_12) << 4;
        j++; i++;

        outBuffer[j] = B64EncodeTable[((inBuffer[i] & B64_MASK_21) >> 4)
         | tempByte];
        tempByte = (inBuffer[i] & B64_MASK_22) << 2;
        j++; i++;

        outBuffer[j] = B64EncodeTable[tempByte | 0];
        j++;

        outBuffer[j] = 61; //'=';
        j++;

        break;
    }
  }

  free(inBuffer);

  outBuffer[j] = 0;

  return j;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_toUpperCase
//
// Convers a string from lowercase to uppercase.
//
//////////////////////////////////////////////////////////////////////////////

int
  CSSTR_ToUpperCase
    (char* buffer,
     int size) {

   int i = 0;

   while(buffer[i] && i < size)
   {
      buffer[i] = toupper(buffer[i]);
      i++;
   }

   return i;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_Trim
//
// Removes blanks from a string; the original string remains
// unaffected.
//
//////////////////////////////////////////////////////////////////////////////

long
  CSSTR_Trim
    (char* source,
     char* target) {

  long i, j, k;

  i=0;
  while(*(source + i) != 0) {

    if (*(source + i) == ' ')
    {
      i++;
    }
    else {
      break;
    }
  }

  j=i;
  while(*(source + j) != 0)
    j++;

  if (*(source + j) == 0)
    j--;

  while(*(source + j) == ' ')
    j--;

  k=0;
  while(i <= j) {
    *(target + k) = *(source + i);
    i++; k++;
  }

  *(target + k) = 0;
  return k;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_UrlDecode
//
// URL-decodes an URL-encoded string.
//
//////////////////////////////////////////////////////////////////////////////

long
  CSSTR_UrlDecode
    (unsigned char* inStr,
     long InSize,
     unsigned char* out,
     long flags) {

  long i;
  long j;

  unsigned char n;

  unsigned char* in;

  in = (unsigned char*)malloc( (InSize * sizeof(unsigned char) * 4) + 1 );

  memcpy(in, inStr, InSize);

  in[InSize] = 0;

  i=0;
  j=0;
 
  while( in[i] != 0 ){

    if (in[i] == 43) {  // ASCII + sign
        out[j] = 32;
        i++;
        j++;
    }
    else {
      if (in[i] == 37)   // ASCII % sign
      {
        // skip over the percent
        i++;

        // next two characters are hex value;
        // could be digits or characters a-f or A-F

        if (in[i] >= 48 && in[i] <= 57) {
          n = (in[i] - 48) << 4;
        }
        else {

          if (in[i] >= 65 && in[i] <= 70) {
            n = (in[i] - 55) << 4;
          }
          else {

            if (in[i] >= 97 && in[i] <= 102) {
              n = (in[i] - 87) << 4;
            }
            else {
              // error
              out[0] = 0;
              return -1;
            }
          }
        }

        i++;

        if (in[i] >= 48 && in[i] <= 57) {
          n |= (in[i] - 48);
        }
        else {

          if (in[i] >= 65 && in[i] <= 70) {
            n |= (in[i] - 55);
          }
          else {

            if (in[i] >= 97 && in[i] <= 102) {
              n |= (in[i] - 87);
            }
            else {
              // error
              out[0] = 0;
              return -1;
            }
          }
        }

        out[j] = n;

        i++;
        j++;
      }
      else {

        out[j] = in[i];
        i++;
        j++;
      }
    }
  }

  free(in);

  out[j] = 0;

  return j;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_UrlEncode
//
// URL-encodes a string. The caller must insure that
// the resulting buffer is large enough (up to 3 times the size).
//
//////////////////////////////////////////////////////////////////////////////

long
  CSSTR_UrlEncode
    (unsigned char* inStr,
     long InSize,
     unsigned char* out,
     long flags) {

  long i;
  long j;

  char hex[16] = { 48, 49, 50, 51, 52, 53, 54, 55 , 56, 57,
                   65, 66, 67, 68, 69, 70 };

  unsigned char* in;
  
  in = (unsigned char*)malloc( (InSize * sizeof(unsigned char) * 4) + 1 );

  memcpy(in, inStr, InSize);

  in[InSize] = 0;

  i=0;
  j=0;

  if (flags & CSSTR_URLENCODE_CONVERTALL) {

    if (flags & CSSTR_URLENCODE_SPACETOPLUS) {

      while( in[i] != 0) {

        if (in[i] == 0x20) {
          out[j] = 43;  // ASCII plus sign
        }
        else {

          out[j] = 37; // ASCII percent sign (%);
          j++;
          out[j] = hex[in[i] >> 4];
          j++;
          out[j] = hex[in[i] & 15];
        }

        i++;
        j++;
      }
    }
    else {

      while( in[i] != 0) {

        out[j] = 37; // ASCII percent sign (%);
        j++;
        out[j] = hex[in[i] >> 4];
        j++;
        out[j] = hex[in[i] & 15];

        i++;
        j++;
      }
    }
  }
  else {

    if (flags & CSSTR_URLENCODE_SPACETOPLUS) {

      while( in[i] != 0) {

        if ((97 <= in[i] && in[i] <= 122) ||   //ASCII  A to Z
            (65 <= in[i] && in[i] <= 90)  ||   //ASCII  a to z
            (48 <= in[i] && in[i] <= 57)  ||   //ASCII  0 to 9
            in[i] == 0x2D ||                   // '-'
            in[i] == 0x5F ||                   // '_'
            in[i] == 0x2E ||                   // '.'
            in[i] == 0x7E) {                   // '~'

          // Unreserved character
          out[j] = in[i];
        }
        else {

          if (in[i] == 0x20) {
            out[j] = 43;  // ASCII plus sign
          }
          else {

            out[j] = 37; // ASCII percent sign (%);
            j++;
            out[j] = hex[in[i] >> 4];
            j++;
            out[j] = hex[in[i] & 15];
          }
        }

        i++;
        j++;
      }
    }
    else {

      while( in[i] != 0) {

        if ((97 <= in[i] && in[i] <= 122) ||   //ASCII  A to Z
            (65 <= in[i] && in[i] <= 90)  ||   //ASCII  a to z
            (48 <= in[i] && in[i] <= 57)  ||   //ASCII  0 to 9
            in[i] == 0x2D ||                   // '-'
            in[i] == 0x5F ||                   // '_'
            in[i] == 0x2E ||                   // '.'
            in[i] == 0x7E) {                   // '~'

          // Unreserved character
          out[j] = in[i];
        }
        else {

          out[j] = 37; // ASCII percent sign (%);
          j++;
          out[j] = hex[in[i] >> 4];
          j++;
          out[j] = hex[in[i] & 15];
        }

        i++;
        j++;
      }
    }
  }

  free(in);

  out[j] = 0;

  return j;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSYS_MakeUUID
//
// This function generates a UUID and returns its string representation.
//
//////////////////////////////////////////////////////////////////////////////


CSRESULT
  CSSYS_MakeUUID
    (char* szUUID,
     int mode) {

  uuid_t u;

  uuid_generate_random(u);

  if (mode & CSSYS_UUID_LOWERCASE) {
	  uuid_unparse_lower(u, szUUID);
  }
  else {
	  uuid_unparse_upper(u, szUUID);
  }

  return CS_SUCCESS;
}

