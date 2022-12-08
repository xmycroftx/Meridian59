// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* adminfn.c
*

  This module has all the administrator functions.  These can be called from an
  account in admin mode (see admin.c) or from an account in game mode, if they
  are an administrator.
  
	The inputted command from admin or game mode should be sent to TryAdminCommand.
	Then, the results of the admin command are funneled through the aprintf function,
	which sends the text straight back to admin mode or encapsulates it into a game
	message.
	
	  The allowed commands are stored in nested tables that have function
	  pointers to the function to call and its parameters for a given
	  command.
	  
		Admin mode is powerful, and much care has to be taken when doing stuff here
		because the session's state can change, and then our state data is no longer
		valid.
		
		  This is the largest source file in Blakserv.  It would be very nice if it
		  were shorter.
		  
*/

#include "blakserv.h"

enum { N,S,I,R }; /* no more params, string param, int param, rest of line param */

enum 
{ 
	A = 0x01,
		M = 0x02
}; /* admin mode can do vs. maintenance mode can do */

#define T True
#define F False

#define MAX_ADMIN_PARM 6
#define MAX_ADMIN_BLAK_PARM 10

typedef UINT64 admin_parm_type;

typedef struct admin_table_struct
{
	void (*admin_func)(int session_id, admin_parm_type parms[],
                      int num_blak_parm, parm_node blak_parms[]);
	int parm_type[MAX_ADMIN_PARM];
	Bool has_blak_parm;
	int permissions;
	struct admin_table_struct *sub_table;
	int len_sub_table;
	const char *admin_cmd;
	const char *help;
} admin_table_type;


void AdminSendBufferList(void);
void SendAdminBuffer(char *buf, int len_buf);

void DoAdminCommand(char *admin_command);
void AdminTable(int len_admin_table, admin_table_type command_table[], int session_id, char *command);
Bool AdminIsValidBlakParm(val_type check_val);

void AdminHelp(int session_id, int len_command_table, admin_table_type command_table[]);

