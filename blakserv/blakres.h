// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/* 
 * blakres.h
 *
 */

#ifndef _BLAKRES_H
#define _BLAKRES_H

typedef struct resource_struct
{
   int resource_id;
   char *resource_val[MAX_LANGUAGE_ID];
   char *resource_name;
   struct resource_struct *next;
} resource_node;

void SaveRsbMD5(char *rsb_hash);
char * GetRsbMD5(void);
void InitResource(void);
void ResetResource(void);
void AddResource(int id, int lang_id, char *str_value);
void SetResourceName(int id,char *name);
int AddDynamicResource(char *str_value);
void ChangeDynamicResourceStr(resource_node *r,char *str_value);
void ChangeDynamicResource(resource_node *r,char *data,int len_data);
int GetNumDynamicRscFiles(void);
int SetNumDynamicRscFiles(int num_files);
resource_node * GetResourceByID(int id);
char * GetResourceStrByLanguageID(int id, int lang_id);
Bool IsResourceByID(int id);
resource_node * GetResourceByName(const char *resource_name);
void ForEachResource(void (*callback_func)(resource_node *r));
void ForEachDynamicRsc(void (*callback_func)(resource_node *r));

#endif
