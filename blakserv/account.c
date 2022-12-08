// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * account.c
 *
 
 This module keeps a linked list of accounts in memory.  These are loaded in
 from a file (loadacco.c) when Blakserv starts (or initialized by builtin.c).
 The linked list is stored in account number, just so everytime it is
 loaded in and saved the file is in the same order.
 
 */

#include "blakserv.h"

account_node *accounts;
int next_account_id;

account_node console_account_node,*console_account;

/* local function prototypes */
void InsertAccount(account_node *a);

void InitAccount(void)
{
   accounts = NULL;
   next_account_id = 1;

   console_account = &console_account_node;
   console_account->account_id = 0;
   console_account->name = ConfigStr(CONSOLE_ADMINISTRATOR);
   console_account->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,1);
   console_account->password[0] = 0;
   console_account->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,1);
   console_account->email[0] = 0;

   console_account->type = ACCOUNT_ADMIN;
   console_account->last_login_time = 0;
   console_account->suspend_time = 0;
   console_account->seconds_logged_in = 0;
}

void ResetAccount(void)
{
   account_node *a,*temp;

   a = accounts;
   while (a != NULL)
   {
      temp = a->next;
      FreeMemory(MALLOC_ID_ACCOUNT,a->name,strlen(a->name)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a->password,strlen(a->password)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a->email,strlen(a->email)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a,sizeof(account_node));
      a = temp;
   }
   accounts = NULL;
   next_account_id = 1;
}

account_node * GetConsoleAccount()
{
   return console_account;
}

/* GetNextAccountID - used for show status admin command only */
int GetNextAccountID(void)
{
   return next_account_id;
}

void SetNextAccountID(int accountNum)
{
   next_account_id = accountNum;
}

void InsertAccount(account_node *a)
{
   account_node *temp;

   if (accounts == NULL || accounts->account_id > a->account_id)
   {
      a->next = accounts;
      accounts = a;
   }
   else
   {
      temp = accounts;
      while (temp->next != NULL && temp->next->account_id < a->account_id)
         temp = temp->next;
      a->next = temp->next;
      temp->next = a;
   }
}

// Just basic check (no : because it gets used in savegame, must have @)
bool AccountValidateEmail(char *email)
{
   if (!email || strchr(email, ':') || !strchr(email,'@'))
      return false;
   return true;
}

Bool CreateAccount(char *name, char *password, char *email, int type, int *account_id)
{
   char buf[ENCRYPT_LEN + 1];
   account_node *a;

   if (GetAccountByName(name) != NULL)
      return False;

   a = (account_node *)AllocateMemory(MALLOC_ID_ACCOUNT, sizeof(account_node));
   a->account_id = next_account_id++;

   a->name = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(name) + 1);
   strcpy(a->name, name);

   MDString(password, (unsigned char *)buf);
   buf[ENCRYPT_LEN] = 0;
   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(buf) + 1);
   strcpy(a->password, buf);

   if (!email || !AccountValidateEmail(email))
      email = "\0";

   a->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(email) + 1);
   strcpy(a->email, email);

   a->type = type;
   a->last_login_time = 0;
   a->suspend_time = 0;
   a->seconds_logged_in = 0;

   InsertAccount(a);

   *account_id = a->account_id;
   return True;
}

int CreateAccountSecurePassword(char *name,char *password,char *email,int type)
{
   char buf[100],*ptr;
   int index;
   account_node *a;
   unsigned int ch;

   index = 0;
   ptr = password;
   while (sscanf(ptr,"%02x",&ch) == 1)
   {
      buf[index++] = ch;
      ptr += 2;
   }
   buf[index] = 0;

   a = (account_node *)AllocateMemory(MALLOC_ID_ACCOUNT,sizeof(account_node));
   a->account_id = next_account_id++;

   a->name = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(name)+1);
   strcpy(a->name,name);

   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(buf)+1);
   strcpy(a->password,buf);

   if (!email || !AccountValidateEmail(email))
      email = "\0";

   a->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(email)+1);
   strcpy(a->email,email);

   a->type = type;
   a->last_login_time = 0;
   a->suspend_time = 0;
   a->seconds_logged_in = 0;

   InsertAccount(a);

   return a->account_id;
}

int RecreateAccountSecurePassword(int account_id, char *name, char *password, char *email, int type)
{
   char buf[100], *ptr;
   int index;
   account_node *a;
   unsigned int ch;

   index = 0;
   ptr = password;
   while (sscanf(ptr, "%02x", &ch) == 1)
   {
      buf[index++] = ch;
      ptr += 2;
   }
   buf[index] = 0;

   if (GetAccountByID(account_id) != NULL)
   {
      return -1;
   }

   if (next_account_id < account_id)
      next_account_id = account_id;

   a = (account_node *)AllocateMemory(MALLOC_ID_ACCOUNT, sizeof(account_node));
   a->account_id = account_id;
   if (account_id >= next_account_id)
      next_account_id = account_id + 1;

   a->name = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(name) + 1);
   strcpy(a->name, name);

   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(buf) + 1);
   strcpy(a->password, buf);

   if (!email || !AccountValidateEmail(email))
      email = "\0";

   a->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(email) + 1);
   strcpy(a->email, email);

   a->type = type;
   a->last_login_time = 0;
   a->suspend_time = 0;
   a->seconds_logged_in = 0;

   InsertAccount(a);

   return a->account_id;
}