void AdminGarbage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSaveGame(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSaveConfiguration(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSaveOneConfigNode(config_node *c, const char *config_name, const char *default_str);
void AdminWho(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminWhoEachSession(session_node *s);
void AdminLock(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminUnlock(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminMail(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminPage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminShowStatus(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowMemory(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowCalled(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowCalledClass(class_node *c);
void AdminShowRoomTable(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowMatchingIP(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowPost(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void PrintSessionMatchingIP(session_node *s, char *match_ip);
void AdminShowBlockers(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowObject(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowObjects(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowListNode(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowList(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowListParen(int session_id, int list_id);
void AdminShowUsers(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowUsage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowUDP(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowUserHeader(void);
void AdminShowOneUser(user_node *u);
void AdminShowAccounts(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowActive(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowOneAccountIfActive(account_node *a);
void AdminShowAccByEmail(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminPrintAccountByEmail(account_node *a, char *email);
void AdminShowAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowAccountHeader(void);
void AdminShowOneAccount(account_node *a);
void AdminShowOneAccountIfSuspended(account_node *a);
void AdminDeleteUnusedAccounts(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowResource(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminPrintResource(resource_node *r);
void AdminShowDynamicResources(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTimers(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTimer(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTimerMessageID(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTimerObjectID(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTime(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowOneTimer(timer_node *t);
void AdminShowConfiguration(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowOneConfigNode(config_node *c, const char *config_name, const char *default_str);
void AdminShowString(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowDebugString(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowSuspended(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowSysTimers(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowEachSysTimer(systimer_node *st);
void AdminShowOpcodes(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowCalls(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowMessage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowClass(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowPackages(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowOnePackage(dllist_node *dl);
void AdminShowConstant(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTransmitted(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowTable(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowName(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowNameIDs(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminPrintNameID(nameid_node *n);
void AdminShowReferences(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowReferencesEachObject(object_node *o);
void AdminShowReferencesEachList(int list_id, int parent_id);
void AdminShowReferencesEachTable(int table_id, int parent_id);
void AdminShowInstances(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowMatches(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminShowProtocol(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminSetClass(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetObject(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetObjInt(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetAccountName(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetAccountPassword(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetAccountEmail(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetAccountType(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetAccountObject(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
/*void AdminSetResource(int session_id,admin_parm_type parms[]);*/
void AdminSetConfigInt(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetConfigBool(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSetConfigStr(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminSuspendUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSuspendAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminUnsuspendUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminUnsuspendAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminCheckTimerHeap(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminCreateAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateAutomated(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminRecreateAutomated(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminResetHighestTimed(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminAddUserToEachAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateEscapedConvict(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateAdmin(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateDM(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateObject(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateListNode(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateTable(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateTimer(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCreateResource(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminDeleteTimer(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminDeleteAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminDeleteUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminCheckUserLoggedOn(session_node *s);
void AdminSendInt(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSendObject(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSendList(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSendUsers(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSendClass(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminTraceOnMessage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminTraceOffMessage(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminKickoffAll(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminKickoffEachSession(session_node *s);
void AdminKickoffAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminHangupAll(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminHangupEachSession(session_node *s);
void AdminHangupUser(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminHangupAccount(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminHangupSession(int admin_session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminBlockIP(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminReloadSystem(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminReloadGame(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminReloadGameEachSession(session_node *s);
void AdminReloadMotd(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminReloadPackages(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminResetUDP(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminDisableSysTimer(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminEnableSysTimer(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminTerminateNoSave(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminTerminateSave(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminSay(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminSayEachAdminSession(session_node *s);

void AdminRead(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminMark(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

void AdminTestFirst(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminTestRest(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);
void AdminTestGetClass(int session_id, admin_parm_type parms[], int num_blak_parm, parm_node blak_parm[]);

admin_table_type admin_showtimer_table[] =
{
   { AdminShowTimer,          {I,N},   F, A|M, NULL, 0, "id",      "Show one timer by id" },
   { AdminShowTimerMessageID, {R,N}, F, A|M, NULL, 0, "message", "Show timers matching message name" },
   { AdminShowTimerObjectID,  {I,N}, F, A|M, NULL, 0, "object",  "Show timers matching object ID" },
};
#define LEN_ADMIN_SHOWTIMER_TABLE (sizeof(admin_showtimer_table)/sizeof(admin_table_type))

admin_table_type admin_test_table[] =
{
   { AdminTestFirst, {I,N}, F, A|M, NULL, 0, "first", "Benchmark the 'First' instruction, int parm is loop iterations." },
   { AdminTestRest, {I,N}, F, A|M, NULL, 0, "rest", "Benchmark the 'Rest' instruction, int parm is loop iterations." },
   { AdminTestGetClass, {I,N}, F, A|M, NULL, 0, "getclass", "Benchmark the 'getclass' instruction, int parm is loop iterations." },
};
#define LEN_ADMIN_TEST_TABLE (sizeof(admin_test_table)/sizeof(admin_table_type))

admin_table_type admin_show_table[] =
{
	{ AdminShowAccount,       {R,N}, F, A|M, NULL, 0, "account",       "Show one account by account id or name" },
	{ AdminShowAccounts,      {N},   F, A|M, NULL, 0, "accounts",      "Show all accounts" },
	{ AdminShowActive,        {I, N},F, A|M, NULL, 0, "active",        "Show accounts active in the last n days" },
	{ AdminShowObjects,       {I,N}, F, A|M, NULL, 0, "belong",        "Show objects belonging to id" },
	{ AdminShowBlockers,      {I,N}, F, A|M, NULL, 0, "blockers",      "Show all blockers in a room (TAG_ROOM_DATA parameter)" },
	{ AdminShowCalled,        {I,N}, F, A|M, NULL, 0, "called",        "Show top (int) called messages" },
	{ AdminShowCalls,         {I,N}, F, A|M, NULL, 0, "calls",         "Show top (int) C call counts" },
	{ AdminShowClass,         {S,N}, F,A|M, NULL, 0, "class",          "Show info about class" },
	{ AdminShowTime,          {N},   F, A|M, NULL, 0, "clock",         "Show current server time" },
	{ AdminShowConfiguration, {N},   F, A|M, NULL, 0, "config",        "Show configuration values" },
	{ AdminShowConstant,      {S,N}, F,A|M, NULL, 0, "constant",       "Show value of admin constant" },
   { AdminShowDebugString,   {S,I,N}, F,A|M, NULL, 0, "debugstring",  "Show one debug string by class and debugstr id" },
	{ AdminShowDynamicResources,{N}, F, A|M, NULL, 0, "dynamic",       "Show all dynamic resources" },
   { AdminShowAccByEmail,    {S,N}, F, A|M, NULL, 0, "email",         "Show all accounts with a given email address" },
	{ AdminShowInstances,     {S,N}, F, A|M, NULL, 0, "instances",     "Show all instances of class" },
	{ AdminShowList,          {I,N}, F, A|M, NULL, 0, "list",          "Traverse & show a list" },
	{ AdminShowListNode,      {I,N}, F, A|M, NULL, 0, "listnode",      "Show one list node by id" },
	{ AdminShowMatches,       {S,S,S,S,S,N}, F, A|M, NULL, 0, "matches", "Show all instances of class which match criteria" },
	{ AdminShowMemory,        {N},   F, A|M, NULL, 0, "memory",        "Show system memory use" },
	{ AdminShowMessage,       {S,S,N},F,A|M, NULL, 0, "message",       "Show info about class & message" },
	{ AdminShowName,          {R,N}, F, A|M, NULL, 0, "name",          "Show object of user name" },
	{ AdminShowNameIDs,       {N},   F, A|M, NULL, 0, "nameids",       "Show all name ids (message/parms)" },
	{ AdminShowObject,        {I,N}, F, A|M, NULL, 0, "object",        "Show one object by id" },
#if KOD_OPCODE_TESTING
	{ AdminShowOpcodes,       {N},   F, A|M, NULL, 0, "opcodes",       "Show opcode count and timing (enable with PP def KOD_OPCODE_TESTING" },
#endif
	{ AdminShowPackages,      {N},   F,A, NULL, 0, "packages",         "Show all packages loaded" },
	{ AdminShowProtocol,      {N},   F, A|M, NULL, 0, "protocol",      "Show protocol message counts" },
	{ AdminShowReferences,    {S,S,N}, F, A|M, NULL, 0, "references",  "Show what objects or lists reference a particular data value" },
	{ AdminShowResource,      {S,N}, F, A|M, NULL, 0, "resource",      "Show a resource by resource name" },
	{ AdminShowRoomTable,     {N},   F, A|M, NULL, 0, "roomtable",     "Show the rooms table" },
	{ AdminShowMatchingIP,    {I,N}, F, A|M, NULL, 0, "sameip",        "Lists all logged on accounts matching IP of the given session ID." },
	{ AdminShowPost,          {I,I,N},F,A|M, NULL, 0, "post",          "Show a post by news ID and post ID" },
	{ AdminShowStatus,        {N},   F, A|M, NULL, 0, "status",        "Show system status" },
	{ AdminShowString,        {I,N}, F, A|M, NULL, 0, "string",        "Show one string by string id" },
	{ AdminShowSuspended,     {N},   F, A|M, NULL, 0, "suspended",     "Show all suspended accounts" },
	{ AdminShowSysTimers,     {N},   F, A|M, NULL, 0, "systimers",     "Show system timers" },
	{ AdminShowTable,         {I,N}, F, A|M, NULL, 0, "hashtable",     "Show a hash table" },
	{ AdminShowTable,         {I,N}, F, A|M, NULL, 0, "table",         "Show a hash table" },
	{ NULL, {N}, F, A|M, admin_showtimer_table,  LEN_ADMIN_SHOWTIMER_TABLE, "timer", "Timer subcommand" },
	{ AdminShowTimers,        {N},   F, A|M, NULL, 0, "timers",        "Show all timers" },
	{ AdminShowTransmitted,   {N},   F,A, NULL, 0, "transmitted",      "Show # of bytes transmitted in last minute" },
   { AdminShowUDP,           {N},   F, A|M, NULL, 0, "udp",           "Show UDP information (e.g. last received time" },
	{ AdminShowUsage,         {N},   F,A|M,NULL, 0, "usage",           "Show current usage" },
	{ AdminShowUser,          {R,N}, F, A|M, NULL, 0, "user",          "Show one user by name or object id" },
	{ AdminShowUsers,         {N},   F, A|M, NULL, 0, "users",         "Show all users" },
};
#define LEN_ADMIN_SHOW_TABLE (sizeof(admin_show_table)/sizeof(admin_table_type))

admin_table_type admin_setacco_table[] =
{
	{ AdminSetAccountName,     {I,R,N}, F, A|M, NULL, 0, "name", 
		"Set account name by account number and password" },
	{ AdminSetAccountObject,   {I,I,N}, F, A|M, NULL, 0, "object", 
	"Set an object to be the game object for an account, i.e., a character" },
	{ AdminSetAccountPassword, {I,S,N}, F,A|M, NULL, 0, "password", 
	"Set password by account number and password" },
   { AdminSetAccountEmail, { I, S, N }, F, A | M, NULL, 0, "email",
   "Set email by account number and email" },
   { AdminSetAccountType, { I, I, N }, F, A | M, NULL, 0, "type",
   "Set type of account (0 = User, 1 = Admin, 2 = DM) by acct ID and type" },
};
#define LEN_ADMIN_SETACCO_TABLE (sizeof(admin_setacco_table)/sizeof(admin_table_type))

admin_table_type admin_suspend_table[] =
{
	{ AdminSuspendAccount,  {I,R,N}, F, A|M, NULL, 0, "account", 
	"Suspends account by name or id (parm2) for number of hours (parm1)" },
	{ AdminSuspendUser,     {I,R,N}, F, A|M, NULL, 0, "user", 
		"Suspends account by user name for number (parm2) of hours (parm1)" },
};
#define LEN_ADMIN_SUSPEND_TABLE (sizeof(admin_suspend_table)/sizeof(admin_table_type))

admin_table_type admin_unsuspend_table[] =
{
	{ AdminUnsuspendUser,     {R,N}, F, A|M, NULL, 0, "user", 
		"Unsuspends account by user name immediately" },
	{ AdminUnsuspendAccount,  {R,N}, F, A|M, NULL, 0, "account", 
	"Unsuspends account by name or id immediately" },
};
#define LEN_ADMIN_UNSUSPEND_TABLE (sizeof(admin_unsuspend_table)/sizeof(admin_table_type))

admin_table_type admin_check_table[] =
{
   { AdminCheckTimerHeap, {N}, F, A|M, NULL, 0, "timerheap",
      "Checks the validity of the timer heap" },
};
#define LEN_ADMIN_CHECK_TABLE (sizeof(admin_check_table)/sizeof(admin_table_type))

admin_table_type admin_setcfg_table[] =
{
	{ AdminSetConfigBool,     {S,S,S,N}, F, A|M, NULL, 0, "boolean",  
	"Set a modifiable boolean configuration parameter by [group], name, yes/no" },
	{ AdminSetConfigInt,     {S,S,I,N}, F, A|M, NULL, 0, "integer",  
		"Set a modifiable integer configuration parameter by [group], name, new integer value" },
	{ AdminSetConfigStr,      {S,S,R,N}, F, A|M, NULL, 0, "string",  
	"Set a modifiable string configuration parameter by [group], name, new string value" },
};
#define LEN_ADMIN_SETCFG_TABLE (sizeof(admin_setcfg_table)/sizeof(admin_table_type))

admin_table_type admin_set_table[] =
{
	/*
	{ AdminSetResource,   {I,R,N},     F, A|M, NULL, 0, "resource", 
	"Set a dynamic resource to have a different string/filename" },
	*/
	{ NULL, {N}, F, A|M, admin_setacco_table,  LEN_ADMIN_SETACCO_TABLE, "account",  
	"Account subcommand" },
	{ AdminSetClass,      {S,S,S,S,N}, F, A|M, NULL, 0, "class", "Set classvar by name of class, name of var, and value" },
	{ NULL, {N}, F, A|M, admin_setcfg_table,   LEN_ADMIN_SETCFG_TABLE,  "config",   
	"Config subcommand" },
   { AdminSetObjInt, { I,S,S,S,N }, F, A | M, NULL, 0, "integer", "Set object by int (constant) single property" },
	{ AdminSetObject,     {I,S,S,S,N}, F, A|M, NULL, 0, "object", "Set object by id single property" },
};
#define LEN_ADMIN_SET_TABLE (sizeof(admin_set_table)/sizeof(admin_table_type))

admin_table_type admin_create_table[] =
{
	{ AdminCreateAccount, {S,S,S,S,N}, F,A|M,NULL, 0, "account", 
		"Create account by type (user/admin/dm), name, password and email" },
	{ AdminCreateAdmin,   {I,N},   F, A|M, NULL, 0, "admin",   "Create admin object by account id" },
	{ AdminCreateAutomated,{S,S,S,N},F, A|M,NULL, 0, "automated",
	"Create account and user by name, password and email" },
	{ AdminCreateDM,      {I,N},   F, A|M, NULL, 0, "dm",      "Create DM object by account id" },
   { AdminCreateEscapedConvict, { I, N }, F, A | M, NULL, 0, "ec", "Create escaped convict object by account id" },
	{ AdminCreateListNode,{S,S,S,S,N},F, A|M, NULL, 0, "listnode","Create list node" },
	{ AdminCreateObject,  {S,N},   T, A|M, NULL, 0, "object",  "Create object by class name and parms" },
	{ AdminCreateResource,{R,N},   F, A|M, NULL, 0, "resource","Create resource string" },
   { AdminCreateTable,   {I,N},   F, A|M, NULL, 0, "table", "Create table of a given size" },
	{ AdminCreateTimer,   {I,S,I,N},F,A|M, NULL, 0, "timer","Create timer for obj id, message, milli" },
	{ AdminCreateUser,    {I,N},   F, A|M, NULL, 0, "user",    "Create user object by account id" },
	{ AdminAddUserToEachAccount, {N},   F, A|M, NULL, 0, "useroneachaccount", "Add one user object to each account" }
};
#define LEN_ADMIN_CREATE_TABLE (sizeof(admin_create_table)/sizeof(admin_table_type))

admin_table_type admin_delete_table[] =
{
	{ AdminDeleteAccount, {I,N}, F, A|M,NULL, 0, "account","Delete account & user by ID" },
	{ AdminDeleteUnusedAccounts, { N }, F, A | M, NULL, 0, "unused", "Delete unused accounts" },
	{ AdminDeleteTimer,   {I,N}, F, A|M, NULL, 0, "timer",  "Delete timer by ID" },
	{ AdminDeleteUser,    {I,N}, F, A|M, NULL, 0, "user",   "Delete user by object ID" },
};
#define LEN_ADMIN_DELETE_TABLE (sizeof(admin_delete_table)/sizeof(admin_table_type))

admin_table_type admin_send_table[] =
{
	{ AdminSendObject,    {I,S,N}, T, A|M, NULL, 0, "object", "Send object by ID a message" },
	{ AdminSendList,      {I,S,N}, T, A|M, NULL, 0, "list", "Send a message to each object in a list" },
	{ AdminSendInt,       {I,S,N}, T, A|M, NULL, 0, "integer", "Send integer a message" },
	{ AdminSendUsers,     {R,N},   F, A|M, NULL, 0, "users",  "Send logged in people a system message" },
	{ AdminSendClass,     {S,S,N}, T, A|M, NULL, 0, "class",  "Send all objects of class a message" },
};
#define LEN_ADMIN_SEND_TABLE (sizeof(admin_send_table)/sizeof(admin_table_type))

admin_table_type admin_trace_table[] =
{
	{ AdminTraceOffMessage, {S,S,N}, F, A|M, NULL, 0, "off", 
	"Stop tracing message by class & msg names " },
	{ AdminTraceOnMessage,  {S,S,N}, F, A|M, NULL, 0, "on",  "Trace message by class & message names " },
};
#define LEN_ADMIN_TRACE_TABLE (sizeof(admin_trace_table)/sizeof(admin_table_type))

admin_table_type admin_kickoff_table[] =
{
	{ AdminKickoffAccount, {I,N},  F, A|M, NULL, 0, "account", "Kick one account out of the game" },
	{ AdminKickoffAll,     {N},    F, A|M, NULL, 0, "all", "Kick all users out of the game" },
};
#define LEN_ADMIN_KICKOFF_TABLE (sizeof(admin_kickoff_table)/sizeof(admin_table_type))

admin_table_type admin_hangup_table[] =
{
	{ AdminHangupAccount,  {R,N},  F, A|M, NULL, 0, "account", "Hangup one account" },
	{ AdminHangupAll,      {N},    F, A|M, NULL, 0, "all", "Hangup all users" },
	{ AdminBlockIP,        {R,N},  F, A|M, NULL, 0, "ip", "Block an IP address (temporarily)" },
	{ AdminHangupSession,  {I,N},  F, A|M, NULL, 0, "session", "Hangup one session" },
	{ AdminHangupUser,     {R,N},  F, A|M, NULL, 0, "user", "Hangup one user" },
};
#define LEN_ADMIN_HANGUP_TABLE (sizeof(admin_hangup_table)/sizeof(admin_table_type))

admin_table_type admin_reload_table[] =
{
	{ AdminReloadGame,     {I,N}, F, A|M, NULL, 0, "game",   "Reload game from any save time (0 for last)" },
	{ AdminReloadMotd,     {N},   F, A|M, NULL, 0, "motd",   "Reload message of the day from file" },
	{ AdminReloadPackages, {N},   F, A|M, NULL, 0, "packages","Rescan upload directory for packages" },
	{ AdminReloadSystem,   {N},   F, A|M, NULL, 0, "system", "Save game and reload all kod, motd" },
};
#define LEN_ADMIN_RELOAD_TABLE (sizeof(admin_reload_table)/sizeof(admin_table_type))

admin_table_type admin_recreate_table[] =
{
	{ AdminRecreateAutomated,{I,S,S,S,N},F,A|M,NULL, 0, "automated",
		"Create specific account and user by name, password and email" },
};
#define LEN_ADMIN_RECREATE_TABLE (sizeof(admin_recreate_table)/sizeof(admin_table_type))

admin_table_type admin_reset_table[] =
{
   { AdminResetHighestTimed, {N}, F, A|M, NULL, 0, "highest", "Reset highest timed message" },
   { AdminResetUDP,          {N}, F, A|M, NULL, 0, "udp",     "Resets UDP socket" },
};
#define LEN_ADMIN_RESET_TABLE (sizeof(admin_reset_table)/sizeof(admin_table_type))

admin_table_type admin_disable_table[] =
{
	{ AdminDisableSysTimer,{I,N}, F, A|M, NULL, 0, "systimer","Disable a system timer" },
};
#define LEN_ADMIN_DISABLE_TABLE (sizeof(admin_disable_table)/sizeof(admin_table_type))

admin_table_type admin_enable_table[] =
{
	{ AdminEnableSysTimer,{I,N}, F, A|M, NULL, 0, "systimer","Enable a system timer" },
};
#define LEN_ADMIN_ENABLE_TABLE (sizeof(admin_enable_table)/sizeof(admin_table_type))

admin_table_type admin_terminate_table[] =
{
	{ AdminTerminateNoSave,{N},   F, A|M, NULL, 0, "nosave", "Shut down the server without saving" },
	{ AdminTerminateSave,  {N},   F, A|M, NULL, 0, "save",   "Save game and shut down the server" },
};
#define LEN_ADMIN_TERMINATE_TABLE (sizeof(admin_terminate_table)/sizeof(admin_table_type))

admin_table_type admin_save_table[] =
{
	{ AdminSaveConfiguration,{N},F, A|M, NULL, 0, "configuration","Save blakserv.cfg" },
	{ AdminSaveGame,      {N},   F, A|M, NULL, 0, "game",    "Save game (will garbage collect first)" },
};
#define LEN_ADMIN_SAVE_TABLE (sizeof(admin_save_table)/sizeof(admin_table_type))

admin_table_type admin_main_table[] = 
{ 
	{ NULL, {N}, F, A|M, admin_check_table,  LEN_ADMIN_CHECK_TABLE,  "check",  "Check subcommand" },
	{ NULL, {N}, F, A|M, admin_create_table, LEN_ADMIN_CREATE_TABLE, "create", "Create subcommand" },
	{ NULL, {N}, F, A|M, admin_delete_table, LEN_ADMIN_DELETE_TABLE, "delete", "Delete subcommand" },
	{ NULL, {N}, F, A|M, admin_disable_table,LEN_ADMIN_DISABLE_TABLE,"disable", "Disable subcommand" },
	{ NULL, {N}, F, A|M, admin_enable_table, LEN_ADMIN_ENABLE_TABLE, "enable", "Enable subcommand" },
	{ AdminGarbage,       {N},   F, A|M, NULL, 0, "garbage",   "Garbage collect" },
	{ NULL, {N}, F, A|M, admin_hangup_table, LEN_ADMIN_HANGUP_TABLE, "hangup", "Hangup subcommand" },
	{ AdminLock,          {R,N}, F, A|M, NULL, 0, "lock",      "Lock the game" },
	{ NULL, {N}, F, A|M, admin_kickoff_table,LEN_ADMIN_KICKOFF_TABLE,"kickoff","Kickoff subcommand" },
	{ AdminMail,          {N},   F, A|M, NULL, 0, "mail",      "Read administrator mail" },
	{ AdminMark,          {N},   F, A|M, NULL, 0, "mark",      "Mark all channel logs with a dashed line" },
	{ AdminPage,          {N},   F, A|M, NULL, 0, "page",      "Page the console" },
	{ AdminRead,          {S,N}, F, A|M, NULL, 0, "read",      "Read admin commands from a file, echoes everything" },
	{ NULL, {N}, F, A|M, admin_recreate_table,LEN_ADMIN_RECREATE_TABLE, "recreate", "Recreate subcommand" },
	{ NULL, {N}, F, A|M, admin_reload_table, LEN_ADMIN_RELOAD_TABLE, "reload", "Reload subcommand" },
   { NULL, {N}, F, A|M, admin_reset_table, LEN_ADMIN_RESET_TABLE, "reset", "Reset subcommand" },
	{ NULL, {N}, F, A|M, admin_save_table,   LEN_ADMIN_SAVE_TABLE,   "save",   "Save subcommand" },
	{ AdminSay,           {R,N}, F, A|M, NULL, 0, "say",       "Say text to all admins logged in" },
	{ NULL, {N}, F, A|M, admin_send_table,   LEN_ADMIN_SEND_TABLE,   "send",   "Send subcommand" },
	{ NULL, {N}, F, A|M, admin_set_table,    LEN_ADMIN_SET_TABLE,    "set",    "Set subcommand" },
	{ NULL, {N}, F, A|M, admin_show_table,   LEN_ADMIN_SHOW_TABLE,   "show",   "Show subcommand" },
	{ NULL, {N}, F, A|M, admin_suspend_table, LEN_ADMIN_SUSPEND_TABLE,"suspend", "Suspend subcommand" },
	{ NULL, {N}, F, A|M, admin_terminate_table,LEN_ADMIN_TERMINATE_TABLE,"terminate",
	"Terminate subcommand" },
   { NULL, {N}, F, A|M, admin_test_table,  LEN_ADMIN_TEST_TABLE, "test", "Test subcommand" },
	{ NULL, {N}, F, A|M, admin_trace_table,  LEN_ADMIN_TRACE_TABLE,  "trace",  "Trace subcommand" },
	{ AdminUnlock,        {N},   F, A|M, NULL, 0, "unlock",    "Unlock the game" },
	{ NULL, {N}, F, A|M, admin_unsuspend_table, LEN_ADMIN_UNSUSPEND_TABLE,"unsuspend", "Unsuspend subcommand" },
	{ AdminWho,           {N},   F, A|M, NULL, 0, "who",       "Show every account logged on" },
};

#define LEN_ADMIN_MAIN_TABLE (sizeof(admin_main_table)/sizeof(admin_table_type))

int admin_session_id; /* set by TryAdminCommand each time */
static buffer_node *blist; /* same */

char *to_lowercase(char *s)
{
   char* p = s;
   while (*p = tolower(*p)) p++;
   return s;
}

void AdminBufferSend(char *buf,int len_buf)
{
	blist = AddToBufferList(blist,buf,len_buf);
}

void AdminSendBufferList(void)
{
	session_node *s;
	buffer_node *bn = NULL;
	unsigned short len;
	
	s = GetSessionByID(admin_session_id);
	if (s == NULL)
		return; /* must have errored on earlier write and been hung up */
	
	if (s->state == STATE_GAME)
	{
		/* put in length now... kind of a hack */
		len = 0;
		bn = blist;
		
		while (bn != NULL)
		{
			len += bn->len_buf;
			bn = bn->next;
		}
		bn = GetBuffer();
		AddByteToBufferList(bn,(unsigned char)BP_ADMIN);
		AddToBufferList(bn,&len,2);
		bn->next = blist;
		blist = bn;
		
	}
	
	if (blist != NULL)
	{
		SecurePacketBufferList(admin_session_id,blist);
		SendClientBufferList(admin_session_id,blist);
	}
	
	blist = NULL;
}

/* SendAdminBuffer
Sends a block o' bytes to admin_session_id, but not using buffer pool.  Used
only for say and trace. */
void SendAdminBuffer(char *buf,int len_buf)
{ 
	session_node *session;
	
	if (len_buf > BUFFER_SIZE)
	{
		eprintf("SendAdminBuffer sent only first %lu bytes of requested buffer,\n",BUFFER_SIZE);
		len_buf = BUFFER_SIZE;
	}
	else if (len_buf < 0 || !buf)
	{
		eprintf("SendAdminBuffer got invalid buffer\n");
		return;
	}
	
	session = GetSessionByID(admin_session_id);
	if (session == NULL)
		return; /* must have errored on earlier write and been hung up */
	
	switch (session->state)
	{
	case STATE_ADMIN :
		SendClient(admin_session_id,buf,(unsigned short)len_buf);
		break;
	case STATE_GAME :
		AddByteToPacket(BP_ADMIN);
		AddStringToPacket(len_buf,buf);
		SendPacket(admin_session_id);
		break;
	default :
		eprintf("SendAdminBuffer called, SESSION %lu state %lu is not admin or game\n",
			session->session_id,session->state);
	}      
}

/* SendSessionAdminText
This can be called from any module to asynchronously send
admin text. Currently only used for trace info and say. */
void SendSessionAdminText(int session_id,const char *fmt,...)
{
	int prev_admin_session_id;
	
	char s[BUFFER_SIZE];
	va_list marker;
	
	va_start(marker,fmt);
	vsnprintf(s,sizeof(s),fmt,marker);
	va_end(marker);
	
	prev_admin_session_id = admin_session_id;
	
	admin_session_id = session_id;
	
	SendAdminBuffer(s,strlen(s));
	
	admin_session_id = prev_admin_session_id;
}

void TryAdminCommand(int session_id,char *admin_command)
{
	session_node *s;
	
	s = GetSessionByID(session_id);
	if (s == NULL)
	{
		eprintf("TryAdminCommand got invalid SESSION %lu\n",session_id);
		return;
	}
	
	admin_session_id = session_id;
	
	if (blist != NULL)
		eprintf("TryAdminCommand entered with blist = %08x!\n",blist);
	
	blist = NULL;
	
	if (s->account == NULL)
	{
		gprintf("Session: M Command: %s\n",admin_command);
	}
	else
		gprintf("Session: %i Command: %s\n",s->account->account_id,admin_command);
	
	DoAdminCommand(admin_command);
}

void DoAdminCommand(char *admin_command)
{
	char *cmd;
	
	cmd = strtok(admin_command," \t\n");
	
	if (cmd != NULL)
	{
		AdminTable(LEN_ADMIN_MAIN_TABLE,admin_main_table,admin_session_id,cmd);
		AdminSendBufferList();
	}
	
}

void AdminTable(int len_command_table,admin_table_type command_table[],int session_id,
				char *command)
{
	int mode_type,i,index,num_parms,num,num_blak_parm;
	char *parm_str = NULL,*prev_tok;
	admin_parm_type admin_parm[MAX_ADMIN_PARM];
	parm_node blak_parm[MAX_ADMIN_BLAK_PARM];
	val_type blak_val;
	session_node *s;
	
	s = GetSessionByID(session_id);
	if (s == NULL)
	{
		eprintf("AdminTable got invalid SESSION %lu\n",session_id);
		return;
	}
	if (command == NULL || !stricmp(command,"HELP") || !stricmp(command,"?"))
	{
		if (s->state == STATE_MAINTENANCE)
			aprintf("Help is unavailable through maintenance mode.\n");
		else
			AdminHelp(session_id,len_command_table,command_table);
		return;
	}
	
	index = -1;
	for (i=0;i<len_command_table;i++)
	{
		to_lowercase(command);
		if (strstr(command_table[i].admin_cmd,command) 
			== command_table[i].admin_cmd)
		{
			if (stricmp(command_table[i].admin_cmd,command) == 0)
         {
            // Exact match; stop looking
            index = i;
            break;
         }

         if (index != -1)
         {
            aprintf("Ambiguous command; try 'help'.\n");
            return;
			}
			index = i;
		}
	}
	
	if (index == -1)
	{
		aprintf("Unknown command; try 'help'.\n");
		return;
	}
	
	if (command_table[index].admin_func == NULL)
	{
		/* we've got a subtable */
		
		AdminTable(command_table[index].len_sub_table,
			command_table[index].sub_table,session_id,
			strtok(NULL," \t\n"));
		return;
	}
	
	
	/* we've got a function call.  check permissions */
	
	mode_type = 0;
	if (s->state == STATE_MAINTENANCE)
		mode_type |= M;
	if (s->state == STATE_ADMIN || (s->state == STATE_GAME && 
		(s->account->type == ACCOUNT_DM || 
				    s->account->type == ACCOUNT_ADMIN)))
					mode_type |= A;
	
	if ((command_table[index].permissions & mode_type) == 0)
	{
		aprintf("You do not have access to this command.\n");
		return;
	}
	
	/* check parameters */
	
	for (i=0;i<MAX_ADMIN_PARM;i++)
	{
		if (command_table[index].parm_type[i] == N)
			break;
	}
	num_parms = i;
	
	prev_tok = command;
	
	i = 0;
	while (i < num_parms)
	{
		if (command_table[index].parm_type[i] != R) /* for rest of line char (R), don't mess it up*/
		{
			parm_str = strtok(NULL," \t\n");
			if (parm_str == NULL)
			{
				aprintf("Missing parameter %lu.\n",i+1);
				return;
			}
			prev_tok = parm_str;
		}
		switch (command_table[index].parm_type[i])
		{
		case S : 
			admin_parm[i] = (admin_parm_type)parm_str;
			break;
		case I :
			if (sscanf(parm_str,"%d",&num) != 1)
			{
				aprintf("Parameter %d should be an int, not '%s'.\n",
					i+1,parm_str);
				return;
			}
			admin_parm[i] = (admin_parm_type)num;
			break;
		case R :
			/* remember how strtok works to see why this works */
			admin_parm[i] = (admin_parm_type) (prev_tok + strlen(prev_tok) + 1);
			if (!admin_parm[i])
			{
				aprintf("Missing text parameter.\n");
				return;
			}
			/* now make sure no more params */
			prev_tok = NULL;
			break;
		}
		i++;
	}
	
	if (command_table[index].has_blak_parm)
	{
		i = 0;
		while ((parm_str = strtok(NULL," \t\n")) != NULL)
		{
			if (i >= MAX_ADMIN_BLAK_PARM)
			{
				aprintf("Too many parameters, command ignored.\n");
				return;
			}
			blak_parm[i].type = CONSTANT;
			blak_parm[i].name_id = GetIDByName(parm_str);
			if (blak_parm[i].name_id == INVALID_ID)
			{
				aprintf("'%s' is not a valid parameter.\n",parm_str);
				return;
			}
			
			parm_str = strtok(NULL," \t\n");
			if (parm_str == NULL)
			{
				aprintf("Blakod parameter %lu needs tag and value.\n",i+1);
				return;
			}
			num = GetTagNum(parm_str);
			if (num == INVALID_TAG)
			{
				aprintf("Blakod parameter %lu has invalid tag.\n",i+1);
				return;
			}
			blak_val.v.tag = num;
			
			if( num == TAG_TEMP_STRING )	// Added string ("quote") support -- AJM
			{
				char *text;
				parm_str = strtok( NULL, "" );	// Get rest of line for "quote" type parameter
				if (parm_str == NULL)
				{
					aprintf("Blakod parameter %lu needs value\n",i+1);
					return;
				}
				
				text = (char *) parm_str;
				SetTempString( text, strlen( text ) );	// Copies to global temp_str
				
				// we need to set type, name_id, and value
				//	type is CONSTANT (set above)
				//	name_id is the blakod message parameter name (also set above)
				//	value is the union of blak_val.v.tag (set to TAG_TEMP_STRING) and blak_val.v.data
				blak_val.v.data = 0;		/* doesn't matter for TAG_TEMP_STRING */
				blak_parm[i].value = blak_val.int_val;
				
				i++;	// increment to keep count correct
				break;	// bail out of the parse loop
			}
			else
			{
				parm_str = strtok(NULL," \t\n");
				if (parm_str == NULL)
				{
					aprintf("Blakod parameter %lu needs value.\n",i+1);
					return;
				}
				if (0 == stricmp("SELF", parm_str) && blak_val.v.tag == TAG_OBJECT &&
					s && s->state == STATE_GAME && s->game && s->game->object_id != INVALID_OBJECT)
				{
					num = s->game->object_id;
				}
				else if (LookupAdminConstant(parm_str,&num) == False)
				{
					bool negate = false;
					
					// INT parameters may have a negative number and still be legal
					if (blak_val.v.tag == TAG_INT && *parm_str == '-')
					{
						negate = true;
						parm_str++;
					}
					
					num = GetDataNum(blak_val.v.tag,parm_str);
					if (num == INVALID_DATA)
					{
						aprintf("Blakod parameter %lu has invalid data.\n",i+1);
						return;
					}
					
					if (negate)
						num = -num;
				}
				blak_val.v.data = num;
				
				if (AdminIsValidBlakParm(blak_val) == False)
				{
					aprintf("Blakod parameter %lu references invalid data.\n",i+1);
					return;
				}
				
				blak_parm[i].value = blak_val.int_val;
				i++;
			}
			
		}	// end while( next parm != NULL )
		num_blak_parm = i;
		
	}	// end if has blakparm
	
	if (prev_tok != NULL && strtok(NULL," \t\n") != NULL)
	{
		aprintf("Too many parameters, command ignored.\n");
		return;
	}
	if (command_table[index].has_blak_parm)
		command_table[index].admin_func(session_id,admin_parm,num_blak_parm,blak_parm);
	else
		command_table[index].admin_func(session_id,admin_parm, 0, blak_parm);
}

Bool AdminIsValidBlakParm(val_type check_val)
{
	switch (check_val.v.tag)
	{
	case TAG_OBJECT :
		return IsObjectByID(check_val.v.data);
	case TAG_STRING :
		return IsStringByID(check_val.v.data);
	case TAG_LIST :
		return IsListNodeByID(check_val.v.data);
	case TAG_RESOURCE :
		return IsResourceByID(check_val.v.data);
	case TAG_NIL :
		return check_val.v.data == 0;
	case TAG_TEMP_STRING :
		return True;		// "quote" type parm: Assume any (non-null) string is valid
	}      
	
	return True;
}

void AdminHelp(int session_id,int len_command_table,admin_table_type command_table[])
{
	int i,j;
	Bool done_parm;
	
	for (i=0;i<len_command_table;i++)
	{
		aprintf("%-12s ",command_table[i].admin_cmd);
		
		done_parm = False;
		for (j=0;j<MAX_ADMIN_PARM;j++)
		{
			if (command_table[i].parm_type[j] == N)
				done_parm = True;
			if (done_parm)
				aprintf(" ");
			else	    
				switch (command_table[i].parm_type[j])
			{
		case S : aprintf("S"); break;
		case I : aprintf("I"); break;
		case R : aprintf("R"); break;
			}
		}
		if (command_table[i].has_blak_parm)
			aprintf(" P");
		else
			aprintf("  ");
		aprintf(" %s\n",command_table[i].help);
	}
}

void AdminTerminateNoSave(int session_id,admin_parm_type parms[],
                          int num_blak_parm,parm_node blak_parm[])
{
	aprintf("Terminating server. All connections, "
		"including yours, about to be lost...\n");
	
	SetQuit();
}

void AdminTerminateSave(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	lprintf("AdminTerminateSave saving\n");
	
	aprintf("Garbage collecting and saving game... ");
	
	SendBlakodBeginSystemEvent(SYSEVENT_SAVE);
	GarbageCollect();
	SaveAll();
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	SendBlakodEndSystemEvent(SYSEVENT_SAVE);
	
	aprintf("done.\n");
	
	aprintf("Terminating server. All connections, "
		"including yours, about to be lost\n");
	
	SetQuit();
}

void AdminGarbage(int session_id,admin_parm_type parms[],
                  int num_blak_parm,parm_node blak_parm[])                  
{
	lprintf("AdminGarbage garbage collecting\n");
	
	PauseTimers();
	aprintf("Garbage collecting... ");
	AdminSendBufferList();
	
	SendBlakodBeginSystemEvent(SYSEVENT_GARBAGE);
	
	GarbageCollect();
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	SendBlakodEndSystemEvent(SYSEVENT_GARBAGE);
	aprintf("done.\n");
	UnpauseTimers();
	
	ResetBufferPool();
}

void AdminSaveGame(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])
{
	int save_time;
	
	lprintf("AdminSave saving\n");
	
	PauseTimers();
	aprintf("Garbage collecting and saving game... ");
	AdminSendBufferList();
	
	SendBlakodBeginSystemEvent(SYSEVENT_SAVE);
	
	GarbageCollect();
	save_time = SaveAll();
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	
	SendBlakodEndSystemEvent(SYSEVENT_SAVE);
	
	aprintf("done.  Save time is (%i).\n", save_time);
	UnpauseTimers();
}

/* data for ForEachConfigNode */
static FILE *configfile;
void AdminSaveConfiguration(int session_id,admin_parm_type parms[],
                            int num_blak_parm,parm_node blak_parm[])
{
	
	if ((configfile = fopen(CONFIG_FILE,"wt")) == NULL)
	{
		eprintf("AdminSaveConfiguration can't open %s to save config.\n",CONFIG_FILE);
		return;
	}
	
	fprintf(configfile,"# %s\n",BlakServLongVersionString());
	fprintf(configfile,"# Configuration file automatically generated at %s\n",
		TimeStr(GetTime()));
	fprintf(configfile,"# -------------------------------------------\n");
	
	
	ForEachConfigNode(AdminSaveOneConfigNode);
	fclose(configfile);
	
	aprintf("Configuration saved.\n");
}

void AdminSaveOneConfigNode(config_node *c,const char *config_name,const char *default_str)
{
	/* print out non-default config data */ 
	switch (c->config_type)
	{
	case CONFIG_GROUP :
		fprintf(configfile,"\n");
		fprintf(configfile,"%-20s ",config_name);
		fprintf(configfile,"\n");
		break;
	case CONFIG_STR :
		if (strcmp(c->config_str_value,default_str) != 0)
		{
			fprintf(configfile,"%-20s ",config_name);
			if (strchr(c->config_str_value,' ') != NULL)
				fprintf(configfile,"<%s>\n",c->config_str_value);
			else
				fprintf(configfile,"%s\n",c->config_str_value);
		}
		break;
	case CONFIG_INT :
		if (atoi(default_str) != c->config_int_value)
		{
			fprintf(configfile,"%-20s ",config_name);
			fprintf(configfile,"%i\n",c->config_int_value);
		}
		break;
	case CONFIG_PATH :
		if (strcmp(c->config_str_value,default_str) != 0)
		{
			fprintf(configfile,"%-20s ",config_name);
			fprintf(configfile,"%s\n",c->config_str_value);
		}
		break;
	case CONFIG_BOOL :
		if ((stricmp(default_str,"Yes") == 0) ^ c->config_int_value)
		{
			fprintf(configfile,"%-20s ",config_name);
			if (c->config_int_value != False)
				fprintf(configfile,"Yes\n");
			else
				fprintf(configfile,"No\n");
		}
		break;
	default :
		fprintf(configfile,"%-20s ",config_name);
		fprintf(configfile,"xxx # Unknown type\n");
		break;
	}   
}

static int admin_who_count;
void AdminWho(int session_id,admin_parm_type parms[],
              int num_blak_parm,parm_node blak_parm[])
{
	aprintf("\n");
	aprintf("Name                Act Ver Sess Port                   Where\n");
	aprintf("--------------------------------------------------------"
		"--------------------\n");
	
	admin_who_count = 0;
	ForEachSession(AdminWhoEachSession);
}

void AdminWhoEachSession(session_node *s)
{
	const char *str;
	
	if (s->conn.type == CONN_CONSOLE)
		return;
	
	if (s->account != NULL)
		str = s->account->name; 
	else
		str = "?";
	
	aprintf("%-18.18s ",str);
	
	if (s->account != NULL)
		aprintf("%4i ",s->account->account_id);
	else
		aprintf("    ");
	
	if (s->blak_client)
		aprintf("%4i",s->version_major*100+s->version_minor);
	else
		aprintf(" %-3s","No");
	
	aprintf("%4i ",s->session_id);
	aprintf("%-22.22s ",s->conn.name);
	
	aprintf("%s",GetStateName(s));
	
	if (s->state == STATE_GAME)
	{
		val_type name_val;
		resource_node *r;
		
		if (s->game->object_id != INVALID_OBJECT)
		{
			aprintf(" - ");
			
			name_val.int_val = SendTopLevelBlakodMessage(s->game->object_id,USER_NAME_MSG,0,NULL);
			if (name_val.v.tag == TAG_RESOURCE)
			{
				r = GetResourceByID(name_val.v.data);
				if (r == NULL)
					aprintf("Invalid resource id %i",name_val.v.data);
				else
					aprintf("%s",r->resource_val[0]);
			}
			else
				aprintf("Non-resource %i,%i",name_val.v.tag,name_val.v.data);
			aprintf(" (%i)",s->game->object_id);
		}
	}
	aprintf("\n");
	admin_who_count++;
	if ((admin_who_count % 40) == 0)
		AdminSendBufferList();
	
}

void AdminLock(int session_id,admin_parm_type parms[],
               int num_blak_parm,parm_node blak_parm[])               
{
	char *ptr = NULL;
	char *lockstr;
	
	char *text = NULL;
	text = (char *)parms[0];
	
	lockstr = ConfigStr(LOCK_DEFAULT);
	
	ptr = text;
	while (ptr && *ptr > 0)
	{
		if (*ptr != ' ' && *ptr != '\n' && *ptr != '\t')
		{
			lockstr = text;
			break;
		}
		ptr++;
	}
	aprintf("Locking game <%s>.\n",lockstr);
	lprintf("AdminLock locking game (%s).\n",lockstr);   
	SetGameLock(lockstr);
	InterfaceUpdate();
}

void AdminUnlock(int session_id,admin_parm_type parms[],
                 int num_blak_parm,parm_node blak_parm[])
{
	if (!IsGameLocked())
	{
		aprintf("Game isn't locked.\n");
		return;
	}
	
	aprintf("Unlocking game.\n");
	lprintf("AdminUnlock unlocking game.\n");
	SetGameUnlock();
	InterfaceUpdate();
}

void AdminMail(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   enum { MAIL_BUFSIZE = 400 }; /* because we aprintf this buffer, keep small */

   int infile, numread;
   char loadname[MAX_PATH + FILENAME_MAX];
   char buf[MAIL_BUFSIZE + 2];

   sprintf(loadname, "%s%s", ConfigStr(PATH_FORMS), NOTE_FILE);

   if ((infile = open(loadname, O_RDONLY | O_TEXT)) == -1)
   {
      aprintf("Couldn't open mail file.\n");
      return;
   }

   buf[MAIL_BUFSIZE + 1] = 0;
   do
   {
      numread = read(infile, buf, MAIL_BUFSIZE);
      if (numread > 0)
      {
         buf[numread] = 0;
         aprintf("%s", buf);
      }
   } while (numread > 0);

   close(infile);
}

void AdminPage(int session_id,admin_parm_type parms[],
               int num_blak_parm,parm_node blak_parm[])
{
#ifdef BLAK_PLATFORM_WINDOWS
	session_node *s;
	
	s = GetSessionByID(session_id);
	if (s == NULL)
		eprintf("AdminPage admin SESSION %i has no session!",session_id);
	else
		if (s->account != NULL)
			lprintf("AdminPage %s paged the console\n",s->account->name);
		
		InterfaceSignalConsole();
#endif
}


void AdminShowStatus(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	kod_statistics *kstat;
	object_node *o = NULL;
	class_node *c = NULL;
	char *m = NULL;
	int now = GetTime();
	
	aprintf("System Status -----------------------------\n");
	
	aprintf("%s on %s\n",BlakServLongVersionString(),LockConfigStr(CONSOLE_LOCAL_MACHINE_NAME));
	UnlockConfigStr();
	
	kstat = GetKodStats();
	
	aprintf("Current time is %s\n",TimeStr(now));
	aprintf("System started at %s\n(up for %s = %i seconds)\n",
		TimeStr(kstat->system_start_time),
		RelativeTimeStr(now - kstat->system_start_time),
		now - kstat->system_start_time);
	
	aprintf("----\n");
	aprintf("Interpreted %i.%09i billion total instructions in %.2f seconds\n",
		kstat->billions_interpreted,kstat->num_interpreted,
		kstat->interpreting_time / 1000000.0);
	aprintf("Handled %i top level messages, total %i messages\n",
		kstat->num_top_level_messages, kstat->num_messages);
	aprintf("Deepest message call stack is %i calls from top level\n",kstat->message_depth_highest);
	aprintf("Most instructions on one top level message is %i instructions\n",kstat->num_interpreted_highest);
	aprintf("Number of top level messages over 1000 milliseconds is %i\n",kstat->interpreting_time_over_second);
	aprintf("Longest time on one top level message is %i milliseconds\n",(int)(kstat->interpreting_time_highest/1000.0));
	if (kstat->interpreting_time_object_id != INVALID_ID)
	{
		o = GetObjectByID(kstat->interpreting_time_object_id);
		c = o ? GetClassByID(o->class_id) : NULL;
	}
		m = GetNameByID(kstat->interpreting_time_message_id);
		aprintf("Most recent slow top level message is:\nOBJECT %i CLASS %s MESSAGE %s\n",
			kstat->interpreting_time_object_id,
			(char*)(c? c->class_name : "(unknown)"),
			(char*)(m? m : "(unknown)"));
		aprintf("Most recent slow top level message includes %i posted followups\n",
			kstat->interpreting_time_posts);
	
	aprintf("----\n");
	aprintf("Active accounts: %i\n",GetActiveAccountCount());
	aprintf("Next account number is %i\n",GetNextAccountID());
	aprintf("Clients on port %i, maintenance on port %i\n",
		ConfigInt(SOCKET_PORT),
		ConfigInt(SOCKET_MAINTENANCE_PORT));
	aprintf("There are %i sessions logged on\n",
		GetUsedSessions());
	
	aprintf("----\n");
	aprintf("Used %i list nodes\n",GetListNodesUsed());
	aprintf("Used %i tables\n",GetTablesUsed());
	aprintf("Used %i object nodes\n",GetObjectsUsed());
	aprintf("Used %i string nodes\n",GetStringsUsed());
	aprintf("Watching %i active timers\n",GetNumActiveTimers());
	aprintf("%i message hash table collisions\n", GetNumMessageHashCollisions());
	
	if (IsGameLocked())
		aprintf("The game is LOCKED (%s)\n",GetGameLockedReason());
	
	aprintf("-------------------------------------------\n");
}

void AdminShowMemory(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	int i,total;
	memory_statistics *mstat;
	
	aprintf("System Memory -----------------------------\n");
	
	mstat = GetMemoryStats();
	
	total = 0;
	
	aprintf("%s\n",TimeStr(GetTime()));
	for (i=0;i<GetNumMemoryStats();i++)
	{
		aprintf("%-20s %8lu\n",GetMemoryStatName(i),mstat->allocated[i]);
		total += mstat->allocated[i];
	}
	aprintf("%-20s %4lu MB\n","-- Total",total/1024/1024);
	
	aprintf("-------------------------------------------\n");
}

static int show_messages_ignore_count;
static int show_messages_ignore_id;
static int show_messages_timed_count;
static int show_messages_untimed_count;
static int show_messages_total_count;
static double show_messages_time;
static int show_messages_message_id;
static class_node * show_messages_class;
void AdminShowCalled(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
   int i;

   int num_show;
   num_show = (int)parms[0];

   num_show = std::max(1,num_show);
   num_show = std::min(500,num_show);

   aprintf("%4s %-20s %-30s %-11s %s\n", "Rank", "Class", "Message", "Count", "Avg Time(us)");

   show_messages_ignore_count = -1;
   show_messages_ignore_id = -1;
   for (i=0;i<num_show;i++)
   {
      show_messages_timed_count = -1;
      show_messages_untimed_count = -1;
      show_messages_total_count = -1;
      ForEachClass(AdminShowCalledClass);
      if (show_messages_total_count <= 0)
         break;
      aprintf("%3i. %-20s %-30s %-11i %.3f\n",i+1,
         (show_messages_class == NULL)?("Unknown"):show_messages_class->class_name,
         GetNameByID(show_messages_message_id), show_messages_total_count, show_messages_time);
      show_messages_ignore_count = show_messages_total_count;
      show_messages_ignore_id = show_messages_message_id;
   }
}

void AdminShowCalledClass(class_node *c)
{
   int num_calls;
   message_node *m;

   if (!c->num_messages)
      return;
   for (int i = 0; i < MESSAGE_TABLE_SIZE; ++i)
   {
      m = c->messages[i];
      while (m != NULL)
      {
         num_calls = m->timed_call_count + m->untimed_call_count;
         if ((show_messages_ignore_count == -1 ||
            (num_calls < show_messages_ignore_count ||
            (num_calls == show_messages_ignore_count &&
            m->message_id > show_messages_ignore_id))))
         {
            if (num_calls > show_messages_total_count ||
               (num_calls == show_messages_total_count &&
               m->message_id < show_messages_message_id))
            {
               show_messages_timed_count = m->timed_call_count;
               show_messages_untimed_count = m->untimed_call_count;
               show_messages_total_count = num_calls;
               if (show_messages_timed_count)
               {
                  show_messages_time = m->total_call_time / (double)show_messages_timed_count;
               }
               else
               {
                  show_messages_time = 0.0;
               }
               show_messages_message_id = m->message_id;
               show_messages_class = c;
            }
         }
         m = m->next;
      }
   }
}


void AdminShowMatchingIP(int session_id, admin_parm_type parms[],
                        int num_blak_parm, parm_node blak_parm[])
{
   int user_session_id = (int)parms[0];
   session_node *s = GetSessionByID(user_session_id);
   if (!s)
   {
      aprintf("Session %i not found!\n", user_session_id);
      return;
   }

   if (!s->connected)
   {
      aprintf("Session %i not connected!\n", user_session_id);
      return;
   }

   if (s->conn.name == NULL)
   {
      aprintf("Session %i missing IP address!\n");

      return;
   }

   aprintf("Accounts matching IP of session %i:\n",user_session_id);
   aprintf("Name                Act Ver Sess Port                   Where\n");
   aprintf("--------------------------------------------------------"
      "--------------------\n");
   // Data valid, iterate sessions and print matching.
   ForEachSessionWithString(PrintSessionMatchingIP, s->conn.name);
}

void PrintSessionMatchingIP(session_node *s, char *match_ip)
{
   if (s && s->conn.name && stricmp(s->conn.name, match_ip) == 0)
   {
      // Got a match
      AdminWhoEachSession(s);
   }
}

// Posts are stored in kod lists of:
// [ TAG_INT post_id,
//   TAG_OBJECT author object,
//   TAG_INT post_time (with kod offset for int overflow),
//   TAG_STRING unix_time (no offset),
//   TAG_STRING post_title,
//   TAG_STRING post_body ]
void AdminShowPost(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   val_type blak_val, news_obj_id, post_list_id, retrieved_post_id,
      author_id, name_val, post_title_id, post_body_id;
   resource_node *r;
   char *author_name_ptr, author_name[1];

   // Passed in post/news IDs.
   int news_id = (int)parms[0];
   int post_id = (int)parms[1];

   // Step 1: Get newsglobe object.
   blak_val.v.tag = TAG_INT;
   blak_val.v.data = news_id;

   blak_parm[0].type = CONSTANT;
   blak_parm[0].value = blak_val.int_val;

   int blakparm_id = GetIDByName("num");
   if (blakparm_id == INVALID_ID)
   {
      aprintf("Couldn't find ID for parameter num!\n");
      return;
   }
   blak_parm[0].name_id = blakparm_id;

   int message_id = GetIDByName("FindNewsByNum");
   if (message_id == INVALID_ID)
   {
      aprintf("Couldn't find ID for message FindNewsByNum!\n");
      return;
   }

   news_obj_id.int_val = SendTopLevelBlakodMessage(0, message_id, 1, blak_parm);
   if (news_obj_id.int_val == NIL)
   {
      aprintf("Couldn't find news object for news ID %i!\n", news_id);
      return;
   }

   // Step 2: Get the requested news post list.
   blak_val.v.data = post_id;
   blak_parm[0].value = blak_val.int_val;

   blakparm_id = GetIDByName("index");
   if (blakparm_id == INVALID_ID)
   {
      aprintf("Couldn't find ID for parameter index!\n");
      return;
   }
   blak_parm[0].name_id = blakparm_id;
   message_id = GetIDByName("GetArticle");
   if (message_id == INVALID_ID)
   {
      aprintf("Couldn't find ID for message GetArticle!\n");
      return;
   }

   post_list_id.int_val = SendTopLevelBlakodMessage(news_obj_id.v.data, message_id, 1, blak_parm);
   if (post_list_id.int_val == NIL)
   {
      aprintf("Couldn't find post %i!\n",post_id);
      return;
   }

   // Step 3: Confirm we got the right post.
   retrieved_post_id.int_val = First(post_list_id.v.data);
   if (retrieved_post_id.v.data != post_id)
   {
      aprintf("Got mismatch in post ID! Asked for %i, received %i.\n",
         post_id, retrieved_post_id.v.data);

      return;
   }

   // Step 4: Get author name.
   author_id.int_val = Nth(2, post_list_id.v.data); // Reverse from kod Nth
   if (author_id.v.tag != TAG_OBJECT)
   {
      aprintf("Bad object ID for post author\n");
      return;
   }

   name_val.int_val = SendTopLevelBlakodMessage(author_id.v.data, USER_NAME_MSG, 0, NULL);
   author_name[0] = 0;
   author_name_ptr = author_name;
   if (name_val.v.tag == TAG_RESOURCE)
   {
      r = GetResourceByID(name_val.v.data);
      if (r)
         author_name_ptr = r->resource_val[0];
   }

   // Step 5: Get post title.
   post_title_id.int_val = Nth(5, post_list_id.v.data);
   if (post_title_id.v.tag != TAG_STRING)
   {
      aprintf("Bad string ID for post title\n");
      return;
   }
   string_node *post_title = GetStringByID(post_title_id.v.data);
   if (!post_title)
   {
      aprintf("Couldn't find string for post title\n");
      return;
   }

   // Step 6: Get post body.
   post_body_id.int_val = Nth(6, post_list_id.v.data);
   if (post_body_id.v.tag != TAG_STRING)
   {
      aprintf("Bad string ID for post body\n");
      return;
   }
   string_node *post_body = GetStringByID(post_body_id.v.data);
   if (!post_body)
   {
      aprintf("Couldn't find string for post body\n");
      return;
   }

   // Step 7: Print post data.
   aprintf("Showing post ID %i by author: %s, title: %s:\n",
      post_id, author_name_ptr, post_title->data);
   aprintf(post_body->data);
}

void AdminShowRoomTable(int session_id, admin_parm_type parms[],
                        int num_blak_parm, parm_node blak_parm[])
{
   PrintRoomTable();
}

void AdminShowBlockers(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])
{
   room_node *room;
   BlockerNode *b;
   int row, col, finerow, finecol;

   room = GetRoomDataByID((int)parms[0]);
   if (!room)
   {
      aprintf("Invalid roomdata.\n");

      return;
   }

   b = room->data.Blocker;
   while (b)
   {
      row = ROOCOORDTOGRIDBIG(b->Position.Y);
      finerow = ROOCOORDTOGRIDFINE(b->Position.Y);
      col = ROOCOORDTOGRIDBIG(b->Position.X);
      finecol = ROOCOORDTOGRIDFINE(b->Position.X);
      aprintf("Obj ID %i at row %i, %i, col %i, %i\n",
         b->ObjectID, row, finerow, col, finecol);
      b = b->Next;
   }
}

void AdminShowObjects(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])
{
	object_node *o;
	class_node *c;
	int object_id;
	int howManyObjects ;
	int propId,propObjId ;
	int i;
	admin_parm_type lparm[MAX_ADMIN_PARM];

	object_id = (int)parms[0];

	if( IsObjectByID( object_id ) == False )
	{
		aprintf("Invalid object id %i (or it has been deleted).\n",
			object_id);
		return;
	}

	howManyObjects = GetObjectsUsed();

	aprintf("%d owns the following objects - \n",object_id);

	for(i=0;i<howManyObjects;i++) {
		/* attempt to get an object */
		o = GetObjectByIDQuietly( i ) ;
		if ( o != NULL ) {
			/* get the class for the object */
			c = GetClassByID(o->class_id);
			if( c!= NULL ) {
				/* attempt to get poOwner */
				propId = GetPropertyIDByName(c,"poOwner");
				if( propId != INVALID_PROPERTY ) {
					/* valid object, class and has poOwner property */
					propObjId = atol( GetDataName(o->p[ propId ].val) );
					if(propObjId == object_id ) {
						lparm[0] = (admin_parm_type) i;
						AdminShowObject(session_id,lparm, 0, NULL);
					}
				}
			}
		}
	}

}

void AdminShowObject(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])                     
{
	object_node *o;
	class_node *c;
	int i;
	const char *prop_name;
	char buf[200];
	
	int object_id;
	object_id = (int)parms[0];
	
	if (IsObjectByID(object_id) == False)
	{
		aprintf("Invalid object id %i (or it has been deleted).\n",
			object_id);
		return;
	}
	
	o = GetObjectByID(object_id);
	if (o == NULL)
	{
		aprintf("Invalid object id %i (or it has been deleted).\n",
			object_id);
		return;
	}
	
	c = GetClassByID(o->class_id);
	if (c == NULL) /* can't ever be, charlie: yes it can..*/
	{
		aprintf("OBJECT %i has invalid class!\n",o->object_id);
		return;
	}
	
	aprintf(":< OBJECT %i is CLASS %s\n",o->object_id,c->class_name);
	/* = is ok because self is not counted */
	for (i=0;i<=c->num_properties;i++)
	{
		if (i == 0)
			prop_name = "self";
		else
			prop_name = GetPropertyNameByID(c,o->p[i].id);
		if (prop_name == NULL)
			sprintf(buf,": #%-19i",o->p[i].id);
		else
			sprintf(buf,": %-20s",prop_name);
		aprintf("%s = %s %s\n",buf,GetTagName(o->p[i].val),
			GetDataName(o->p[i].val));
	}
	aprintf(":>\n");
	return;
}

void AdminShowListNode(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	list_node *l;
	
	int list_id;
	list_id = (int)parms[0];
	
	l = GetListNodeByID(list_id);
	if (l == NULL)
	{
		aprintf("Invalid list node id %i (or it has been deleted).\n",
			list_id);
		return;
	}
	aprintf(":< first = %s %s\n",GetTagName(l->first),
		GetDataName(l->first));
	aprintf(":  rest = %s %s\n",GetTagName(l->rest),
		GetDataName(l->rest));
	aprintf(":>\n");
}

void AdminShowList(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])
{
   int list_id;
   list_id = (int)parms[0];

   aprintf(":<\n");
   AdminShowListParen(session_id, list_id);
   aprintf(":>\n");
}

void AdminShowListParen(int session_id,int list_id)
{
   list_node *l;
   int count = 0;

   l = GetListNodeByID(list_id);
   if (!l)
   {
      aprintf("Invalid list node id %i (or it has been deleted).\n",
         list_id);
      return;
   }

   aprintf(": [\n");

   if (l->first.v.tag == TAG_LIST)
      AdminShowListParen(session_id, l->first.v.data);
   else
      aprintf(": %s %s\n",GetTagName(l->first), GetDataName(l->first));
   ++count;

   while (l && l->rest.v.tag != TAG_NIL)
   {
      ++count;
      if (l->rest.v.tag != TAG_LIST)
      {
         aprintf(": .\n");
         aprintf(": %s %s\n", GetTagName(l->rest),
            GetDataName(l->rest));
         aprintf(": ] length %i\n", count);
         aprintf("Invalid list node!\n");

         return;
      }

      l = GetListNodeByID(l->rest.v.data);
      if (!l)
      {
         aprintf("Invalid list node id %i (or it has been deleted).\n",
            list_id);
         return;
      }
      if (l->first.v.tag == TAG_LIST)
         AdminShowListParen(session_id, l->first.v.data);
      else
         aprintf(": %s %s\n", GetTagName(l->first), GetDataName(l->first));
   }

   aprintf(": ] length %i\n", count);
}

void AdminShowUsers(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
	AdminShowUserHeader();
	ForEachUser(AdminShowOneUser);
}

void AdminShowUserHeader(void)
{
	aprintf("%-5s%-8s%-8s%s\n","Acct","Object","Class","Name");
}

void AdminShowOneUser(user_node *u)
{
	val_type name_val;
	resource_node *r;
	object_node *o;
	class_node *c;
	
	if (!u)
		return;
	
	o = GetObjectByID(u->object_id);
	if (!o)
		return;
	
	c = GetClassByID(o->class_id);
	if (!c)
		return;
	
	aprintf("%4i %7i %-7s ",u->account_id,u->object_id,c->class_name);
	
	name_val.int_val = SendTopLevelBlakodMessage(u->object_id,USER_NAME_MSG,0,NULL);
	if (name_val.v.tag == TAG_RESOURCE)
	{
		r = GetResourceByID(name_val.v.data);
		if (r == NULL)
			aprintf("Invalid resource id %i.",name_val.v.data);
		else
			aprintf("%s",r->resource_val[0]);
	}
	else
		aprintf("Non-resource %i,%i.",name_val.v.tag,name_val.v.data);
	aprintf("\n");
}

void AdminShowUser(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])                   
{
	user_node *u;
	int id;
	char *ptr;
	Bool is_by_number;
	char *arg_str;
	
	arg_str = (char *)parms[0];
	is_by_number = True;
	
	ptr = arg_str;
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_by_number = False;
		ptr++;
	}
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		u = GetUserByObjectID(id);
	}
	else
	{
		u = GetUserByName(arg_str);
	}
	
	if (u == NULL)
	{
		aprintf("Cannot find user %s.\n",arg_str);
		return;
	}
	
	AdminShowUserHeader();
	AdminShowOneUser(u);
}

void AdminShowUDP(int session_id, admin_parm_type parms[],
                  int num_blak_parm, parm_node blak_parm[])
{
   int last_udp_time = GetLastUDPReadTime();
   int now = GetTime();

   char now_timestr[80], udp_timestr[80];

   strncpy(now_timestr, TimeStr(now), 80);
   now_timestr[79] = 0;
   strncpy(udp_timestr, TimeStr(last_udp_time), 80);
   udp_timestr[79] = 0;

   aprintf(":< Current time %s, last UDP packet received %s\n", now_timestr, udp_timestr);
   aprintf(":>\n");
}

void AdminShowUsage(int session_id, admin_parm_type parms[],
                    int num_blak_parm, parm_node blak_parm[])
{
   aprintf(":< sessions %i\n", GetUsedSessions());
   aprintf(":>\n");
}

void AdminShowAccounts(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	AdminShowAccountHeader();
	ForEachAccount(AdminShowOneAccount);
}

// For checking active accounts.
static int login_time_check;
static int num_active_accts;

void AdminShowActive(int session_id, admin_parm_type parms[],
                     int num_blak_parm, parm_node blak_parm[])
{
   int days = (int)parms[0];

   // Reset active acct number.
   num_active_accts = 0;
   // Set time to compare against to now minus days given.
   login_time_check = GetTime() - days * 86400;
   AdminShowAccountHeader();
   ForEachAccount(AdminShowOneAccountIfActive);

   // Also print the number.
   aprintf("%i accounts active in the last %i days.\n", num_active_accts, days);
}

void AdminShowOneAccountIfActive(account_node *a)
{
   // Check last login time against login_time_check, print acct if active.
   if (a->last_login_time < login_time_check)
      return;

   ++num_active_accts;
   AdminShowOneAccount(a);
}

void AdminShowAccByEmail(int session_id, admin_parm_type parms[],
                         int num_blak_parm, parm_node blak_parm[])
{
   char *email;
   email = (char *)parms[0];

   if (!email)
      return;

   AdminShowAccountHeader();
   ForEachAccountWithString(AdminPrintAccountByEmail, email);
}

void AdminPrintAccountByEmail(account_node *a, char *email)
{
   if (strcmp(a->email, email) != 0)
      return;

   AdminShowOneAccount(a);
}

void AdminShowAccount(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])                      
{
	account_node *a;
	int account_id;
	char *ptr;
	Bool is_account_number;
	
	char *account_str;
	account_str = (char *)parms[0];
	
	is_account_number = True;
	
	ptr = account_str;
	if (!ptr)
	{
		aprintf("Missing account string.\n");
		return;
	}
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_account_number = False;
		ptr++;
	}
	
	if (is_account_number)
	{
		sscanf(account_str,"%i",&account_id);
		a = GetAccountByID(account_id);
	}
	else
		a = GetAccountByName(account_str);
	
	if (a == NULL)
	{
		aprintf("Cannot find account %s.\n",account_str);
		return;
	}
	
	
	AdminShowAccountHeader();
	AdminShowOneAccount(a);
	
	AdminShowUserHeader();
	ForEachUserByAccountID(AdminShowOneUser,a->account_id);
	
}

void AdminShowAccountHeader()
{
   aprintf("%-6s%-23s%-23s%-10s%-8s%-30s\n", "Acct", "Name", "Email",
      "Suspended", "Sec logged in", "Last login");
}

void AdminShowOneAccount(account_node *a)
{
   char ch = ' ';
   static const char* types = " AD"; // see enum ACCOUNT_* in account.h
   char buff[9];

   if (a->type >= 0 && a->type <= (int)strlen(types))
      ch = types[a->type];

   // Check the suspend time.  We don't print a negative time.
   if (a->suspend_time <= GetTime())
   {
      a->suspend_time = 0;
   }

   if (a->suspend_time > 0)
   {
      // Print suspended time left in hours.
      sprintf(buff, "%7.1fh", (a->suspend_time - GetTime()) / (60.0*60.0));
   }
   else
   {
      sprintf(buff, "");
   }

   aprintf("%4i%c %-23s%-23s%8s %4i.%02i %-30s\n", a->account_id, ch, a->name, a->email,
      buff, a->seconds_logged_in / 60, a->seconds_logged_in % 60,
      TimeStr(a->last_login_time));
}

void AdminShowSuspended(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   AdminShowAccountHeader();
   ForEachAccount(AdminShowOneAccountIfSuspended);
}

void AdminShowOneAccountIfSuspended(account_node *a)
{
   // Check the suspend time.  Only printing suspended accounts.
   if (a->suspend_time <= GetTime())
      return;

   AdminShowOneAccount(a);
}

void AdminShowResource(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	int rsc_id;
	resource_node *r;
	
	char *name_resource;
	name_resource = (char *)parms[0];
	
	/* try name_resource as a string or as an int, for handiness */
	
	r = GetResourceByName(name_resource);
	if (r == NULL)
	{
		if (sscanf(name_resource,"%i",&rsc_id) != 1)
		{
			aprintf("There is no resource named %s.\n",name_resource);
			return;
		}
		
		r = GetResourceByID(rsc_id);
		if (r == NULL)
		{
			aprintf("There is no resource with id %s.\n",name_resource);
			return;
		}
	}
	aprintf("%-7s %s\n","ID","Name = Value");
	AdminPrintResource(r);
}

void AdminPrintResource(resource_node *r)
{
	if (r == NULL)
		eprintf("AdminPrintResource got passed NULL\n");
	else
	{
		aprintf("%-7i %s = %s\n",r->resource_id,
			r->resource_name == NULL ? "(dynamic)" : r->resource_name,r->resource_val[0]);
	}
}

void AdminShowDynamicResources(int session_id,admin_parm_type parms[],
                               int num_blak_parm,parm_node blak_parm[])                               
{
	aprintf("%-7s %s\n","ID","Name = Value");
	ForEachDynamicRsc(AdminPrintResource);
}

// Check the validity of the timer min binary heap.
// Prints the result.
void AdminCheckTimerHeap(int session_id, admin_parm_type parms[],
                         int num_blak_parm, parm_node blak_parm[])
{
   double startTime = GetMicroCountDouble();

   // Perform the check, returns true or false.
   bool retVal = TimerHeapCheck(0, 0);

   aprintf("Timer heap check completed in %.3f microseconds.\n",
      GetMicroCountDouble() - startTime);

   if (retVal)
      aprintf("Timer heap is valid.\n");
   else
      aprintf("Timer heap is NOT valid.\n");
}

// Keep track of how many timers get printed.
static int num_timers_printed;

void AdminShowTimers(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
   aprintf("%-7s%-14s%-8s%-20s\n","Timer","Remaining ms","Object",
      "Message");
   num_timers_printed = 0;
   ForEachTimer(AdminShowOneTimer);
   aprintf("%i timers found.\n", num_timers_printed);
}

void AdminShowTimer(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
	timer_node *t;
	
	int timer_id;
	timer_id = (int)parms[0];
	
	t = GetTimerByID(timer_id);
	if (t == NULL)
	{
		aprintf("Cannot find timer %i.\n",timer_id);
		return;
	}
	
	aprintf("%-7s%-14s%-8s%-20s\n","Timer","Remaining ms","Object",
		"Message");
	AdminShowOneTimer(t);
}

void AdminShowTimerMessageID(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
   char *message_str;
   int message_id;

   message_str = (char *)parms[0];
   message_id = GetIDByName(message_str);
   if (message_id == INVALID_ID)
   {
      aprintf("Invalid message name, message not found.");

      return;
   }

   aprintf("%-7s%-14s%-8s%-20s\n","Timer","Remaining ms","Object",
      "Message");
   num_timers_printed = 0;
   ForEachTimerMatchingMsgID(AdminShowOneTimer, message_id);
   aprintf("%i timers found.\n", num_timers_printed);
}

void AdminShowTimerObjectID(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
   int object_id;
   object_id = (int)parms[0];

   aprintf("%-7s%-14s%-8s%-20s\n","Timer","Remaining ms","Object",
      "Message");
   num_timers_printed = 0;
   ForEachTimerMatchingObjID(AdminShowOneTimer, object_id);
   aprintf("%i timers found.\n", num_timers_printed);
}

void AdminShowOneTimer(timer_node *t)
{
   int expire_time;
   object_node *o;

   o = GetObjectByID(t->object_id);
   if (o == NULL)
   {
      aprintf("Timer has invalid object %i?\n", t->object_id);
      return;
   }

   // Increment the count of printed timers.
   ++num_timers_printed;
   expire_time = (int)(t->time - GetMilliCount());

   aprintf("%5i  %-14u%-8i%-20s\n", t->timer_id, expire_time,
      t->object_id, GetNameByID(t->message_id));
}

void AdminShowTime(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])
{
	int now = GetTime();
	
	aprintf("Current server clock reads %i (%s).\n", now, TimeStr(now));
}

void AdminShowConfiguration(int session_id,admin_parm_type parms[],
                            int num_blak_parm,parm_node blak_parm[])
{
	ForEachConfigNode(AdminShowOneConfigNode);
}

void AdminShowOneConfigNode(config_node *c,const char *config_name,const char *default_str)
{
	if (c->config_type == CONFIG_GROUP)
		aprintf("\n");
	
	aprintf("%-20s ",config_name);
	switch (c->config_type)
	{
	case CONFIG_GROUP :
		aprintf("\n");
		break;
	case CONFIG_STR :
		if (strchr(c->config_str_value,' ') != NULL)
			aprintf("<%s>\n",c->config_str_value);
		else
			aprintf("%s\n",c->config_str_value);
		break;
	case CONFIG_INT :
		aprintf("%i\n",c->config_int_value);
		break;
	case CONFIG_PATH :
		aprintf("%s\n",c->config_str_value);
		break;
	case CONFIG_BOOL :
		if (c->config_int_value != False)
			aprintf("Yes\n");
		else
			aprintf("No\n");
		break;
	default :
		aprintf("xxx # Unknown type\n");
		break;
	}   
}

void AdminShowString(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	string_node *snod;
	
	int string_id;
	string_id = (int)parms[0];
	
	if (IsStringByID(string_id) == False)
	{
		aprintf("Cannot find string %i.\n",string_id);
		return;
	}
	
	snod = GetStringByID(string_id);
	if (snod == NULL)
	{
		aprintf("Cannot find string %i.\n",string_id);
		return;
	}
	
	aprintf("String %i\n",string_id);
	aprintf("-------------------------------------------\n");
	/* not null-terminated */
	AdminBufferSend(snod->data,snod->len_data); 
	aprintf("\n-------------------------------------------\n");
}

#define MAX_HANDLED_DSTRING 4192
void AdminShowDebugString(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   char *class_str = (char *)parms[0];
   int debugstr_id = (int)parms[1];

   class_node *c = GetClassByName(class_str);
   if (c == NULL)
   {
      aprintf("Cannot find CLASS %s.\n", class_str);
      return;
   }
   const char *dstr = GetClassDebugStr(c, debugstr_id);

   aprintf("Debug String %i\n", debugstr_id);
   aprintf("-------------------------------------------\n");
   if (strnlen(dstr, MAX_HANDLED_DSTRING) == MAX_HANDLED_DSTRING)
      aprintf("String too large to print, greater than size %i\n", MAX_HANDLED_DSTRING);
   else
      aprintf("%s\n", dstr);
   aprintf("\n-------------------------------------------\n");
}

void AdminShowSysTimers(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])
{
	aprintf("%s %-18s %-15s %-15s %-22s\n","#","System Timer type","Period","On the",
		"Next time");
	ForEachSysTimer(AdminShowEachSysTimer);
}

void AdminShowEachSysTimer(systimer_node *st)
{
  const char *s;
	
	switch (st->systimer_type)
	{
	case SYST_GARBAGE : s = "Garbage collect"; break;
	case SYST_SAVE : s = "Save game"; break;
	case SYST_BLAKOD_HOUR : s = "New Blakod hour"; break;
	case SYST_INTERFACE_UPDATE : s = "Update interface"; break;
	case SYST_RESET_TRANSMITTED : s = "Reset TX count"; break;
	case SYST_RESET_POOL : s = "Reset buffer pool"; break;
	default : s = "Unknown"; break;
	}
	aprintf("%i %-18s %-15s ",st->systimer_type,s,RelativeTimeStr(st->period));
	aprintf("%-15s ",RelativeTimeStr(st->time));
	
	if (st->enabled)
		aprintf("%-22s",TimeStr(st->next_time_activate));
	else
		aprintf("Disabled");
	aprintf("\n");
}

// Times an operation by calling 2 messages, presumably local-store and property-store.
// local_msg and prop_msg are the message names to call, test_desc is the name of the
// operation being tested (e.g. "First" or "IsClass").
void AdminTimeOPLocalProp(int test_count, char *local_msg, char *prop_msg, char *test_desc)
{
   val_type count_val;
   parm_node count_parm[1];

   if (!local_msg || !prop_msg || !test_desc)
   {
      aprintf("Bad string data sent to AdminTimeOPLocalProp!\n");

      return;
   }

   // Safety check count.
   if (test_count <= 0 || test_count > 1500000)
   {
      aprintf("Sent bad loop count %i.  Setting to safe value of 5000.\n", test_count);
      test_count = 5000;
   }
   else if (test_count < 10)
   {
      aprintf("Minimum number of function calls is 10 (function runs \n10x each iteration). Setting to 10.\n");
      test_count = 10;
   }
   else if (test_count % 10 != 0)
   {
      aprintf("Note that the tested function runs 10x in each loop, \nso the requested number of calls is rounded.\n");
   }

   // Create a kod parameter for the number of iterations.
   count_val.v.tag = TAG_INT;
   count_val.v.data = test_count / 10;

   count_parm[0].type = CONSTANT;
   count_parm[0].value = count_val.int_val;
   int blakparm_id = GetIDByName("iCount");
   if (blakparm_id == INVALID_ID)
   {
      aprintf("Couldn't find ID for parameter iCount!\n");
      return;
   }
   count_parm[0].name_id = blakparm_id;

   // Create our test object.
   int object_id = CreateObject(TEST_CLASS, 0, NULL);

   // Get control time for loop.
   message_node *m = GetMessageByName(TEST_CLASS, "TimeEmptyLoop", NULL);
   if (!m)
   {
      aprintf("Could not find control message for testing.\n");
      return;
   }

   double control_time = GetMicroCountDouble();
   SendTopLevelBlakodMessage(object_id, m->message_id, 1, count_parm);
   control_time = GetMicroCountDouble() - control_time;

   // Test op-store-local.
   m = GetMessageByName(TEST_CLASS, local_msg, NULL);
   if (!m)
   {
      aprintf("Could not find message %s.\n", local_msg);
      return;
   }
   double time_local = GetMicroCountDouble();
   SendTopLevelBlakodMessage(object_id, m->message_id, 1, count_parm);
   time_local = GetMicroCountDouble() - control_time - time_local;

   // Test op-store-prop.
   m = GetMessageByName(TEST_CLASS, prop_msg, NULL);
   if (!m)
   {
      aprintf("Could not find message %s.\n", prop_msg);
      return;
   }
   double time_prop = GetMicroCountDouble();
   SendTopLevelBlakodMessage(object_id, m->message_id, 1, count_parm);
   time_prop = GetMicroCountDouble() - control_time - time_prop;

   aprintf("Control %.3f us, %s local: %.3f us, %s prop: %.3f us\n",
      control_time, test_desc, time_local, test_desc, time_prop);
}

void AdminTestFirst(int session_id, admin_parm_type parms[],
                    int num_blak_parm, parm_node blak_parm[])
{
   int test_count = (int)parms[0];

   AdminTimeOPLocalProp(test_count, "TimeFirstStoreLocal", "TimeFirstStoreProperty", "First");
}

void AdminTestRest(int session_id, admin_parm_type parms[],
                   int num_blak_parm, parm_node blak_parm[])
{
   int test_count = (int)parms[0];

   AdminTimeOPLocalProp(test_count, "TimeRestStoreLocal", "TimeRestStoreProperty", "Rest");
}

void AdminTestGetClass(int session_id, admin_parm_type parms[],
                       int num_blak_parm, parm_node blak_parm[])
{
   int test_count = (int)parms[0];
   aprintf("test count:%i\n", test_count);
   AdminTimeOPLocalProp(test_count, "TimeGetClassStoreLocal", "TimeGetClassStoreProperty", "GetClass");
}

void AdminShowOpcodes(int session_id, admin_parm_type parms[],
                      int num_blak_parm, parm_node blak_parm[])
{
#if KOD_OPCODE_TESTING
   kod_statistics *kstat;
   kstat = GetKodStats();
   UINT64 total_opcodes = 0;
   int unused_opcodes = 0;

   for (int i = 0; i < NUMBER_OF_OPCODES; ++i)
   {
      double iTime;

      if (!kstat->opcode_count[i])
      {
         ++unused_opcodes;
         iTime = 0.0;
      }
      else
      {
         total_opcodes += kstat->opcode_count[i];
         // Convert to nanoseconds.
         iTime = kstat->opcode_total_time[i] * 1000.0 / kstat->opcode_count[i];
      }
      aprintf("Opcode: %3i, count: %10llu, time: %8.3f ns\n", i, kstat->opcode_count[i], iTime);
   }
   aprintf("\nUnused opcodes: %i\n", unused_opcodes);
   aprintf("Total instructions: %llu\n", total_opcodes);
#else
   aprintf("Opcode count/timing disabled - enable by building \nblakserv with preprocessor define KOD_OPCODE_TESTING.\n");
#endif
}

void AdminShowCalls(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])                    
{
   int i, count, ignore_val, max_index, totalCalls;
	kod_statistics *kstat;
	char c_name[50];
	
	int num_show;
	num_show = (int)parms[0];
	
	kstat = GetKodStats();
   aprintf("\nNote: IsClass, GetClass, First and Rest are \nnow opcodes and won't show in this list.\n\n");
	aprintf("%4s %-23s %-12s %s\n","Rank","Function","Count","Avg Time(us)");
	
	ignore_val = -1;
	for (count=0;count<num_show;count++)
	{
		max_index = -1;
		for (i=0;i<MAX_C_FUNCTION;i++)
		{
         totalCalls = kstat->c_count_timed[i] + kstat->c_count_untimed[i];
         if (ignore_val == -1 || totalCalls < ignore_val)
			{
            if (max_index == -1 || totalCalls > (kstat->c_count_timed[max_index] + kstat->c_count_untimed[max_index]))
					max_index = i;
			}
		}
      ignore_val = kstat->c_count_timed[max_index] + kstat->c_count_untimed[max_index];
		
		if (ignore_val == 0)
			break;
		
		switch (max_index)
		{
		case CREATEOBJECT : strcpy(c_name, "CreateObject"); break;
		case SENDMESSAGE : strcpy(c_name, "Send"); break;
		case POSTMESSAGE : strcpy(c_name, "Post"); break;
		case SENDLISTMSG : strcpy(c_name, "SendList"); break;
		case SENDLISTMSGBREAK : strcpy(c_name, "SendListBreak"); break;
		case SENDLISTMSGBYCLASS : strcpy(c_name, "SendListByClass"); break;
		case SENDLISTMSGBYCLASSBREAK : strcpy(c_name, "SendListByClassBreak"); break;
		case GODLOG : strcpy(c_name, "GodLog"); break;
		case DEBUG : strcpy(c_name, "Debug"); break;
		case SAVEGAME : strcpy(c_name, "SaveGame"); break;
		case LOADGAME : strcpy(c_name, "LoadGame"); break;
		case ADDPACKET : strcpy(c_name, "AddPacket"); break;
		case SENDPACKET : strcpy(c_name, "SendPacket"); break;
		case SENDCOPYPACKET : strcpy(c_name, "SendCopyPacket"); break;
		case CLEARPACKET : strcpy(c_name, "ClearPacket"); break;
		case GETINACTIVETIME : strcpy(c_name, "GetInactiveTime"); break;
		case STRINGEQUAL : strcpy(c_name, "StringEqual"); break;
		case STRINGCONTAIN : strcpy(c_name, "StringContain"); break;
		case SETRESOURCE : strcpy(c_name, "SetResource"); break;
		case PARSESTRING : strcpy(c_name, "ParseString"); break;
		case SETSTRING : strcpy(c_name, "SetString"); break;
		case CREATESTRING : strcpy(c_name, "CreateString"); break;
		case ISSTRING : strcpy(c_name, "IsString"); break;
		case STRINGSUBSTITUTE : strcpy(c_name, "StringSubstitute"); break;
		case APPENDTEMPSTRING : strcpy(c_name, "AppendTempString"); break;
		case CLEARTEMPSTRING : strcpy(c_name, "ClearTempString"); break;
		case GETTEMPSTRING : strcpy(c_name, "GetTempString"); break;
		case STRINGLENGTH : strcpy(c_name, "StringLength"); break;
		case STRINGCONSISTSOF : strcpy(c_name, "StringConsistsOf"); break;
		case CREATETIMER : strcpy(c_name, "CreateTimer"); break;
		case DELETETIMER : strcpy(c_name, "DeleteTimer"); break;
		case GETTIMEREMAINING : strcpy(c_name, "GetTimeRemaining"); break;
		case ISTIMER : strcpy(c_name, "IsTimer"); break;
		case CHANGESECTORFLAGBSP: strcpy(c_name, "ChangeSectorFlagBSP"); break;
		case MOVESECTORBSP: strcpy(c_name, "MoveSectorBSP"); break;
		case CHANGETEXTUREBSP: strcpy(c_name, "ChangeTextureBSP"); break;
		case CREATEROOMDATA : strcpy(c_name, "CreateRoomData"); break;
		case FREEROOM : strcpy(c_name, "FreeRoom"); break;
		case ROOMDATA : strcpy(c_name, "RoomData"); break;
		case CANMOVEINROOMBSP: strcpy(c_name, "CanMoveInRoomBSP"); break;
		case LINEOFSIGHTVIEW: strcpy(c_name, "LineOfSightView"); break;
		case LINEOFSIGHTBSP: strcpy(c_name, "LineOfSightBSP"); break;
		case GETLOCATIONINFOBSP: strcpy(c_name, "GetLocationInfoBSP"); break;
		case GETSECTORHEIGHTBSP: strcpy(c_name, "GetSectorHeightBSP"); break;
		case SETROOMDEPTHOVERRIDEBSP: strcpy(c_name, "SetRoomDepthOverrideBSP"); break;
		case CALCUSERMOVEMENTBUCKET: strcpy(c_name, "CalcUserMovementBucket"); break;
		case INTERSECTLINECIRCLE: strcpy(c_name, "IntersectLineCircle"); break;
		case GETRANDOMPOINTBSP: strcpy(c_name, "GetRandomPointBSP"); break;
		case GETSTEPTOWARDSBSP: strcpy(c_name, "GetStepTowardsBSP"); break;
		case GETRANDOMMOVEDESTBSP: strcpy(c_name, "GetRandomMoveDestBSP"); break;
		case BLOCKERADDBSP: strcpy(c_name, "BlockerAddBSP"); break;
		case BLOCKERMOVEBSP: strcpy(c_name, "BlockerMoveBSP"); break;
		case BLOCKERREMOVEBSP: strcpy(c_name, "BlockerRemoveBSP"); break;
		case BLOCKERCLEARBSP: strcpy(c_name, "BlockerClearBSP"); break;
		case STRINGTONUMBER : strcpy(c_name, "StringToNumber"); break;
		case APPENDLISTELEM : strcpy(c_name, "AppendListElem"); break;
		case CONS : strcpy(c_name, "Cons"); break;
		case LENGTH : strcpy(c_name, "Length"); break;
		case LAST : strcpy(c_name, "Last"); break;
		case NTH : strcpy(c_name, "Nth"); break;
		case ISLISTMATCH : strcpy(c_name, "IsListMatch"); break;
		case MLIST : strcpy(c_name, "List"); break;
		case ISLIST : strcpy(c_name, "IsList"); break;
		case SETFIRST : strcpy(c_name, "SetFirst"); break;
		case SETNTH : strcpy(c_name, "SetNth"); break;
		case SWAPLISTELEM : strcpy(c_name, "SwapListElem"); break;
		case INSERTLISTELEM : strcpy(c_name, "InsertListElem"); break;
		case DELLISTELEM : strcpy(c_name, "DelListElem"); break;
		case DELLASTLISTELEM: strcpy(c_name, "DelLastListElem"); break;
		case FINDLISTELEM : strcpy(c_name, "FindListElem"); break;
		case GETLISTELEMBYCLASS : strcpy(c_name, "GetListElemByClass"); break;
		case GETLISTNODE : strcpy(c_name, "GetListNode"); break;
		case GETALLLISTNODESBYCLASS : strcpy(c_name, "GetAllListNodesByClass"); break;
		case LISTCOPY : strcpy(c_name, "ListCopy"); break;
		case GETTIME : strcpy(c_name, "GetTime"); break;
		case GETUNIXTIMESTRING: strcpy(c_name, "GetUnixTimeString"); break;
		case OLDTIMESTAMPFIX: strcpy(c_name, "OldTimestampFix"); break;
		case GETTICKCOUNT : strcpy(c_name, "GetTickCount"); break;
		case GETDATEANDTIME : strcpy(c_name, "GetDateAndTime"); break;
		case ABS : strcpy(c_name, "Abs"); break;
		case BOUND : strcpy(c_name, "Bound"); break;
		case SQRT : strcpy(c_name, "Sqrt"); break;
		case CREATETABLE : strcpy(c_name, "CreateTable"); break;
		case ADDTABLEENTRY : strcpy(c_name, "AddTableEntry"); break;
		case GETTABLEENTRY : strcpy(c_name, "GetTableEntry"); break;
		case DELETETABLEENTRY : strcpy(c_name, "DeleteTableEntry"); break;
		case DELETETABLE : strcpy(c_name, "DeleteTable"); break;
		case ISTABLE : strcpy(c_name, "IsTable"); break;
		case ISOBJECT : strcpy(c_name, "IsObject"); break;
		case RECYCLEUSER : strcpy(c_name, "RecycleUser"); break;
		case RANDOM : strcpy(c_name, "Random"); break;
		case RECORDSTAT : strcpy(c_name, "RecordStat"); break;
		case GETSESSIONIP : strcpy(c_name, "GetSessionIP"); break;
		case SETCLASSVAR : strcpy(c_name, "SetClassVar"); break;
			
		default : 
			sprintf(c_name,"Unknown (%i)",max_index);
			break;
		}
      double avgTime = 0.0;
      if (kstat->c_count_timed[max_index] > 0)
         avgTime = kstat->ccall_total_time[max_index] / (double)kstat->c_count_timed[max_index];
      aprintf("%3i. %-23s %-12i %4.3f\n", count + 1, c_name, ignore_val, avgTime);
	}
}

void AdminShowMessage(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])                      
{
	class_node *c,*found_class;
	message_node *m;
	char *bkod_ptr;
	int i,num_parms;
	int parm_id;
	val_type parm_init_value;
	
	char *class_str;
	char *message_str;
	class_str = (char *)parms[0];
	message_str = (char *)parms[1];
	
	c = GetClassByName(class_str);
	if (c == NULL)
	{
		c = GetClassByID( atol( class_str ) );
		if (c == NULL)
		{
			aprintf("Cannot find class %s.\n",class_str);
			return;
		}
	}

	m = GetMessageByName(c->class_id,message_str,&found_class);
	if (m == NULL)
	{
		m = GetMessageByID(c->class_id,atol( message_str ),&found_class);
		if (m == NULL)
		{
			aprintf("Cannot find message %s in CLASS %i.\n",message_str,c->class_id);
			return;
		}
	}
	
	aprintf("--------------------------------------------------------------\n");
	aprintf("CLASS %i : %s\n",c->class_id,c->class_name);
	aprintf("MESSAGE %i : %s",m->message_id,GetNameByID(m->message_id));
	if (c != found_class)
		aprintf(" (handled by CLASS %s)",found_class->class_name);
	aprintf("\n");
   aprintf("Called count (total): %i\n", m->timed_call_count + m->untimed_call_count);
   if (m->timed_call_count > 0)
      aprintf("Average running time: %8.3f\n", m->total_call_time / (double)m->timed_call_count);
   if (m->untimed_call_count > 0)
      aprintf("Called count (untimed): %i\n", m->untimed_call_count);
	aprintf("--------------------------------------------------------------\n");
	aprintf("  Parameters:\n");
	/* we need to read the first few bytes from the bkod to get the parms */
	bkod_ptr = m->handler;
	/* # of local vars at this byte */
	bkod_ptr++;
	num_parms = *bkod_ptr;
	bkod_ptr++;
	
	for (i=0;i<num_parms;i++)
	{
		parm_id = *((int *)bkod_ptr);
		bkod_ptr += 4;
		parm_init_value.int_val = *((int *)bkod_ptr);
		bkod_ptr += 4;
		
		aprintf("  %-20s %s %s\n",GetNameByID(parm_id),GetTagName(parm_init_value),
			GetDataName(parm_init_value));
	}
	
	if (num_parms == 0)
		aprintf("  (none)\n");
	
	aprintf("--------------------------------------------------------------\n");
	if (m->dstr_id != INVALID_DSTR)
		aprintf("%s\n",GetClassDebugStr(found_class,m->dstr_id));
	else
		aprintf("No description\n");
	aprintf("--------------------------------------------------------------\n");
}

void AdminShowClass(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])                    
{
	int i;
	class_node *c;
	message_node *m;
	char *classvar_name;
	char buf[200];
	
	char *class_str;
	class_str = (char *)parms[0];
	
	c = GetClassByName(class_str);
	if (c == NULL)
	{
		c = GetClassByID(atoi( class_str ) );
		if(c == NULL ) {
			aprintf("Cannot find class %s.\n",class_str);
			return;
		}
	}
	
	if (c->super_ptr == NULL)
		aprintf(":< CLASS %s (%i) %s\n",c->class_name,c->class_id,c->fname);
	else
		aprintf(":< CLASS %s (%i) %s is %s\n",c->class_name,c->class_id,c->fname,c->super_ptr->class_name);

	for (i=0;i<c->num_vars;i++)
	{
		classvar_name = GetClassVarNameByID(c,i);
		if (classvar_name == NULL)
			sprintf(buf,": VAR #%-19i",i);
		else
			sprintf(buf,": VAR %-20s",classvar_name);
		aprintf("%s = %s %s\n",buf,GetTagName(c->vars[i].val),
				  GetDataName(c->vars[i].val));
	}

   if (c->num_messages)
   {
      for (i = 0; i < MESSAGE_TABLE_SIZE; i++)
      {
         m = c->messages[i];
         while (m != NULL)
         {
            aprintf(": MSG %s\n", GetNameByID(m->message_id));
            m = m->next;
         }
      }
   }

   aprintf(":>\n");
}

void AdminShowInstances(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	int i, m;
	class_node *c;
	extern object_node* objects;
	extern int num_objects;
	
	char *class_str;
	class_str = (char *)parms[0];
	
	c = GetClassByName(class_str);
	if (c == NULL)
	{
		aprintf("Cannot find CLASS %s.\n",class_str);
		return;
	}
	
	aprintf(":< instances of CLASS %s (%i)\n:",c->class_name,c->class_id);
	m = 0;
	for (i = 0; i < num_objects; i++)
	{
		class_node* wc = GetClassByID(objects[i].class_id);
		while (wc)
		{
			if (wc == c)
			{
				aprintf(" OBJECT %i", i);
				m++;
				break;
			}
			wc = wc->super_ptr;
		}
	}
	aprintf("\n: %i total", m);
	aprintf("\n:>\n");
}

void AdminShowMatches(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])
{
	int i, m;
	class_node *c;
	val_type match;
	char* class_str;
	char* property_str;
	char* relation_str;
	char* tag_str;
	char* data_str;
	char* walk;
	int tag_int;
	int data_int;
	int property_id;
	extern object_node* objects;
	extern int num_objects;
	enum { none=0,isequal=1,isgreater=2,isless=4,sametag=8,difftag=16 };
	int matchtype;

	// For returning a list of objects
	int list_id = INVALID_ID;
	val_type first_val, rest_val;
	rest_val.int_val = NIL;

	class_str = (char *)parms[0];
	property_str = (char *)parms[1];
	relation_str = (char *)parms[2];
	tag_str = (char *)parms[3];
	data_str = (char *)parms[4];
	
	c = GetClassByName(class_str);
	if (c == NULL)
	{
		aprintf("Cannot find CLASS %s.\n",class_str);
		return;
	}
	
	property_id = GetPropertyIDByName(c,property_str);
	if (property_id == INVALID_PROPERTY)
	{
		aprintf("Property %s doesn't exist (at least for CLASS %s (%i)).\n",
			property_str,c->class_name,c->class_id);
		return;
	}
	
	matchtype = 0;
	walk = relation_str;
	while (walk && *walk)
	{
		switch (*walk++)
		{
		case '<': matchtype |= isless | sametag; break;
		case '=': matchtype |= isequal | sametag; break;
		case '>': matchtype |= isgreater | sametag; break;
		case '*': matchtype |= isless | isequal | isgreater | sametag; break;
		case '#': matchtype |= isless | isequal | isgreater | difftag; break;
		default:
			aprintf("Relationship not recognized.  Use <, =, >, *, #, or combination.\n");
			return;
		}
	}
	
	tag_int = GetTagNum(tag_str);
	if (tag_int == INVALID_TAG)
	{
		aprintf("'%s' is not a tag.\n",tag_str);
		return;
	}
	
	if (LookupAdminConstant(data_str,&data_int) == False)
	{
		bool negate = false;
		
		// INT properties may have a negative number and still be legal
		if (tag_int == TAG_INT && *data_str == '-')
		{
			negate = true;
			data_str++;
		}
		
		data_int = GetDataNum(tag_int,data_str);
		if (data_int == INVALID_DATA)
		{
			aprintf("'%s' is not valid data.\n",data_str);
			return;
		}
		
		if (negate)
			data_int = -data_int;
	}
	
	match.v.tag = tag_int;
	match.v.data = data_int;
	
	aprintf(":< matching instances of CLASS %s (%i) %s %s %s %s\n",
		c->class_name,c->class_id, property_str, relation_str,
		GetTagName(match), GetDataName(match));

	// Limit size of the return list.
	int max_list_size = ConfigInt(BLAKOD_MATCHES_LIST_MAX);

	// Store everything in a buffer, flush to aprintf every 9000 chars.
	// Necessary because this command can potentially list 100000s of objects.
	char admin_buf[BUFFER_SIZE];
	char *buf_ptr = admin_buf;
	int len_admin_buf = 0, num_chars = 0;

	m = 0; // Number of objects returned.
	double startTime = GetMicroCountDouble();
	for (i = 0; i < num_objects; i++)
	{
		class_node* wc = GetClassByID(objects[i].class_id);
		class_node* thisc = wc;
		while (wc)
		{
			if (wc == c)
			{
				val_type thisv;
				int thismatch;
				
#if 0
				property_id = GetPropertyIDByName(thisc,property_str);
				if (property_id == INVALID_PROPERTY)
				{
					aprintf("Property %s doesn't exist (at least for CLASS %s (%i)).\n",
						property_str,thisc->class_name,thisc->class_id);
					break;
				}
#endif
				
				// This object's property's value.
				thisv = objects[i].p[property_id].val;
				
				// Compare it to the match value.
				thismatch = 0;
				if (thisv.v.tag == match.v.tag)
					thismatch |= sametag;
				else
					thismatch |= difftag;
				if (thisv.v.tag == TAG_INT)
				{
					if ((int)thisv.v.data > (int)match.v.data) thismatch |= isgreater;
					if ((int)thisv.v.data < (int)match.v.data) thismatch |= isless;
				}
				else
				{
					if ((unsigned int)thisv.v.data > (unsigned int)match.v.data) thismatch |= isgreater;
					if ((unsigned int)thisv.v.data < (unsigned int)match.v.data) thismatch |= isless;
				}
				if (thisv.v.data == match.v.data) thismatch |= isequal;
				
				// If it compares favorably according to the relationship requested, show it.
				if ((thismatch &  (sametag|difftag)) == (matchtype &  (sametag|difftag)) &&
					(thismatch & ~(sametag|difftag)) &  (matchtype & ~(sametag|difftag)))
				{
					if (m < max_list_size)
					{
						first_val.v.tag = TAG_OBJECT;
						first_val.v.data = i;
						list_id = Cons(first_val, rest_val);
						rest_val.v.tag = TAG_LIST;
						rest_val.v.data = list_id;
					}
					num_chars = sprintf(buf_ptr, ": OBJECT %i CLASS %s (%i) %s = %s %s\n",
										i, thisc->class_name, thisc->class_id, property_str,
										GetTagName(thisv), GetDataName(thisv));
					len_admin_buf += num_chars;
					buf_ptr += num_chars;
					if (len_admin_buf > BUFFER_SIZE * 0.9)
					{
						buf_ptr -= len_admin_buf;
						aprintf(buf_ptr);
						len_admin_buf = 0;
					}
					m++;
				}
				
				break;
			}
			wc = wc->super_ptr;
		}
	}
	if (len_admin_buf)
	{
		buf_ptr -= len_admin_buf;
		aprintf(buf_ptr);
	}
	aprintf(": %i total\n", m);
	if (m)
		aprintf(": Objects list: LIST %i\n",list_id);
	aprintf(":>\n");
	aprintf("Time to run: %.3f microseconds\n", GetMicroCountDouble() - startTime);
}

void AdminShowPackages(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	aprintf("%-35s %-4s %-9s\n","Filename","Type","Date/Time/Sequence");
	ForEachDLlist(AdminShowOnePackage);
}

void AdminShowOnePackage(dllist_node *dl)
{
	aprintf("%-35s %-4i %-9i\n",dl->fname,dl->file_type,dl->last_mod_time);
}

void AdminShowConstant(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])                       
{
	int value;
	
	char *name;
	name = (char *)parms[0];
	
	if (LookupAdminConstant(name,&value))
		aprintf("%s = %i\n",name,value);
	else
		aprintf("There is no value for %s\n",name);
}

void AdminShowTransmitted(int session_id,admin_parm_type parms[],
                          int num_blak_parm,parm_node blak_parm[])
{
	aprintf("In most recent transmission period, server has transmitted %i bytes.\n",
		GetTransmittedBytes());
}

void AdminShowTable(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])                    
{
	table_node *tn;
	hash_node *hn;
	int i;
	
	int table_id;
	table_id = (int)parms[0];
	
	tn = GetTableByID(table_id);
	if (tn == NULL)
	{
		aprintf("Cannot find table %i.\n",table_id);
		return;
	}
   if (tn->table == NULL)
   {
      aprintf("Table %i is empty.\n", table_id);
      return;
   }
	aprintf("Table %i (size %i)\n",table_id,tn->size);
	aprintf("----------------------------------------------------------------------\n");
	for (i=0;i<tn->size;i++)
	{
		hn = tn->table[i];
		if (hn != NULL)
		{
			aprintf("hash %5i : ",i);
			while (hn != NULL)
			{
				aprintf("(key %s %s ",GetTagName(hn->key_val),GetDataName(hn->key_val));
				aprintf("val %s %s) ",GetTagName(hn->data_val),GetDataName(hn->data_val));
				hn = hn->next;
			}
			aprintf("\n");
		}
	}
	   
}

int nameid_count;
void AdminShowNameIDs(int session_id, admin_parm_type parms[],
                      int num_blak_parm, parm_node blak_parm[])
{
   nameid_count = 0;
   ForEachNameID(AdminPrintNameID);
   aprintf("Number of name IDs is %i.\n", nameid_count);
}

void AdminPrintNameID(nameid_node *n)
{
   if (n)
   {
      aprintf("ID: %-5i Name: %s\n", n->id, n->name);
      ++nameid_count;
   }
}

void AdminShowName(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])                   
{
	char *username = (char *)parms[0];
	user_node * u = GetUserByName(username);
	
	if (!u)
	{
		aprintf("Cannot find user with name %s.\n",username);
		return;
	}
	
	aprintf(":< object %i\n",u->object_id);
	aprintf(":>\n");
}

static val_type admin_show_references_value;
static const char *admin_show_references_tag_str;
static const char *admin_show_references_data_str;
static int admin_show_references_count;
void AdminShowReferences(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	int tag_int,data_int;
	
	admin_show_references_tag_str = (char *)parms[0];
	admin_show_references_data_str = (char *)parms[1];
	
	tag_int = GetTagNum(admin_show_references_tag_str);
	if (tag_int == INVALID_TAG)
	{
		aprintf("'%s' is not a tag.\n",admin_show_references_tag_str);
		return;
	}
	if (tag_int == TAG_NIL)
	{
		/* Showing thousands of instances of NIL would be overwhelming. */
		aprintf("Cannot show references to NIL.\n");
		return;
	}
	
	if (LookupAdminConstant(admin_show_references_data_str,&data_int) == False)
	{
		bool negate = false;
		
		// INT properties may have a negative number and still be legal
		if (tag_int == TAG_INT && *admin_show_references_data_str == '-')
		{
			negate = true;
			admin_show_references_data_str++;
		}
		
		data_int = GetDataNum(tag_int,admin_show_references_data_str);
		if (data_int == INVALID_DATA)
		{
			aprintf("'%s' is not valid data.\n",admin_show_references_data_str);
			return;
		}
		
		if (negate)
			data_int = -data_int;
	}
	
	admin_show_references_value.v.tag = tag_int;
	admin_show_references_value.v.data = data_int;
	
	admin_show_references_tag_str = GetTagName(admin_show_references_value);
	admin_show_references_data_str = GetDataName(admin_show_references_value);
	
	admin_show_references_count = 0;
	
	aprintf(":< references to %s %s\n",admin_show_references_tag_str,admin_show_references_data_str);
	
	ForEachObject(AdminShowReferencesEachObject);
	
	aprintf(":>\n");
}

static int admin_show_references_current_object;
static const char * admin_show_references_current_prop;
static class_node* admin_show_references_current_class;
void AdminShowReferencesEachObject(object_node *o)
{
	int i;
	
	admin_show_references_current_class = GetClassByID(o->class_id);
	admin_show_references_current_object = o->object_id;
	
	for (i=0;i<o->num_props;i++)
	{
		if (i == 0)
			admin_show_references_current_prop = "self";
		else
			admin_show_references_current_prop =
			GetPropertyNameByID(admin_show_references_current_class,o->p[i].id);
		
		if (o->p[i].val.int_val == admin_show_references_value.int_val)
		{
			admin_show_references_count++;
			aprintf(": OBJECT %i CLASS %s %s = %s %s\n",
				admin_show_references_current_object,
				admin_show_references_current_class->class_name,
				admin_show_references_current_prop,
				admin_show_references_tag_str,
				admin_show_references_data_str);
		}
		else if (o->p[i].val.v.tag == TAG_LIST)
		{
			AdminShowReferencesEachList(o->p[i].val.v.data, -1);
		}
      else if (o->p[i].val.v.tag == TAG_TABLE)
      {
         AdminShowReferencesEachTable(o->p[i].val.v.data, -1);
      }
	}
}

void AdminShowReferencesEachList(int list_id, int parent_id)
{
	list_node *l;

   if (list_id == parent_id)
   {
      eprintf("AdminShowReferencesEachList found self-referencing list %i inside container %i!\n",
         list_id, parent_id);
      return;
   }

	for(;;)
	{
		l = GetListNodeByID(list_id);
		if (l == NULL)
		{
			eprintf("AdminShowReferencesEachList can't get LIST %i\n",list_id);
			return;
		}
		
		if (l->first.int_val == admin_show_references_value.int_val)
			aprintf(": OBJECT %i CLASS %s %s = LIST containing %s %s\n",
			admin_show_references_current_object,
			admin_show_references_current_class->class_name,
			admin_show_references_current_prop,
			admin_show_references_tag_str,
			admin_show_references_data_str);
		
		if (l->first.v.tag == TAG_LIST)
			AdminShowReferencesEachList(l->first.v.data, list_id);
      else if (l->first.v.tag == TAG_TABLE && l->first.v.data != parent_id)
         AdminShowReferencesEachTable(l->first.v.data, list_id);
		if (l->rest.v.tag != TAG_LIST)
			break;
		
		list_id = l->rest.v.data;
	}
}

void AdminShowReferencesEachTable(int table_id, int parent_id)
{
   table_node *t;
   hash_node *hn;

   if (table_id == parent_id)
   {
      eprintf("AdminShowReferencesEachTable found self-referencing table %i inside container %i!\n",
         table_id, parent_id);
      return;
   }

   t = GetTableByID(table_id);
   if (t == NULL)
   {
      eprintf("AdminShowReferencesEachTable can't get TABLE %i\n", table_id);
      return;
   }
   for (int i = 0; i < t->size; ++i)
   {
      hn = t->table[i];
      while (hn != NULL)
      {
         if (hn->data_val.int_val == admin_show_references_value.int_val)
            aprintf(": OBJECT %i CLASS %s %s = TABLE containing %s %s\n",
            admin_show_references_current_object,
            admin_show_references_current_class->class_name,
            admin_show_references_current_prop,
            admin_show_references_tag_str,
            admin_show_references_data_str);
         if (hn->data_val.v.tag == TAG_LIST && hn->data_val.v.data != parent_id)
            AdminShowReferencesEachList(hn->data_val.v.data, table_id);
         else if (hn->data_val.v.tag == TAG_TABLE)
            AdminShowReferencesEachTable(hn->data_val.v.data, table_id);
         hn = hn->next;
      }
   }
}

/* in parsecli.c */
extern client_table_node *user_table,*system_table,*usercommand_table;

void AdminShowProtocol(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	int i;
	
#define QT (CLIENT_COMMANDS_PER_TABLE/4)
	
	aprintf("Regular commands\n");
	
	for (i=0;i<CLIENT_COMMANDS_PER_TABLE/4;i++)
		aprintf("%3i %10u | %3i %10u | %3i %10u | %3i %10u\n",
		i,user_table[i].call_count,i+QT,user_table[i+QT].call_count,
		i+2*QT,user_table[i+2*QT].call_count,i+3*QT,user_table[i+3*QT].call_count);
	
	AdminSendBufferList();
	
	aprintf("\nSystem commands\n");
	
	for (i=0;i<CLIENT_COMMANDS_PER_TABLE/4;i++)
		aprintf("%3i %10u | %3i %10u | %3i %10u | %3i %10u\n",
		i,system_table[i].call_count,i+QT,system_table[i+QT].call_count,
		i+2*QT,system_table[i+2*QT].call_count,i+3*QT,system_table[i+3*QT].call_count);
	
	AdminSendBufferList();
	
	aprintf("\nUser commands\n");
	
	for (i=0;i<CLIENT_COMMANDS_PER_TABLE/4;i++)
		aprintf("%3i %10u | %3i %10u | %3i %10u | %3i %10u\n",
		i,usercommand_table[i].call_count,i+QT,usercommand_table[i+QT].call_count,
		i+2*QT,usercommand_table[i+2*QT].call_count,
		i+3*QT,usercommand_table[i+3*QT].call_count);
}

void AdminSetClass(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])                   
{
   // Note that setting a class var is only temporary until the server restarts.
	class_node *c;
	char *class_name, *var_name, *tag_str, *data_str;
   int var_id, tag_int, data_int;
	val_type val;
   
	class_name = (char *)parms[0];
	var_name = (char *) parms[1];
	tag_str = (char *) parms[2];
	data_str = (char *) parms[3];
		
	c = GetClassByName(class_name);
	if (c == NULL)
	{
		aprintf("Cannot find class named %s.\n",class_name);
		return;
	}

   var_id = GetClassVarIDByName(c, var_name);
   if (var_id == INVALID_CLASSVAR)
   {
		aprintf("Cannot find classvar named %s in class %s.\n", var_name, class_name);
		return;
   }

   tag_int = GetTagNum(tag_str);
	if (tag_int == INVALID_TAG)
	{
		aprintf("'%s' is not a tag.\n", tag_str);
		return;
	}

	if (LookupAdminConstant(data_str,&data_int) == False)
	{
		bool negate = false;
		
		// INT properties may have a negative number and still be legal
		if (tag_int == TAG_INT && *data_str == '-')
		{
			negate = true;
			data_str++;
		}
		
		data_int = GetDataNum(tag_int,data_str);
		if (data_int == INVALID_DATA)
		{
			aprintf("'%s' is not valid data.\n",data_str);
			return;
		}
		
		if (negate)
			data_int = -data_int;
	}
   
   val.v.tag = tag_int;
	val.v.data = data_int;

   c->vars[var_id].val = val;
}

void AdminSetObjInt(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   if (parms[0] >= 0 && parms[0] <= MAX_BUILTIN_OBJECT)
   {
      parms[0] = GetBuiltInObjectID((int)parms[0]);
      AdminSetObject(session_id, parms, num_blak_parm, blak_parm);
   }
   else
   {
      aprintf("Can't reference object with int %i.\n", (int)parms[0]);
   }
}

void AdminSetObject(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
	object_node *o;
	class_node *c;
	int property_id,tag_int,data_int;
	val_type val;
	int object_id;
	char *property_str,*tag_str,*data_str;
	
	object_id = (int)parms[0];
	property_str = (char *)parms[1];
	tag_str = (char *)parms[2];
	data_str = (char *)parms[3];
	
	o = GetObjectByID(object_id);
	if (o == NULL)
	{
		aprintf("Invalid object id %i (or it has been deleted).\n",
			object_id);
		return;
	}
	
	c = GetClassByID(o->class_id);
	if (c == NULL) /* can't ever be */
	{
		aprintf("OBJECT %i has invalid class!\n",o->object_id);
		return;
	}
	
	property_id = GetPropertyIDByName(c,property_str);
	if (property_id == INVALID_PROPERTY)
	{
		aprintf("Property %s doesn't exist (at least for CLASS %s (%i)).\n",
			property_str,c->class_name,c->class_id);
		return;
	}
	
	tag_int = GetTagNum(tag_str);
	if (tag_int == INVALID_TAG)
	{
		aprintf("'%s' is not a tag.\n",tag_str);
		return;
	}
	
	if (LookupAdminConstant(data_str,&data_int) == False)
	{
		bool negate = false;
		
		// INT properties may have a negative number and still be legal
		if (tag_int == TAG_INT && *data_str == '-')
		{
			negate = true;
			data_str++;
		}
		
		data_int = GetDataNum(tag_int,data_str);
		if (data_int == INVALID_DATA)
		{
			aprintf("'%s' is not valid data.\n",data_str);
			return;
		}
		
		if (negate)
			data_int = -data_int;
	}
	
	val.v.tag = tag_int;
	val.v.data = data_int;
	
	o->p[property_id].val = val;
}

void AdminSetAccountName(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	account_node *a;
	
	int account_id;
	char *name;
	account_id = (int)parms[0];
	name = (char *)parms[1];
	if (!name || !*name)
	{
		aprintf("Missing parameter 2: new account name.\n");
		return;
	}
	
	if (strchr(name, ':'))
	{
		aprintf("Account names cannot contain the character ':'.\n");
		return;
	}
	
	a = GetAccountByID(account_id);
	if (a == NULL)
	{
		aprintf("Cannot find account %i.\n",account_id);
		return;
	}
	lprintf("AdminSetAccountName changing name of account %i from %s to %s\n",
		a->account_id,a->name,name);
	aprintf("Changing name of account %i from '%s' to '%s'.\n",
		a->account_id,a->name,name);
	SetAccountName(a,name);
}

void AdminSetAccountPassword(int session_id,admin_parm_type parms[],
                             int num_blak_parm,parm_node blak_parm[])                             
{
	account_node *a;
	
	int account_id;
	char *password;
	account_id = (int)parms[0];
	password = (char *)parms[1];
	
	a = GetAccountByID(account_id);
	if (a == NULL)
	{
		aprintf("Cannot find account %i.\n",account_id);
		return;
	}
	lprintf("AdminSetAccountPassword setting password for %s\n",a->name);
	SetAccountPassword(a,password);
	
	aprintf("Set password for account %i (%s).\n",a->account_id,a->name);
}

void AdminSetAccountEmail(int session_id, admin_parm_type parms[],
                          int num_blak_parm, parm_node blak_parm[])
{
   account_node *a;

   int account_id;
   char *email;
   account_id = (int)parms[0];
   email = (char *)parms[1];
   if (!email || !*email)
   {
      aprintf("Missing parameter 2: new account email.\n");
      return;
   }

   if (!AccountValidateEmail(email))
   {
      aprintf("Invalid email address.\n");

      return;
   }

   a = GetAccountByID(account_id);
   if (a == NULL)
   {
      aprintf("Cannot find account %i.\n", account_id);

      return;
   }

   lprintf("AdminSetAccountEmail changing name of account %i from %s to %s\n",
      a->account_id, a->email, email);
   aprintf("Changing email of account %i from '%s' to '%s'.\n",
      a->account_id, a->email, email);
   SetAccountEmail(a, email);
}

void AdminSetAccountType(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   account_node *a;
   int account_id, type;

   account_id = (int)parms[0];
   type = (int)parms[1];

   // Type must be valid.
   if (type < ACCOUNT_NORMAL || type > ACCOUNT_DM)
   {
      aprintf("Invalid account type specified.\n");
      return;
   }

   a = GetAccountByID(account_id);
   if (a == NULL)
   {
      aprintf("Cannot find account %i.\n", account_id);

      return;
   }

   lprintf("AdminSetAccountType changing type of account %i from %i to %i\n",
      a->account_id, a->type, type);
   aprintf("Changing type of account %i from '%i' to '%i'.\n",
      a->account_id, a->type, type);
   SetAccountType(a, type);
}

void AdminSetAccountObject(int session_id,admin_parm_type parms[],
                           int num_blak_parm,parm_node blak_parm[])
{
	int account_id,object_id;
	user_node *u;
	
	account_id = (int)parms[0];
	object_id = (int)parms[1];
	
	if (GetObjectByID(object_id) == NULL)
	{
		aprintf("Object %i does not exist.\n",object_id);
		return;
	}
	
	if (GetAccountByID(account_id) == NULL)
	{
		aprintf("Cannot find account %i.\n",account_id);
		return;
	}
	
	u = GetUserByObjectID(object_id);
	if (u == NULL)
	{
		aprintf("warning: Object %i is not a known user object; results may not be what you expect.\n",object_id);
	}
	else
	{
		aprintf("Removing user object %i from the old account %i.\n", u->object_id, u->account_id);
		DeleteUserByObjectID(u->object_id); /* deletes user node but not object */
		u = NULL;
	}
	
	if (AssociateUser(account_id,object_id) == False) /* creates a user node for it */
	{
		aprintf("Error assocating new account and object.\n");
	}
	else
	{
		aprintf("Associated account %i with object %i as a user.\n", account_id, object_id);
	}
}

/*
void AdminSetResource(int session_id,admin_parm_type parms[])
{
resource_node *r;

  int rsc_id;
  char *str_value;
  rsc_id = (int)parms[0];
  str_value = (char *)parms[1];
  
	r = GetResourceByID(rsc_id);
	if (r == NULL)
	{
	aprintf("Resource ID %i does not exist.\n",rsc_id);
	return;
	}
	
	  if (r->resource_id < MIN_DYNAMIC_RSC)
	  {
      aprintf("Resource %i is not a dynamic resource.\n",
	  r->resource_id);
      return;
	  }
	  
		ChangeDynamicResourceStr(r,str_value);
		
		  AdminPrintResource(r);
		  }
*/
void AdminSetConfigInt(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])                       
{
	int config_id;
	config_node *c;
	
	char *group;
	char *name;
	int new_value;
	group = (char *)parms[0];
	name = (char *)parms[1];
	new_value = (int)parms[2];
	
	config_id = GetConfigIDByGroupAndName(group,name);
	if (config_id == INVALID_CONFIG)
	{
		aprintf("Unable to find configure group %s name %s.\n",group,name);
		return;
	}
	
	c = GetConfigByID(config_id);
	if (c == NULL)
	{
		aprintf("Bizarre--got config id %i, but no config node exists.\n",config_id);
		return;
	}
	
	if (c->is_dynamic == False)
	{
		aprintf("This configure option is not settable at runtime.\n");
		return;
	}
	
	if (c->config_type != CONFIG_INT)
	{
		aprintf("This configure option is not an integer.\n");
		return;
	}
	
	
	SetConfigInt(c->config_id,new_value);
	
	aprintf("Configure option group %s name %s is now set to %i.\n",
		group,name,new_value);
	
}

void AdminSetConfigBool(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	int config_id;
	config_node *c;
	
	char *group;
	char *name;
	char *new_value;
	group = (char *)parms[0];
	name = (char *)parms[1];
	new_value = (char *)parms[2];
	
	config_id = GetConfigIDByGroupAndName(group,name);
	if (config_id == INVALID_CONFIG)
	{
		aprintf("Unable to find configure group %s name %s.\n",group,name);
		return;
	}
	
	c = GetConfigByID(config_id);
	if (c == NULL)
	{
		aprintf("Bizarre--got config id %i, but no config node exists.\n",config_id);
		return;
	}
	
	if (c->is_dynamic == False)
	{
		aprintf("This configure option is not settable at runtime.\n");
		return;
	}
	
	if (c->config_type != CONFIG_BOOL)
	{
		aprintf("This configure option is not boolean.\n");
		return;
	}
	
	
	if (stricmp(new_value,"YES") != 0 && stricmp(new_value,"NO") != 0)
	{
		aprintf("Boolean configuration options must be 'yes' or 'no'\n");
		return;
	}
	
   if (stricmp(new_value, "YES") == 0)
   {
      SetConfigBool(c->config_id, True);
      // Start timing if we set the debug time calls boolean to true.
      if (c->config_id == DEBUG_TIME_CALLS)
         InitTimeProfiling();
   }
   else
   {
      SetConfigBool(c->config_id, False);
      // Stop timing if we set the debug time calls boolean to true.
      if (c->config_id == DEBUG_TIME_CALLS)
         EndTimeProfiling();
   }

	aprintf("Configure option group %s name %s is now set to '%s'.\n",
		group,name,new_value);
}

void AdminSetConfigStr(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])                       
{
	int config_id;
	config_node *c;
	
	char *group;
	char *name;
	char *new_value;
	group = (char *)parms[0];
	name = (char *)parms[1];
	new_value = (char *)parms[2];
	
	config_id = GetConfigIDByGroupAndName(group,name);
	if (config_id == INVALID_CONFIG)
	{
		aprintf("Unable to find configure group %s name %s.\n",group,name);
		return;
	}
	
	c = GetConfigByID(config_id);
	if (c == NULL)
	{
		aprintf("Bizarre--got config id %i, but no config node exists.\n",config_id);
		return;
	}
	
	if (c->is_dynamic == False)
	{
		aprintf("This configure option is not settable at runtime.\n");
		return;
	}
	
	if (c->config_type != CONFIG_STR)
	{
		aprintf("This configure option is not a string.\n");
		return;
	}
	
	
	SetConfigStr(c->config_id,new_value);
	
	aprintf("Configure option group %s name %s is now set to %s.\n",
		group,name,new_value);
	
}

void AdminSuspendUser(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])                      
{
	user_node *u;
	account_node *a;
	int hours;
	int id;
	char *arg_str;
	Bool is_by_number;
	//char *ptr;
	
	hours = (int)parms[0];
	
	arg_str = (char *)parms[1];
	if (!arg_str || !*arg_str)
	{
		aprintf("Missing parameter 2: user name or number.\n");
		return;
	}
	
	is_by_number = False;
	//   is_by_number = True;
	
	//   ptr = arg_str;
	//   while (*ptr != 0)
	//   {
	//      if (*ptr < '0' || *ptr > '9')
	//	 is_by_number = False;
	//      ptr++;
	//   }
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		u = GetUserByObjectID(id);
	}
	else
	{
		u = GetUserByName(arg_str);
	}
	
	if (u == NULL)
	{
		aprintf("Cannot find user %s.\n",arg_str);
		return;
	}
	
	a = GetAccountByID(u->account_id);
	
	if (a == NULL)
	{
		aprintf("Cannot find account for user %s.\n",arg_str);
		return;
	}
	
	if (!SuspendAccountRelative(a, hours))
	{
		aprintf("Suspension of account %i (%s) failed.\n",
			a->account_id, a->name);
		return;
	}
	
	if (a->suspend_time <= 0)
	{
		aprintf("Account %i (%s) is unsuspended.\n",
			a->account_id, a->name);
	}
	else
	{
		aprintf("Account %i (%s) is suspended until %s.\n",
			a->account_id, a->name, TimeStr(a->suspend_time));
	}
}

void AdminSuspendAccount(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	account_node *a;
	int hours;
	int id;
	char *arg_str;
	Bool is_by_number;
	char *ptr;
	
	hours = (int)parms[0];
	
	arg_str = (char *)parms[1];
	if (!arg_str || !*arg_str)
	{
		aprintf("Missing parameter 2: account name or number.\n");
		return;
	}
	
   if (hours < 1)
   {
      aprintf("Got num hours smaller than 1, please enter a valid number (e.g. 1-150000)\n");
      return;
   }

	is_by_number = True;
	
	ptr = arg_str;
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_by_number = False;
		ptr++;
	}
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		a = GetAccountByID(id);
	}
	else
	{
		a = GetAccountByName(arg_str);
	}
	
	if (a == NULL)
	{
		aprintf("Cannot find account %s.\n",arg_str);
		return;
	}
	
	if (!SuspendAccountRelative(a, hours))
	{
		aprintf("Suspension of account %i (%s) failed.\n",
			a->account_id, a->name);
		return;
	}
	
	if (a->suspend_time <= 0)
	{
		aprintf("Account %i (%s) is unsuspended.\n",
			a->account_id, a->name);
	}
	else
	{
		aprintf("Account %i (%s) is suspended until %s.\n",
			a->account_id, a->name, TimeStr(a->suspend_time));
	}
}

void AdminUnsuspendUser(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	user_node *u;
	account_node *a;
	int id;
	char *arg_str;
	Bool is_by_number;
	//char *ptr;
	
	arg_str = (char *)parms[0];
	if (!arg_str || !*arg_str)
	{
		aprintf("Missing parameter 1: user name or number.\n");
		return;
	}
	
	is_by_number = False;
	//   is_by_number = True;
	
	//   ptr = arg_str;
	//   while (*ptr != 0)
	//   {
	//      if (*ptr < '0' || *ptr > '9')
	//	 is_by_number = False;
	//      ptr++;
	//   }
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		u = GetUserByObjectID(id);
	}
	else
	{
		u = GetUserByName(arg_str);
	}
	
	if (u == NULL)
	{
		aprintf("Cannot find user %s.\n",arg_str);
		return;
	}
	
	a = GetAccountByID(u->account_id);
	
	if (a == NULL)
	{
		aprintf("Cannot find account for user %s.\n",arg_str);
		return;
	}
	
	if (!SuspendAccountAbsolute(a, 0) || a->suspend_time > 0)
	{
		aprintf("Unsuspension of account %i (%s) failed.\n",
			a->account_id, a->name);
		return;
	}
	
	aprintf("Account %i (%s) is unsuspended.\n",
		a->account_id, a->name);
}

void AdminUnsuspendAccount(int session_id,admin_parm_type parms[],
                           int num_blak_parm,parm_node blak_parm[])                           
{
	account_node *a;
	int id;
	char *arg_str;
	Bool is_by_number;
	char *ptr;
	
	arg_str = (char *)parms[0];
	if (!arg_str || !*arg_str)
	{
		aprintf("Missing parameter 1: account name or number.\n");
		return;
	}
	
	is_by_number = True;
	
	ptr = arg_str;
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_by_number = False;
		ptr++;
	}
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		a = GetAccountByID(id);
	}
	else
	{
		a = GetAccountByName(arg_str);
	}
	
	if (a == NULL)
	{
		aprintf("Cannot find account %s.\n",arg_str);
		return;
	}
	
	if (!SuspendAccountAbsolute(a, 0) || a->suspend_time > 0)
	{
		aprintf("Unsuspension of account %i (%s) failed.\n",
			a->account_id, a->name);
		return;
	}
	
	aprintf("Account %i (%s) is unsuspended.\n",
		a->account_id, a->name);
}

void AdminCreateAccount(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])
{
	int account_id;
	
   char *name, *password, *email, *type;
	type = (char *)parms[0];
	name = (char *)parms[1];
	password = (char *)parms[2];
   email = (char *)parms[3];
	
	if (0 == stricmp(type, "AUTO")
      || 0 == stricmp(type, "AUTOMATED"))
	{
		// Tried "CREATE ACCOUNT AUTO blah blah" for the command "CREATE AUTO blah blah".
		AdminCreateAutomated(session_id,&parms[1], 0, NULL);
		return;
	}
	
	if (!name || !*name)
	{
		aprintf("Account name must be at least one character.\n");
		return;
	}
	if (strchr(name, ':'))
	{
		aprintf("Account names cannot contain the character ':'.\n");
		return;
	}
	
	switch (toupper(type[0]))
	{
	case 'A':
		if (CreateAccount(name,password,email,ACCOUNT_ADMIN,&account_id) == False)
		{
			aprintf("Account name %s already exists\n",name);
			return;
		}
		break;
		
	case 'D':
      if (CreateAccount(name, password, email,ACCOUNT_DM, &account_id) == False)
		{
			aprintf("Account name %s already exists\n",name);
			return;
		}
		break;
		
	default :
		if (CreateAccount(name,password,email,ACCOUNT_NORMAL,&account_id) == False)
		{
			aprintf("Account name %s already exists\n",name);
			return;
		}
		
	}
	
	aprintf("Created ACCOUNT %i.\n",account_id);
}

void AdminCreateAutomated(int session_id,admin_parm_type parms[],
                          int num_blak_parm,parm_node blak_parm[])
{
   /* create account and num_slots users for it */
   int num_slots;
   int account_id;
   user_node *u;

   char *name,*password,*email;

   name = (char *)parms[0];
   password = (char *)parms[1];
   email = (char *)parms[2];

   if (CreateAccount(name,password,email,ACCOUNT_NORMAL,&account_id) == False)
   {
      aprintf("Account name %s already exists\n",name);

      return;
   }

   num_slots = ConfigInt(ACCOUNT_NUM_SLOTS);

   // Put an upper limit on number of slots
   if (num_slots > 10)
      num_slots = 10;

   // Automated, so don't display users.
   for (int i = 0; i < num_slots; ++i)
      u = CreateNewUser(account_id, USER_CLASS);

   aprintf("Created account %i.\n", account_id);
}

void AdminRecreateAutomated(int session_id,admin_parm_type parms[],
                            int num_blak_parm,parm_node blak_parm[])                            
{
	/* create account and 1 user for it */
	
	int account_id, acct;
	user_node *u;
	
	char *name,*password,*email;
	
	account_id = (int)parms[0];
	name = (char *)parms[1];
	password = (char *)parms[2];
   email = (char *)parms[3];
	
	acct = RecreateAccountSecurePassword(account_id,name,password,email,ACCOUNT_NORMAL);
	if (acct >= 0)
	{
		aprintf("Created account %i.\n",acct);
	}
	else
	{
		aprintf("Cannot recreate account %i; may already exist.\n", account_id);
		return;
	}
	
	u = CreateNewUser(account_id,USER_CLASS);
	if (u == NULL)
	{
		aprintf("Cannot find just created user for account %i!\n",
			account_id);
		return;
	}
	AdminShowOneUser(u);
	
}

// Prints out the highest timed blakod message and resets the values.
void AdminResetHighestTimed(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   kod_statistics *kod_stat = GetKodStats();
   object_node *o = NULL;
   class_node *c = NULL;
   char *m = NULL;

   aprintf("Longest time on one top level message is %i milliseconds\n",
      (int)(kod_stat->interpreting_time_highest / 1000.0));
   if (kod_stat->interpreting_time_object_id != INVALID_ID)
   {
      o = GetObjectByID(kod_stat->interpreting_time_object_id);
      c = o ? GetClassByID(o->class_id) : NULL;
   }
   m = GetNameByID(kod_stat->interpreting_time_message_id);
   aprintf("Most recent slow top level message is:\nOBJECT %i CLASS %s MESSAGE %s\n",
      kod_stat->interpreting_time_object_id,
      (char*)(c ? c->class_name : "(unknown)"),
      (char*)(m ? m : "(unknown)"));
   aprintf("Most recent slow top level message includes %i posted followups\n",
      kod_stat->interpreting_time_posts);

   kod_stat->interpreting_time_highest = 0;
   kod_stat->interpreting_time_message_id = INVALID_ID;
   kod_stat->interpreting_time_object_id = INVALID_ID;
   aprintf("Highest timed message reset.\n");
}

void AdminAddUserToEachAccount(int session_id,admin_parm_type parms[],
							   int num_blak_parm,parm_node blak_parm[])                         
{
	ForEachAccount(CreateUseronAccount);
	aprintf("Added one user to each account.\n");
}

void AdminCreateUser(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	user_node *u;
	
	int account_id;
	account_id = (int)parms[0];
	
	u = CreateNewUser(account_id,USER_CLASS);
	if (u == NULL)
	{
		aprintf("Cannot find just created user for account %i!\n",
			account_id);
		return;
	}
	AdminShowUserHeader();
	AdminShowOneUser(u);
}

void AdminCreateEscapedConvict(int session_id, admin_parm_type parms[],
                              int num_blak_parm, parm_node blak_parm[])
{
   user_node *u;

   int account_id;
   account_id = (int)parms[0];

   u = CreateNewUser(account_id, ESCAPED_CONVICT_CLASS);
   if (u == NULL)
   {
      aprintf("Cannot find just created user for account %i!\n",
         account_id);
      return;
   }
   AdminShowUserHeader();
   AdminShowOneUser(u);
}

void AdminCreateAdmin(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])                      
{
	user_node *u;
	
	int account_id;
	account_id = (int)parms[0];
	
	u = CreateNewUser(account_id,ADMIN_CLASS);
	if (u == NULL)
	{
		aprintf("Cannot find just created user for account %i!\n",
			account_id);
		return;
	}
	AdminShowUserHeader();
	AdminShowOneUser(u);
}

void AdminCreateDM(int session_id,admin_parm_type parms[],
                   int num_blak_parm,parm_node blak_parm[])                   
{
	user_node *u;
	
	int account_id;
	account_id = (int)parms[0];
	
	u = CreateNewUser(account_id,DM_CLASS);
	if (u == NULL)
	{
		aprintf("Cannot find just created user for account %i!\n",
			account_id);
		return;
	}
	AdminShowUserHeader();
	AdminShowOneUser(u);
}


void AdminCreateObject(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	class_node *c;
	int new_object_id;
	val_type system_id_const;
	
	char *class_name;
	class_name = (char *)parms[0];
	
	
	c = GetClassByName(class_name);
	if (c == NULL)
	{
		aprintf("Cannot find class named %s.\n",class_name);
		return;
	}
	
	/* add system_id = the system id for convenience */
	
	system_id_const.v.tag = TAG_OBJECT;
	system_id_const.v.data = GetSystemObjectID();
	
	blak_parm[num_blak_parm].type = CONSTANT;
	blak_parm[num_blak_parm].value = system_id_const.int_val;
	blak_parm[num_blak_parm].name_id = SYSTEM_PARM;
	num_blak_parm++;
	
	new_object_id = CreateObject(c->class_id,num_blak_parm,blak_parm);
	aprintf("Created object %i.\n",new_object_id);
}

void AdminCreateListNode(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	int num,list_id;
	val_type first_val,rest_val;
	
	char *first_tag,*first_data,*rest_tag,*rest_data;
	first_tag = (char *)parms[0];
	first_data = (char *)parms[1];
	rest_tag = (char *)parms[2];
	rest_data = (char *)parms[3];
	
	num = GetTagNum(first_tag);
	if (num == INVALID_TAG)
	{
		aprintf("First tag invalid.\n");
		return;
	}
	first_val.v.tag = num;
	
	num = GetDataNum(first_val.v.tag,first_data);
	if (num == INVALID_DATA)
	{
		aprintf("First data invalid.\n");
		return;
	}
	first_val.v.data = num;
	
	num = GetTagNum(rest_tag);
	if (num == INVALID_TAG)
	{
		aprintf("Rest tag invalid.\n");
		return;
	}
	rest_val.v.tag = num;
	
	num = GetDataNum(rest_val.v.tag,rest_data);
	if (num == INVALID_DATA)
	{
		aprintf("Rest data invalid.\n");
		return;
	}
	rest_val.v.data = num;
	
	list_id = Cons(first_val,rest_val);
	aprintf("Created list node %i.\n",list_id);
	aprintf(":<\n");
	AdminShowListParen(session_id, list_id);
	aprintf(":>\n");
	
}

void AdminCreateTable(int session_id, admin_parm_type parms[],
                      int num_blak_parm, parm_node blak_parm[])
{
   int table_id, table_size;
   table_node *t;

   table_size = (int)parms[0];
   table_id = CreateTable(table_size);
   t = GetTableByID(table_id);
   if (!t)
      aprintf("Couldn't create table %i with size %i!.\n", table_id, table_size);
   else
      aprintf("Created table %i, expected size %i, actual size %i.\n", table_id, table_size, t->size);
}

void AdminCreateTimer(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])
{
	int message_id,timer_id;
	
	int object_id,milliseconds;
	char *message_name;
	object_id = (int)parms[0];
	message_name = (char *)parms[1];
	milliseconds = (int)parms[2];
	
	message_id = GetIDByName(message_name);
	if (message_id == INVALID_ID)
	{
		aprintf("Cannot find message name %s.\n",message_name);
		return;
	}
	
	timer_id = CreateTimer(object_id,message_id,milliseconds);
	aprintf("Created timer %i.\n",timer_id);
}

void AdminCreateResource(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	int rsc_id;
	resource_node *r;
	
	char *resource_value;
	resource_value = (char *)parms[0];
	
	rsc_id = AddDynamicResource(resource_value);
	r = GetResourceByID(rsc_id);
	if (r == NULL)
	{
		aprintf("Error creating resource.\n");
		return;
	}
	AdminPrintResource(r);
   aprintf("WARNING: do not change player names with created resources.\n");
   aprintf("Use send o 0 AdminChangeUserName oUser o obj_num sName q name instead.\n");
   aprintf("Failure to do so will result in player's new name not being added to user table.\n");
}

void AdminDeleteTimer(int session_id,admin_parm_type parms[],
                      int num_blak_parm,parm_node blak_parm[])                      
{
	timer_node *t;
	
	int timer_id;
	timer_id = (int)parms[0];
	
	t = GetTimerByID(timer_id);
	if (t == NULL)
	{
		aprintf("Timer %i does not exist.\n",timer_id);
		return;
	}
	AdminShowOneTimer(t);
	
	DeleteTimer(timer_id);
	aprintf("This timer has been deleted.\n");
	
}

void AdminDeleteAccount(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	account_node *a;
	
	int account_id;
	account_id = (int)parms[0];
	
	a = GetAccountByID(account_id);
	if (a == NULL)
	{
		aprintf("Account %i does not exist.\n",account_id);
		return;
	}
	
	if (a->account_id == admin_session_id)
	{
		aprintf("You can't delete your own account.\n");
		return;
	}
	
	aprintf("Account %i will be deleted.\n",a->account_id);

   // XXX Need a replacement for this on Linux
#ifdef BLAK_PLATFORM_WINDOWS
	PostThreadMessage(main_thread_id,WM_BLAK_MAIN_DELETE_ACCOUNT,0,a->account_id);
#endif
}

void AdminDeleteEachUserObject(user_node *u)
{
	/* delete u->object_id */
	
	val_type ret_val;
	
	ret_val.int_val = SendTopLevelBlakodMessage(u->object_id,DELETE_MSG,0,NULL);
}

static int admin_check_user;
static Bool admin_user_is_logged_in;
void AdminDeleteUser(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])                     
{
	user_node *u;
	
	int object_id;
	object_id = (int)parms[0];
	
	u = GetUserByObjectID(object_id);
	if (u == NULL)
	{
		aprintf("Cannot find user with object id %i.\n",object_id);
		return;
	}
	
	admin_check_user = u->object_id;
	admin_user_is_logged_in = False;
	ForEachSession(AdminCheckUserLoggedOn);
	
	if (admin_user_is_logged_in)
	{
		aprintf("Someone is logged in as that user; you must kick them off before deleting them\n");
		return;
	}
	
	AdminDeleteEachUserObject(u);
	aprintf("Deleting the %i user.\n",DeleteUserByObjectID(u->object_id));
}

void AdminCheckUserLoggedOn(session_node *s)
{
	if (s && s->state == STATE_GAME && s->game && s->game->object_id == admin_check_user)
		admin_user_is_logged_in = True;
}

void AdminSendInt(int session_id,admin_parm_type parms[],
                  int num_blak_parm,parm_node blak_parm[])
{
   if (parms[0] >= 0 && parms[0] <= MAX_BUILTIN_OBJECT)
   {
      parms[0] = GetBuiltInObjectID((int)parms[0]);
      AdminSendObject(session_id, parms, num_blak_parm, blak_parm);
   }
   else
   {
      aprintf("Can't reference object with int %i.\n", (int)parms[0]);
   }
}

void AdminSendList(int session_id, admin_parm_type parms[],
                  int num_blak_parm, parm_node blak_parm[])
{
   int list_id, message_id, temp_list_id;
   const char *message_name;
   list_node *l;

   list_id = (int)parms[0];
   l = GetListNodeByID(list_id);
   if (!l)
   {
      aprintf("AdminSendList couldn't find list_id %i", list_id);
      return;
   }

   message_name = (char *)parms[1];
   message_id = GetIDByName(message_name);
   if (message_id == INVALID_ID)
   {
      aprintf("Cannot find message '%s'.\n", message_name);
      return;
   }

   DoneLoadAccounts();

   // Time this.
   double startTime = GetMicroCountDouble();

   if (l->first.v.tag == TAG_OBJECT)
      SendTopLevelBlakodMessage(l->first.v.data, message_id, num_blak_parm, blak_parm);

   // Reaquire list node in case list node realloc due to SendBlakodMessage
   // clobbered variable. list_id already confirmed valid if we are here.
   l = GetListNodeByID(list_id);

   while (l && l->rest.v.tag != TAG_NIL)
   {
      // Get this list node to replace after call to SendTopLevelBlakodMessage.
      temp_list_id = l->rest.v.data;
      l = GetListNodeByID(l->rest.v.data);
      if (!l)
      {
         aprintf("AdminSendList couldn't find list_id %i", list_id);
         return;
      }
      if (l->first.v.tag == TAG_OBJECT)
         SendTopLevelBlakodMessage(l->first.v.data, message_id, num_blak_parm, blak_parm);
      l = GetListNodeByID(temp_list_id);
   }

   aprintf("AdminSendList completed in %.3f microseconds.\n",
      GetMicroCountDouble() - startTime);
}

void AdminSendObject(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	int message_id;
	val_type blak_val;
	object_node *o;
	message_node *m;
	const char* tag;
	const char* data;
	
	int object_id;
	const char *message_name;
	object_id = (int)parms[0];
	message_name = (char *)parms[1];
	
	DoneLoadAccounts();
	
	/* need to sort blak_parm */
	
	o = GetObjectByID(object_id);
	if (o == NULL)
	{
		aprintf("Invalid object id %i (or it has been deleted).\n",
			object_id);
		return;
	}
	
	message_id = GetIDByName(message_name);
	if (message_id == INVALID_ID)
	{
		aprintf("Cannot find message '%s'.\n",message_name);
		return;
	}
	
	m = GetMessageByID(o->class_id,message_id,NULL);
	if (m == NULL)
	{
		aprintf("OBJECT %i can't handle MESSAGE %i %s.\n",
			object_id,message_id,message_name);
		return;
	}
	data = GetNameByID(m->message_id);
	if (data)
		message_name = data;
	
   // Time the message.
   double startTime = GetMicroCountDouble();

   // Send the message and handle any posted messages spawned, also.
   blak_val.int_val = SendTopLevelBlakodMessage(object_id, message_id, num_blak_parm, blak_parm);

   aprintf("Message %s completed in %.3f microseconds.\n", message_name,
      GetMicroCountDouble() - startTime);

	/* Note that o may be invalid from here if we needed to resize our object array
    * mid-message.  Messages that could create a ton of objects could do that, such
	* as Meridian's RecreateAll() message.
	*/
	
	tag = GetTagName(blak_val);
	data = GetDataName(blak_val);
	aprintf(":< return from OBJECT %i MESSAGE %s (%i)\n", object_id,message_name,m->message_id);
	aprintf(": %s %s\n",(tag? tag : "UNKNOWN"),(data? data : "UNKNOWN"));
	
	if (tag && data)
	{
		
		if (blak_val.v.tag == TAG_STRING || blak_val.v.tag == TAG_TEMP_STRING)
		{
			string_node* snod =
				(blak_val.v.tag == TAG_STRING)?
				GetStringByID(blak_val.v.data) :
			GetTempString();
			int len;
			if (snod && snod->len_data)
			{
			  len = std::min(snod->len_data, 1024);
			  aprintf(":   == \"");
			  AdminBufferSend(snod->data,len);
			  if (len < snod->len_data)
			    aprintf("...");
			  aprintf("\"\n");
			}
		}
		else if (blak_val.v.tag == TAG_RESOURCE)
		{
			resource_node* rnod = GetResourceByID(blak_val.v.data);
			int len;
			if (rnod && rnod->resource_val[0] && *rnod->resource_val[0])
			{
            len = std::min(strlen(rnod->resource_val[0]), (size_t) 60);
			  aprintf(":   == \"");
			  AdminBufferSend(rnod->resource_val[0], len);
			  if (len < (int)strlen(rnod->resource_val[0]))
			    aprintf("...");
			  aprintf("\"\n");
			}
		}
		else if (blak_val.v.tag == TAG_OBJECT)
		{
			object_node* onod = GetObjectByID(blak_val.v.data);
			if (onod)
			{
				class_node* cnod = GetClassByID(onod->class_id);
				aprintf(":   is CLASS %s (%i)\n", (cnod? cnod->class_name : "<invalid>"), (onod->class_id));
			}
		}
		
	}
	
	aprintf(":>\n");
}

void AdminSendUsers(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm_unused[])
{
	val_type str_val;
	parm_node blak_parm[1];
	
	char *text;
	text = (char *)parms[0];
	
	if (!text)
		return;
	SetTempString(text,strlen(text));
	str_val.v.tag = TAG_TEMP_STRING;
	str_val.v.data = 0;		/* doesn't matter for TAG_TEMP_STRING */
	
	blak_parm[0].type = CONSTANT;
	blak_parm[0].value = str_val.int_val;
	blak_parm[0].name_id = STRING_PARM;   
	
	SendTopLevelBlakodMessage(GetSystemObjectID(),SYSTEM_STRING_MSG,1,blak_parm);
	aprintf("Sent to gamers: '%s'.\n",text);
}

void AdminSendClass(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
	int message_id;
	message_node *m;
	class_node *c;
	int executed;
	
	char *class_name,*message_name;
	class_name = (char *)parms[0];
	message_name = (char *)parms[1];
	
	DoneLoadAccounts();
	
	/* need to sort blak_parm */
	
	c = GetClassByName(class_name);
	if (c == NULL)
	{
		aprintf("Cannot find class '%s'.\n",class_name);
		return;
	}
	
	message_id = GetIDByName(message_name);
	if (message_id == INVALID_ID)
	{
		aprintf("Cannot find message '%s'.\n",message_name);
		return;
	}
	
	m = GetMessageByID(c->class_id,message_id,NULL);
	if (m == NULL)
	{
		aprintf("CLASS %i %s cannot handle MESSAGE %i %s.\n",
			c->class_id,c->class_name,message_id,message_name);
		return;
	}   
	message_name = GetNameByID(message_id);
	
	executed = SendBlakodClassMessage(c->class_id,message_id,num_blak_parm,blak_parm);
	
	aprintf(":< %i instance(s) sent MESSAGE %i %s\n:>\n", executed, message_id, message_name);
}

void AdminTraceOnMessage(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	class_node *c;
	int message_id;
	message_node *m;
	
	char *class_name,*message_name;
	class_name = (char *)parms[0];
	message_name = (char *)parms[1];
	
	c = GetClassByName(class_name);
	if (c == NULL)
	{
		aprintf("Cannot find CLASS %s.\n",class_name);
		return;
	}
	
	message_id = GetIDByName(message_name);
	if (message_id == INVALID_ID)
	{
		aprintf("Cannot find MESSAGE %s.\n",message_name);
		return;
	}
	
	message_name = GetNameByID(message_id);
	m = GetMessageByID(c->class_id,message_id,NULL);
	if (m == NULL)
	{
		aprintf("Cannot find MESSAGE %s (%i) for CLASS %s (%i).\n",
			message_name,message_id,c->class_name,c->class_id);
		return;
	}
	
	m->trace_session_id = session_id;
	
}

void AdminTraceOffMessage(int session_id,admin_parm_type parms[],
                          int num_blak_parm,parm_node blak_parm[])                          
{
	class_node *c;
	int message_id;
	message_node *m;
	
	char *class_name,*message_name;
	class_name = (char *)parms[0];
	message_name = (char *)parms[1];
	
	c = GetClassByName(class_name);
	if (c == NULL)
	{
		aprintf("Cannot find CLASS %s.\n",class_name);
		return;
	}
	
	message_id = GetIDByName(message_name);
	if (message_id == INVALID_ID)
	{
		aprintf("Cannot find MESSAGE %s.\n",message_name);
		return;
	}
	
	message_name = GetNameByID(message_id);
	m = GetMessageByID(c->class_id,message_id,NULL);
	if (m == NULL)
	{
		aprintf("Cannot find MESSAGE %s (%i) for CLASS %s (%i).\n",
			message_name,message_id,c->class_name,c->class_id);
		return;
	}
	
	m->trace_session_id = INVALID_ID;
}

void AdminKickoffAll(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	lprintf("AdminKickoffAll kicking everyone out of the game\n");
	ForEachSession(AdminKickoffEachSession);
}

void AdminKickoffEachSession(session_node *s)
{
	if (s->account == NULL)
		return;
	
	if (s->state != STATE_GAME)
		return;
	
	GameClientExit(s);
	SetSessionState(s,STATE_SYNCHED);
}

void AdminKickoffAccount(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	account_node *a;
	session_node *kickoff_session;
	
	int account_id;
	account_id = (int)parms[0];
	
	a = GetAccountByID(account_id);
	if (a == NULL)
	{
		aprintf("Cannot find ACCOUNT %i.\n",account_id);
		return;
	}
	
	kickoff_session = GetSessionByAccount(a);
	
	if (kickoff_session == NULL)
	{
		aprintf("ACCOUNT %i (%s) is not logged on.\n",account_id,a->name);
		return;
	}
	if (kickoff_session->state != STATE_GAME)
	{
		aprintf("ACCOUNT %i (%s) is not in the game.\n",account_id,a->name);
		return;
	}
	
	lprintf("AdminKickoffAccount kicking ACCOUNT %i (%s) out of the game\n",account_id,a->name);
	GameClientExit(kickoff_session);
	SetSessionState(kickoff_session,STATE_SYNCHED);
}

void AdminHangupAll(int session_id,admin_parm_type parms[],
                    int num_blak_parm,parm_node blak_parm[])
{
	lprintf("AdminHangupAll hanging up everyone\n");
	ForEachSession(AdminHangupEachSession);
}

void AdminHangupEachSession(session_node *s)
{
	HangupSession(s);
}

void AdminHangupUser(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])                     
{
	user_node *u;
	account_node *a;
	session_node *hangup_session;
	int id;
	char *arg_str;
	Bool is_by_number;
	char *ptr;
	
	arg_str = (char *)parms[0];
	is_by_number = True;
	
	ptr = arg_str;
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_by_number = False;
		ptr++;
	}
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		u = GetUserByObjectID(id);
	}
	else
	{
		u = GetUserByName(arg_str);
	}
	
	if (u == NULL)
	{
		aprintf("Cannot find user %s.\n",arg_str);
		return;
	}
	
	a = GetAccountByID(u->account_id);
	
	if (a == NULL)
	{
		aprintf("Cannot find account for user %s.\n",arg_str);
		return;
	}
	
	hangup_session = GetSessionByAccount(a);
	if (hangup_session == NULL)
	{
		aprintf("ACCOUNT %i (%s) is not logged in.\n",
			a->account_id, a->name);
		return;
	}
	
	if (session_id == hangup_session->session_id)
	{
		aprintf("Cannot hangup yourself.\n");
		return;
	}
	
	aprintf("ACCOUNT %i (%s) SESSION %i has been disconnected.\n",
		a->account_id, a->name, hangup_session->session_id);
	
	// Manually hanging up an account will block that IP address
	// from reconnecting for a short time (usually 5min).
	//
	id = ConfigInt(SOCKET_BLOCK_TIME);
	AddBlock(id, &hangup_session->conn.addr);
	
	HangupSession(hangup_session);
}

extern block_node* FindBlock(struct in6_addr* piaPeer);

/*
 * AdminBlockIP - Block an IP address from accessing this server
 */

void AdminBlockIP(int session_id,admin_parm_type parms[],
                  int num_blak_parm,parm_node blak_parm[])                  
{
	struct in6_addr blocktoAdd;
	char *arg_str = (char *)parms[0];
	char ip[46];

	aprintf("This command will only affect specified IPs until the server reboots\n");

	if (inet_pton(AF_INET6, arg_str, &blocktoAdd) != -1) {
		inet_ntop(AF_INET6, &blocktoAdd, ip, sizeof(ip));
		if(FindBlock( &blocktoAdd ) == NULL )  {
			AddBlock(-1, &blocktoAdd);
			aprintf("IP %s blocked\n", ip);
		} else {
			DeleteBlock( &blocktoAdd );
			aprintf("IP %s has been unblocked\n", ip);
		}
	}  else {
		aprintf("Couldn`t build IP address bad format %s\n",arg_str );
	}
}

void AdminHangupAccount(int session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])
{
	account_node *a;
	session_node *hangup_session;
	int id;
	char *arg_str;
	Bool is_by_number;
	char *ptr;
	
	arg_str = (char *)parms[0];
	is_by_number = True;
	
	ptr = arg_str;
	while (*ptr != 0)
	{
		if (*ptr < '0' || *ptr > '9')
			is_by_number = False;
		ptr++;
	}
	
	if (is_by_number)
	{
		sscanf(arg_str,"%i",&id);
		a = GetAccountByID(id);
	}
	else
	{
		a = GetAccountByName(arg_str);
	}
	
	if (a == NULL)
	{
		aprintf("Cannot find account %s.\n",arg_str);
		return;
	}
	
	hangup_session = GetSessionByAccount(a);
	if (hangup_session == NULL)
	{
		aprintf("ACCOUNT %i (%s) is not logged in.\n",a->account_id,a->name);
		return;
	}
	
	if (session_id == hangup_session->session_id)
	{
		aprintf("Cannot hangup yourself.\n");
		return;
	}
	
	aprintf("ACCOUNT %i (%s) SESSION %i has been disconnected.\n",
		a->account_id, a->name, hangup_session->session_id);
	
	// Manually hanging up an account will block that IP address
	// from reconnecting for a short time (usually 5min).
	//
	id = ConfigInt(SOCKET_BLOCK_TIME);
	AddBlock(id, &hangup_session->conn.addr);
	
	HangupSession(hangup_session);
}

void AdminHangupSession(int admin_session_id,admin_parm_type parms[],
                        int num_blak_parm,parm_node blak_parm[])                        
{
	session_node *s;
	
	int session_id;
	session_id = (int)parms[0];
	
	s = GetSessionByID(session_id);
	if (s == NULL)
	{
		aprintf("Cannot find SESSION %i.\n",session_id);
		return;
	}
	
	if (admin_session_id == s->session_id)
	{
		aprintf("Cannot hangup on yourself.\n");
		return;
	}
	
	if (s->conn.type == CONN_CONSOLE)
	{
		aprintf("Cannot hangup the console.\n");
		return;
	}
	
	aprintf("SESSION %i has been disconnected.\n",s->session_id);
	
	HangupSession(s);
}

void AdminReloadSystem(int session_id,admin_parm_type parms[],
                       int num_blak_parm,parm_node blak_parm[])
{
	lprintf("AdminReloadSystem reloading system\n");
	
	PauseTimers();
	aprintf("Garbage collecting and saving game... ");
	
	SendBlakodBeginSystemEvent(SYSEVENT_RELOAD_SYSTEM);
	
	GarbageCollect();
	SaveAll();
	aprintf("done.\n");
	
	aprintf("Unloading game, kodbase, and .bof ... ");
	AdminSendBufferList();

	aprintf("done.\n");
	
	aprintf("Loading game, kodbase, and .bof ... ");
	AdminSendBufferList();

   // Reload game data.
   MainReloadGameData();

	/* can't reload accounts because sessions have pointers to accounts */
	if (!LoadAllButAccount()) 
		eprintf("AdminReload couldn't load game.  You are dead.\n");
	
	AddBuiltInDLlist();
	
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	SendBlakodEndSystemEvent(SYSEVENT_RELOAD_SYSTEM);
	
	aprintf("done.\n");
	
	UnpauseTimers();   
}

void AdminResetUDP(int session_id, admin_parm_type parms[],
                    int num_blak_parm, parm_node blak_parm[])
{
   ResetUDP();
   aprintf("<: Reset UDP socket.\n");
   aprintf(":>\n");
}

/* stuff for foreachsession */
int accounts_in_game;
void AdminReloadGame(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])                     
{
	int save_time;
	
	lprintf("AdminReloadGame\n");
	
	/* make sure no one in game */
	AdminHangupAll(session_id, parms, num_blak_parm, blak_parm);
	
	accounts_in_game = 0;
	ForEachSession(AdminReloadGameEachSession);
	if (accounts_in_game > 0)
	{
		aprintf("Cannot reload game because %i %s in it.\n",
			accounts_in_game,accounts_in_game == 1 ? "person is" : "people are");
		return;
	}
	
	aprintf("Unloading game... ");
	AdminSendBufferList();
	ResetRooms();
	ResetUser();
	ResetString();
	ResetTimer();
	ResetList();
	ResetTables();
	ResetObject();
	aprintf("done.\n");
	AdminSendBufferList();
	
	save_time = (int)parms[0];
	if (save_time != 0)
	{
		lprintf("Game save time forced to (%i)\n", save_time);
		aprintf("Forcing game save time to (%i)... ", save_time);
		SaveControlFile(save_time);
		aprintf("done.\n");
	}
	
	aprintf("Loading game... ");
	AdminSendBufferList();
	
	/* can't reload accounts because sessions have pointers to accounts */
	if (!LoadAllButAccount())
		eprintf("AdminReload couldn't reload all, system dead\n");
	
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	
	/* since it's an older saved game, tell Blakod that everyone's off */
	SendTopLevelBlakodMessage(GetSystemObjectID(),LOADED_GAME_MSG,0,NULL);
	
	aprintf("done.\n");
}

// Uses the accounts_in_game global.
void AdminDeleteUnusedAccounts(int session_id, admin_parm_type parms[],
   int num_blak_parm, parm_node blak_parm[])
{
   // Make sure no one in game, because after deleting accounts
   // we compact the numbers.
   AdminHangupAll(session_id, parms, num_blak_parm, blak_parm);

   accounts_in_game = 0;
   ForEachSession(AdminReloadGameEachSession);
   if (accounts_in_game > 0)
   {
      aprintf("Cannot delete unused accounts because %i %s in game.\n",
         accounts_in_game, accounts_in_game == 1 ? "person is" : "people are");
      return;
   }

   int beforeAcct = GetActiveAccountCount();

   aprintf("Number of accounts before is: %i.\n", beforeAcct);
   DeleteAccountsIfUnused();
   CompactAccounts();
   int afterAcct = GetActiveAccountCount();
   aprintf("Number of accounts after is: %i, deleted %i accounts.\n",
      afterAcct, beforeAcct - afterAcct);
}

void AdminReloadGameEachSession(session_node *s)
{
	if (s->state == STATE_GAME)
		accounts_in_game++;
}

void AdminReloadMotd(int session_id,admin_parm_type parms[],
                     int num_blak_parm,parm_node blak_parm[])
{
	lprintf("AdminReloadMotd reloading message of the day\n");
	
	aprintf("Reloading motd... ");
	AdminSendBufferList();
	
	ResetLoadMotd();
	LoadMotd();
	
	aprintf("done.\n");
}

void AdminReloadPackages(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])
{
	aprintf("Reloading packages... ");
	AdminSendBufferList();
	
	ResetDLlist();
	AddBuiltInDLlist();
	aprintf("done.\n");
}

void AdminDisableSysTimer(int session_id,admin_parm_type parms[],
                          int num_blak_parm,parm_node blak_parm[])
{
	
	int systimer_type;
	systimer_type = (int)parms[0];
	
	if (DisableSysTimer(systimer_type) == False)
		aprintf("Invalid systimer type %i.\n",systimer_type);
	else
		aprintf("Systimer disabled.\n");
	
}

void AdminEnableSysTimer(int session_id,admin_parm_type parms[],
                         int num_blak_parm,parm_node blak_parm[])                         
{
	
	int systimer_type;
	systimer_type = (int)parms[0];
	
	if (EnableSysTimer(systimer_type) == False)
		aprintf("Invalid systimer type %i.\n",systimer_type);
	else
		aprintf("Systimer enabled.\n");
	
}

static char *say_admin_text;
static int say_admin_session_id;
void AdminSay(int session_id,admin_parm_type parms[],
              int num_blak_parm,parm_node blak_parm[])              
{
	char *text;
	text = (char *)parms[0];
	
	say_admin_text = text;
	say_admin_session_id = session_id;
	
	ForEachSession(AdminSayEachAdminSession);
	
	aprintf("Said.\n");
}

void AdminSayEachAdminSession(session_node *s)
{
	session_node *sender_session;
	const char* account;
	
	sender_session = GetSessionByID(say_admin_session_id);
	if (sender_session == NULL)
	{
		eprintf("AdminSayEachAdminSession no sender SESSION %i.\n",say_admin_session_id);
		return;
	}
	
	if (sender_session->session_id == s->session_id)
		return;
	
	if (s->state == STATE_ADMIN ||
		(s->state == STATE_GAME && s->account && s->account->type == ACCOUNT_ADMIN))
	{
		account = "Administrator";
		if (sender_session->account &&
			sender_session->account->name &&
			*sender_session->account->name)
		{
			account = sender_session->account->name;
		}
		
		SendSessionAdminText(s->session_id,
			"%s says, \"%s\"\n",
			account,say_admin_text);
	}
}

void AdminRead(int session_id,admin_parm_type parms[],
               int num_blak_parm,parm_node blak_parm[])               
{
	FILE *fptr;
	char line[2000];
	char *ptr;
	static int admin_in_read = False;
	
	char *filename;
	filename = (char *)parms[0];
	
	if (admin_in_read)
	{
		aprintf("Recursive read or script command not allowed.\n");
		return;
	}
	
	fptr = fopen(filename,"rt");
	if (fptr == NULL)
	{
		aprintf("Error opening %s.\n",filename);
		return;
	}
	
	admin_in_read = True;
	
	while (fgets(line,sizeof(line)-1,fptr))
	{
		ptr = strtok(line,"\n");
		if (ptr)
		{
			while (*ptr == ' ' || *ptr == '\t')
				ptr++;
			
			if (*ptr)
			{
				aprintf(">> %s\n",ptr);
				DoAdminCommand(ptr);
				AdminSendBufferList();
			}
		}
	}
	
	fclose(fptr);
	
	admin_in_read = False;
}

void AdminMark(int session_id,admin_parm_type parms[],
               int num_blak_parm,parm_node blak_parm[])               
{
	lprintf("-------------------------------------------------------------------------------------\n");
	dprintf("-------------------------------------------------------------------------------------\n");
	eprintf("-------------------------------------------------------------------------------------\n");
}
