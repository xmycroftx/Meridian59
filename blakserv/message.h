// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * message.c
 *
 */

#ifndef _MESSAGE_H
#define _MESSAGE_H

// This causes 3779 message nodes to be allocated for every class (max num
// of messages in an actual class is 400-500). This is a little excessive but
// allows for fewer collisions between hashes.

#define MESSAGE_TABLE_SIZE 3779
#define GetMessageHashNum(a) ((a) % MESSAGE_TABLE_SIZE)


typedef struct message_struct
{
   int message_id;
   char *handler;
   int dstr_id;
   int trace_session_id;
   int untimed_call_count;
   int timed_call_count;
   struct message_struct *propagate_message;
   struct class_struct *propagate_class;
   double total_call_time;
   struct message_struct *next; /* for open hash table linked list */
} message_node;

void InitMessage(void);
void ResetMessage(void);
void SetClassNumMessages(int class_id,int num_messages);
void AddMessage(int class_id,int count,int message_id,char *offset,int dstr_id);
void SetMessagesPropagate(void);
int GetHighestMessageCount(void);
int GetNumMessageHashCollisions(void);

/* Three more header functions in class.h */

extern void PageAlert(char *subject,char *message);
#endif