void LoadAccount(int account_id, char *name, char *password, char *email, int type,
                 int last_login_time, int suspend_time, int sec_logged_in)
{
   account_node *a;

   a = (account_node *)AllocateMemory(MALLOC_ID_ACCOUNT, sizeof(account_node));

   a->account_id = account_id;
   if (account_id >= next_account_id)
      next_account_id = account_id + 1;

   a->name = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(name) + 1);
   strcpy(a->name, name);
   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(password) + 1);
   strcpy(a->password, password);

   if (!email || !AccountValidateEmail(email))
      email = "\0";

   a->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT, strlen(email) + 1);
   strcpy(a->email, email);

   a->type = type;
   a->last_login_time = last_login_time;
   a->suspend_time = suspend_time;
   a->seconds_logged_in = sec_logged_in;
   a->next = NULL;
   InsertAccount(a);
}

/* DeleteAccount
   Make sure if you call this you remove all users w/ this account too. */
Bool DeleteAccount(int account_id)
{
   account_node *a,*temp;

   a = accounts;

   /* delete from front of list */
   if (a->account_id == account_id)
   {
      accounts = a->next;
      
      FreeMemory(MALLOC_ID_ACCOUNT,a->name,strlen(a->name)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a->password,strlen(a->password)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a->email,strlen(a->email)+1);
      FreeMemory(MALLOC_ID_ACCOUNT,a,sizeof(account_node));
      return True;
   }
   
   while (a != NULL)
   {
      temp = a->next;
      if (temp != NULL && temp->account_id == account_id)
      {
	 /* remove from list, then free memory */
	 a->next = temp->next;

	 FreeMemory(MALLOC_ID_ACCOUNT,temp->name,strlen(temp->name)+1);
	 FreeMemory(MALLOC_ID_ACCOUNT,temp->password,strlen(temp->password)+1);
    FreeMemory(MALLOC_ID_ACCOUNT,temp->email,strlen(temp->email)+1);
	 FreeMemory(MALLOC_ID_ACCOUNT,temp,sizeof(account_node));
	 return True;
      }
      a = a->next;
   }

   return False;
}

void SetAccountName(account_node *a,char *name)
{
   FreeMemory(MALLOC_ID_ACCOUNT,a->name,strlen(a->name)+1);
   a->name = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(name)+1);
   strcpy(a->name,name);
}

void SetAccountPassword(account_node *a,char *password)
{
   char buf[ENCRYPT_LEN+1];

   FreeMemory(MALLOC_ID_ACCOUNT,a->password,strlen(a->password)+1);
   MDString(password,(unsigned char *) buf);
   buf[ENCRYPT_LEN] = 0;
   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(buf)+1);
   strcpy(a->password,buf);
}

void SetAccountPasswordAlreadyEncrypted(account_node *a,char *password)
{
   FreeMemory(MALLOC_ID_ACCOUNT,a->password,strlen(a->password)+1);
   a->password = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(password)+1);
   strcpy(a->password,password);
}

void SetAccountEmail(account_node *a,char *email)
{
   FreeMemory(MALLOC_ID_ACCOUNT,a->email,strlen(a->email)+1);
   a->email = (char *)AllocateMemory(MALLOC_ID_ACCOUNT,strlen(email)+1);
   strcpy(a->email,email);
}

void SetAccountType(account_node *a, int type)
{
   a->type = type;
}

Bool SuspendAccountAbsolute(account_node *a, int suspend_time)
{
   session_node *s;
   int now = GetTime();

   /* validate arguments */

   if (suspend_time < 0)
   {
      eprintf("SuspendAccountAbsolute: invalid suspend time %d; ignored\n", suspend_time);
      return False;
   }

   if (a == NULL || a->account_id == 0)
   {
      eprintf("SuspendAccountAbsolute: cannot suspend account\n");
      return False;
   }

   /* check for lifting suspension */

   if (suspend_time <= now)
   {
      if (a->suspend_time <= now)
      {
         /* no report for lifting suspension on unsuspended account */
      }
      else
      {
         lprintf("Suspension of account %i (%s) lifted\n",
            a->account_id, a->name);
      }
      a->suspend_time = 0;
      return True;
   }

   /* suspension going into effect or remaining in effect */

   a->suspend_time = suspend_time;

   char suspend_timestr[80];
   strncpy(suspend_timestr, TimeStr(suspend_time), 80);
   suspend_timestr[79] = 0;

   lprintf("Suspended account %i (%s) until %s\n",
      a->account_id, a->name, suspend_timestr);

   s = GetSessionByAccount(a);
   if (s != NULL)
   {
      HangupSession(s);
      PollSession(s->session_id);
      if (GetSessionByAccount(a) != NULL)
      {
         eprintf("SuspendAccountAbsolute: tried to hangup account %i but failed\n",
            a->account_id);
      }
   }

   return True;
}

Bool SuspendAccountRelative(account_node *a, int hours)
{
   // Cap at a reasonable level (i.e. 1hr before 32-bit time breaks in 2038).
   int avail_time = (INT_MAX - 3600 - GetTime()) / 3600;
   if (hours >= avail_time)
      hours = avail_time;

   // if not suspended, hours is relative to now.
   // if suspended, hours is relative to their current suspension.
   int suspend_time = std::max(GetTime(), a->suspend_time) + hours * 60 * 60;

   return SuspendAccountAbsolute(a, suspend_time);
}

int GetActiveAccountCount()
{
	int retval = 0;
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
		if (a->suspend_time <= 0)
			retval++;
		a = a->next;
   }

	return retval;
}

account_node * GetAccountByID(int account_id)
{
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
      if (a->account_id == account_id)
	 return a;
      a = a->next;
   }
   return NULL;
}

account_node * GetAccountByName(char *name)
{
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
      if (!stricmp(a->name,name))
	 return a;
      a = a->next;
   }
   return NULL;
}

// Returns only the first account with that email.
account_node * GetAccountByEmail(char *email)
{
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
      if (!stricmp(a->email,email))
         return a;
      a = a->next;
   }

   return NULL;
}

account_node * AccountLoginByName(char *name)
{
   account_node *a = accounts;

   while (a != NULL)
   {
      if (!stricmp(a->name, name))
         return a;
      a = a->next;
   }

   return NULL;
}

void AccountLogoff(account_node *a)
{
}

void DoneLoadAccounts(void)
{
   kod_statistics *kstat;

   kstat = GetKodStats();

   InitProfiling();
}

void ForEachAccount(void (*callback_func)(account_node *a))
{
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
      callback_func(a);
      a = a->next;
   }
}

void ForEachAccountWithString(void(*callback_func)(account_node *a, char *str), char *str)
{
   account_node *a;

   a = accounts;
   while (a != NULL)
   {
      callback_func(a,str);
      a = a->next;
   }
}

void DeleteAccountAndAssociatedUsersByID(int account_id)
{
   account_node *a;
   session_node *s;

   /* called from timer loop */

   a = GetAccountByID(account_id);
   if (a == NULL)
   {
      eprintf("DeleteAccountAndAssociatedUsersByID: can't delete account %i\n", account_id);
      return;
   }
   s = GetSessionByAccount(a);
   if (s != NULL)
   {
      HangupSession(s);
      PollSession(s->session_id);
      if (GetSessionByAccount(a) != NULL)
      {
         eprintf("DeleteAccountAndAssociatedUsersByID: tried to hangup account %i but failed\n",
            account_id);
         return;
      }
   }

   char last_timestr[80];
   strncpy(last_timestr, TimeStr(a->last_login_time), 80);
   last_timestr[79] = 0;

   lprintf("Attempting delete of account %i (%s) (last login %s)\n",
      account_id, a->name, last_timestr);

   ForEachUserByAccountID(AdminDeleteEachUserObject, account_id);

   DeleteUserByAccountID(account_id);

   if (!DeleteAccount(account_id))
   {
      eprintf("DeleteAccountAndAssociatedUsersByID: unable to delete account %i - unknown reason\n",
         account_id);
      lprintf("Delete of account %i failed - unknown reason\n", account_id);
   }
   else
   {
      lprintf("Delete of account %i successful\n", account_id);
   }
}

// Iterate through accounts and remove unused ones.
void DeleteAccountsIfUnused()
{
   account_node *a, *temp;

   a = accounts;
   while (a != NULL)
   {
      temp = a;
      a = a->next;
      DeleteAccountIfUnused(temp);
   }
}

// Deletes an account and associated users if account has never been logged in.
void DeleteAccountIfUnused(account_node *a)
{
   if (a->last_login_time == 0)
      DeleteAccountAndAssociatedUsersByID(a->account_id);
}

// Compacts the accounts if any have been removed. Called by
// AdminDeleteUnusedAccounts after deleting unused accounts.
void CompactAccounts()
{
   account_node *a;
   int new_number = 1;

   a = accounts;

   while (a != NULL)
   {
      if (a->account_id != new_number)
      {
         ChangeUserAccountID(a->account_id, new_number);
         a->account_id = new_number;
      }
      ++new_number;
      a = a->next;
   }
   SetNextAccountID(new_number);
}
