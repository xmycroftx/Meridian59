// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* interface_windows.c
*

  This module maintains the dialog box interface to the server.  The
  initialization starts a new thread with the window procedure, waits
  for it to initialize some stuff (so StartupPrintf() will work), and
  returns.  The interface is a modeless dialog box, with our own window
  procedure.  See Petzold for how this works.
  
	The main thread can signal the interface thread to do things in a few
	ways, which all post messages on the interface's message queue.
	Accessing game data is done by having a lock for everything, which
	the main thread releases when it sleeps in the timer loop.
	
*/

#include "blakserv.h"
#include <mmsystem.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>

/* event sent to our interface when it should update numbers */
#define WM_BLAK_NEW_INFO       (WM_APP + 0)
/* events for logons/offs/session state change, lParam have the session id */
#define WM_BLAK_LOGON          (WM_APP + 1)
#define WM_BLAK_LOGOFF         (WM_APP + 2)
/* events for updating the channels */
#define WM_BLAK_UPDATE_SESSION (WM_APP + 3)
#define WM_BLAK_UPDATE_CHANNEL (WM_APP + 4)
#define WM_BLAK_UPDATE_ADMIN   (WM_APP + 5)
/* event to signal the console */
#define WM_BLAK_SIGNAL_CONSOLE (WM_APP + 6)
/* events for asynchronous socket I/O */
#define WM_BLAK_SOCKET_ACCEPT  (WM_APP + 7)
#define WM_BLAK_SOCKET_MAINTENANCE_ACCEPT (WM_APP + 8)
#define WM_BLAK_SOCKET_NAME_LOOKUP (WM_APP + 10)
#define WM_BLAK_SOCKET_SELECT  (WM_APP + 11)
#define WM_BLAK_SOCKET_SELECT_UDP (WM_APP + 12)

#define CHANNEL_INTERFACE_LINES 5000 /* number of lines we'll keep in a list box */

HINSTANCE hInst;
int window_display;

HANDLE hEvent;

HWND hwndMain;

HWND hwndTab;
HWND hwndLV;

HWND hwndTab_page;

#define NUM_TAB_PAGES 3
HWND tab_pages[NUM_TAB_PAGES];
HWND tab_about;

Bool is_about;

#define HWND_STATUS tab_pages[0]
#define HWND_CHANNEL tab_pages[1]
#define HWND_ADMIN tab_pages[2]

/* subclass admin edit window to get enter and tab keys */

#ifdef STRICT
static WNDPROC lpfnDefAdminInputProc;
static WNDPROC lpfnDefAdminResponseProc;
#else
static FARPROC lpfnDefAdminInputProc;
static FARPROC lpfnDefAdminResponseProc;
#endif

/* status window stuff--make a timer to clear it every once in a while */
#define STATUS_CONNECTION_WIDTH 30
#define STATUS_CLEAR_TIME 20000
#define WIN_TIMER_ID 1
Bool is_timer_pending;

#define ADMIN_RESPONSE_SIZE (256 * 1024)

char admin_response_buf[ADMIN_RESPONSE_SIZE+1];
int len_admin_response_buf;

int console_session_id;

int sessions_logged_on;

/* local function prototypes */
void __cdecl InterfaceThread(void *unused);
LRESULT CALLBACK InterfaceKeyHook(int code,WPARAM wParam,LPARAM lParam);
long WINAPI InterfaceWindowProc(HWND hwnd,UINT message,UINT wParam,LONG lParam);
void InterfaceSetTab(int sel);
void InterfaceTabChange(void);
void InterfaceSetup(void);
void InterfaceCreateListControl(void);
void InterfaceCreate(HWND hwnd,UINT wParam,LONG lParam);
void InterfaceCreateTabControl(HWND hwnd);
void InterfaceCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify);
void InterfaceCountSessions(session_node *s);
void InterfaceDrawText(HWND hwnd);
void InterfaceCheckChannels(void);
void InterfaceSave(void);
void InterfaceReloadSystem(void);
void CenterWindow(HWND hwnd, HWND hwndParent);

BOOL CALLBACK InterfaceDialogMotd(HWND hwnd,UINT message,UINT wParam,LONG lParam);
BOOL CALLBACK InterfaceDialogAbout(HWND hwnd,UINT message,UINT wParam,LONG lParam);
BOOL CALLBACK InterfaceDialogTabPage(HWND hwnd,UINT message,UINT wParam,LONG lParam);
BOOL CALLBACK InterfaceDialogRegCallback(HWND hwnd, UINT message, UINT wParam, LONG lParam);
void InterfaceTabPageCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify);
long CALLBACK InterfaceAdminInputProc(HWND hwnd, UINT message, UINT wParam, LONG lParam);
long CALLBACK InterfaceAdminResponseProc(HWND hwnd, UINT message, UINT wParam, LONG lParam);
void InterfaceAddAdminBuffer(char *buf,int len_buf);
void InterfaceAddList(int session_id);
void InterfaceRemoveList(int session_id);
void InterfaceUpdateList(int session_id);
void InterfaceUpdateAdmin(void);

void InitInterface(void)
{
	HANDLE hThread;
	
	sessions_logged_on = 0;
	is_timer_pending = False;
	is_about = False;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	
	hThread = (HANDLE) _beginthread(InterfaceThread,0,0);
	
	/* important not to reduce priority because otherwise it holds open
    * critical sections too long */
	
	/* we need to wait until the window is created */
	WaitForSingleObject(hEvent,INFINITE);
	CloseHandle(hEvent);
	
}

void StoreInstanceData(HINSTANCE hInstance,int how_show)
{
	hInst = hInstance;
	window_display = how_show;
}

int GetUsedSessions(void)
{
	return sessions_logged_on;
}

void __cdecl InterfaceThread(void *unused)
{
	WNDCLASS main_class;
	MSG msg;
	HACCEL hAccel;
	
	main_class.style = CS_HREDRAW | CS_VREDRAW;
	main_class.lpfnWndProc = InterfaceWindowProc;
	main_class.cbClsExtra = 0;
	main_class.cbWndExtra = DLGWINDOWEXTRA;
	main_class.hInstance = hInst;
	main_class.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_MAIN));
	main_class.hCursor = NULL;
	main_class.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
	main_class.lpszMenuName = NULL;
	main_class.lpszClassName = "blakserv";
	
	RegisterClass(&main_class);
	
	InitCommonControls();

   LoadLibrary("riched20.dll");

	hwndMain = CreateDialog(hInst,MAKEINTRESOURCE(IDD_MAIN_DLG),0,NULL);
	
	InterfaceSetup();
	InterfaceCreateTabControl(hwndMain);
	InterfaceCreateListControl();
	
	ShowWindow(hwndMain,window_display);
	
	/* the main thread is waiting for our window to be created--tell it we're ready */
	SetEvent(hEvent);
	
	hAccel = LoadAccelerators(hInst,MAKEINTRESOURCE(IDR_ACCELERATOR));
	
	
	while (GetMessage(&msg,NULL,0,0))
	{
		if (!TranslateAccelerator(hwndMain,hAccel,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	DestroyAcceleratorTable(hAccel);
	
	_endthread();
}

long WINAPI InterfaceWindowProc(HWND hwnd,UINT message,UINT wParam,LONG lParam)
{
	char buf[40];
	
	switch (message)
	{
	case WM_CREATE :
		InterfaceCreate(hwnd,wParam,lParam);
		break;
		
		HANDLE_MSG(hwnd,WM_COMMAND,InterfaceCommand);
		
	case WM_DESTROY :
		SetQuit();
		PostQuitMessage(0);
		return 0;
		
	case WM_CLOSE :
		if (MessageBox(hwnd,"Are you sure you want to exit?",BlakServNameString(),
			MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES)
		{
			DestroyWindow(hwnd);
		}
		break;
		
	case WM_TIMER :
		KillTimer(hwndMain,WIN_TIMER_ID);
		SendDlgItemMessage(hwndMain,IDS_STATUS_WINDOW,SB_SETTEXT,0,(LPARAM)"");
		break;
		
	case WM_NOTIFY: 
		switch (((LPNMHDR) lParam)->code)
		{	 
		case TTN_NEEDTEXT: 
			{ 
				LPTOOLTIPTEXT lpttt; 
				
				lpttt = (LPTOOLTIPTEXT) lParam; 
				lpttt->hinst = hInst; 
				
				/* Specify the resource identifier of the descriptive 
				text for the given button. */
				switch (lpttt->hdr.idFrom)
				{ 
				case IDM_FILE_EXIT : 
					lpttt->lpszText = "Exit";
					break; 
				case IDM_FILE_SAVE :
					lpttt->lpszText = "Save Game";
					break; 
				case IDM_FILE_RELOADSYSTEM :
					lpttt->lpszText = "Reload System";
					break; 
				case IDM_MESSAGES_MESSAGEOFTHEDAY :
					lpttt->lpszText = "Message of the Day";
					break;
            case IDM_REGISTER_CALLBACK:
               lpttt->lpszText = "Register a kod callback";
               break;
				case IDM_HELP_ABOUT :
					lpttt->lpszText = "About";
					break; 
				} 
				break;
			} 
			break;
			
		case TCN_SELCHANGE :
			InterfaceTabChange();
			break;
		case TCN_SELCHANGING :
			return FALSE; /* means we'll allow it to change */
			break;
			
		}
		break;
		
		case WM_BLAK_NEW_INFO :
			InterfaceDrawText(hwnd);
			break;
			
		case WM_BLAK_LOGON :
			sessions_logged_on++;
			/*
			sprintf(buf,"Connections: %i",sessions_logged_on);
			SetDlgItemText(HWND_STATUS,IDC_CONNECTIONS_BORDER,buf);
			*/
			sprintf(buf,"%3i",sessions_logged_on);
			SendDlgItemMessage(hwndMain,IDS_STATUS_WINDOW,SB_SETTEXT,1,(LPARAM)buf);
			
			InterfaceAddList(lParam);
			break;
			
		case WM_BLAK_LOGOFF :
			sessions_logged_on--;
			if (sessions_logged_on < 0)
				eprintf("InterfaceWindowProc sessions_logged_on just went negative!\n");
				/*
				sprintf(buf,"Connections: %i",sessions_logged_on);
				SetDlgItemText(HWND_STATUS,IDC_CONNECTIONS_BORDER,buf);
			*/
			sprintf(buf,"%3i",sessions_logged_on);
			SendDlgItemMessage(hwndMain,IDS_STATUS_WINDOW,SB_SETTEXT,1,(LPARAM)buf);
			
			InterfaceRemoveList(lParam);
			
			break;
			
		case WM_BLAK_UPDATE_SESSION :
			InterfaceUpdateList(lParam);
			break;
			
		case WM_BLAK_UPDATE_CHANNEL :
			InterfaceCheckChannels();
			break;
			
		case WM_BLAK_UPDATE_ADMIN :
			InterfaceUpdateAdmin();
			break;
			
		case WM_BLAK_SIGNAL_CONSOLE :
			if (PlaySound("signal",hInst,SND_RESOURCE | SND_ASYNC) == FALSE)
			{
				MessageBeep(MB_OK);
				break;
			}
			FlashWindow(hwnd,TRUE);
			Sleep(200);
			FlashWindow(hwnd,TRUE);
			break;
			
		case WM_BLAK_SOCKET_ACCEPT :
			AsyncSocketAccept(wParam,WSAGETSELECTEVENT(lParam),WSAGETSELECTERROR(lParam),
				SOCKET_PORT);
			break;
			
		case WM_BLAK_SOCKET_MAINTENANCE_ACCEPT :
			AsyncSocketAccept(wParam,WSAGETSELECTEVENT(lParam),WSAGETSELECTERROR(lParam),
				SOCKET_MAINTENANCE_PORT);
			break;
			
		case WM_BLAK_SOCKET_NAME_LOOKUP :
			AsyncNameLookup((HANDLE)wParam,WSAGETASYNCERROR(lParam));
			break;
			
		case WM_BLAK_SOCKET_SELECT :
			AsyncSocketSelect(wParam,WSAGETSELECTEVENT(lParam),WSAGETSELECTERROR(lParam));
			break;
			
      case WM_BLAK_SOCKET_SELECT_UDP:
         AsyncSocketSelectUDP(wParam);
         break;

      // NOTE: if we need to handle Windows time update, this would be the place.
      // case WM_TIMECHANGE:
      //    break;

		default :
			return DefWindowProc(hwnd,message,wParam,lParam);    
   }
   return TRUE;
}

void InterfaceAddList(int session_id)
{
	session_node *s;
	LV_ITEM lvi;
	int index;
	
	EnterServerLock();
	s = GetSessionByID(session_id);
	if (s == NULL)
	{
		LeaveServerLock();
		return;
	}
	
	index = ListView_GetItemCount(hwndLV);
	
	/* Initialize LV_ITEM members that are common to all items. */
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE; 
	lvi.state = 0; 
	lvi.stateMask = 0; 
	lvi.pszText = "  ?";  
	lvi.iImage = -1;
	
	lvi.iItem = index;
	lvi.iSubItem = 0; 
	lvi.lParam = session_id;
	
	ListView_InsertItem(hwndLV,&lvi); 
	
	if (s->account == NULL)
	{
		/* need braces around this macro to use in an if/else */
		ListView_SetItemText(hwndLV,index,1,"");
	}
	else   
		ListView_SetItemText(hwndLV,index,1,s->account->name);
	
	ListView_SetItemText(hwndLV,index,2,(char *) ShortTimeStr(s->connected_time));
	ListView_SetItemText(hwndLV,index,3,(char *) GetStateName(s));
	ListView_SetItemText(hwndLV,index,4,s->conn.name);
	
	LeaveServerLock();
}

void InterfaceRemoveList(int session_id)
{
	LV_FINDINFO lvf;
	int index;
	
	lvf.flags = LVFI_PARAM;
	lvf.lParam = session_id;
	index = ListView_FindItem(hwndLV,-1,&lvf);
	
	if (index >= 0)
	{
		ListView_DeleteItem(hwndLV,index);
	}
}

void InterfaceUpdateList(int session_id)
{
	session_node *s;
	char buf[20];
	LV_FINDINFO lvf;
	int index;
	
	EnterServerLock();
	s = GetSessionByID(session_id);
	if (s == NULL)
	{
		LeaveServerLock();
		return;
	}
	
	lvf.flags = LVFI_PARAM;
	lvf.lParam = session_id;
	index = ListView_FindItem(hwndLV,-1,&lvf);
	
	if (index >= 0)
	{
		if (s->account == NULL)
		{
			ListView_SetItemText(hwndLV,index,0,"  ?");
			ListView_SetItemText(hwndLV,index,1,"");
		}
		else
		{
			sprintf(buf,"%3i",s->account->account_id);
			ListView_SetItemText(hwndLV,index,0,buf);
			ListView_SetItemText(hwndLV,index,1,s->account->name);
		}      
		ListView_SetItemText(hwndLV,index,2,(char *) ShortTimeStr(s->connected_time));
		ListView_SetItemText(hwndLV,index,3,(char *) GetStateName(s));
		ListView_SetItemText(hwndLV,index,4,s->conn.name);
	}
	LeaveServerLock();
	
}


/* this is executed in the main, non-interface thread */
void InterfaceUpdate()
{
	if (!GetQuit())
		PostMessage(hwndMain,WM_BLAK_NEW_INFO,0,0);
}

/* this is executed in the main, non-interface thread */
void InterfaceLogon(session_node *s)
{
	if (s->conn.type != CONN_CONSOLE)
		if (!GetQuit())
			PostMessage(hwndMain,WM_BLAK_LOGON,0,(LPARAM) s->session_id);
}

/* this is executed in the main, non-interface thread */
void InterfaceLogoff(session_node *s)
{
	if (s->conn.type != CONN_CONSOLE)
		if (!GetQuit())
			PostMessage(hwndMain,WM_BLAK_LOGOFF,0,(LPARAM) s->session_id);
}

/* this is executed in the main, non-interface thread */
void InterfaceUpdateSession(session_node *s)
{
	if (s->conn.type != CONN_CONSOLE)
		if (!GetQuit())
			PostMessage(hwndMain,WM_BLAK_UPDATE_SESSION,0,(LPARAM) s->session_id);
}

/* this is executed in the main, non-interface thread */
void InterfaceUpdateChannel()
{
	if (!GetQuit())
		PostMessage(hwndMain,WM_BLAK_UPDATE_CHANNEL,0,0);
}

/* this is executed in the main, non-interface thread */
void InterfaceSignalConsole()
{
	if (!GetQuit())
		PostMessage(hwndMain,WM_BLAK_SIGNAL_CONSOLE,0,0);
}

/* this is executed in the main, non-interface thread */
void StartAsyncSocketAccept(SOCKET sock,int connection_type)
{
	int window_event,val;

	window_event = WM_BLAK_SOCKET_ACCEPT;
	if (connection_type == SOCKET_MAINTENANCE_PORT)
		window_event = WM_BLAK_SOCKET_MAINTENANCE_ACCEPT;
	
	val = WSAAsyncSelect(sock,hwndMain,window_event,FD_ACCEPT);
	if (val != 0)
		eprintf("StartAsyncSocketAccept got error %i\n",val);
}

void StartAsyncSocketUDPRead(SOCKET sock)
{
   int val = WSAAsyncSelect(sock, hwndMain, WM_BLAK_SOCKET_SELECT_UDP, FD_READ);
   if (val != 0)
      eprintf("StartAsyncSocketUDPRead got error %i\n", val);
}

/* this is executed in our thread, actually.
   this needs modifications to work with IPv6 */
HANDLE StartAsyncNameLookup(char *peer_addr,char *buf)
{
	HANDLE ret_val;
	
	ret_val = WSAAsyncGetHostByAddr(hwndMain,WM_BLAK_SOCKET_NAME_LOOKUP,
		peer_addr,4,PF_INET,buf,MAXGETHOSTSTRUCT);
	if (ret_val == 0)
		eprintf("StartAsyncNameLookup got error %s\n",GetLastErrorStr());
	
	return ret_val;
}

/* this is executed in the main, non-interface thread */
void StartAsyncSession(session_node *s)
{
	int val;
	
	val = WSAAsyncSelect(s->conn.socket,hwndMain,WM_BLAK_SOCKET_SELECT,
		FD_CLOSE | FD_WRITE | FD_READ);
	if (val != 0)
		eprintf("StartAsyncSocketSelect got error %i\n",val);
}

/* this cannot be called from main thread, causes problems!!!!!! */
void StartupPrintf(const char *fmt,...)
{
	char s[200];
	va_list marker;
	
	va_start(marker,fmt);
	vsprintf(s,fmt,marker);
	
	
	if (strlen(s) > 0)
	{
		if (s[strlen(s)-1] == '\n') /* ignore \n char at the end of line */
			s[strlen(s)-1] = 0;
	}
	va_end(marker);
	
	Edit_SetText(GetDlgItem(HWND_STATUS,IDC_STARTUP_TEXT),s);
}

void StartupComplete()
{
	char str[200];
	connection_node conn;
	session_node *s;
	
	len_admin_response_buf = 0;
	
	conn.type = CONN_CONSOLE;
	s = CreateSession(conn);
	if (s == NULL)
		FatalError("Interface can't make session for console");
	s->account = GetConsoleAccount();
	InitSessionState(s,STATE_ADMIN);
	console_session_id = s->session_id;
	
	
	if (Edit_GetText(GetDlgItem(HWND_STATUS,IDC_STARTUP_TEXT),str,sizeof(str)) == 0)
		StartupPrintf("No errors on startup\n");
	
	SendDlgItemMessage(hwndMain,IDC_TOOLBAR,TB_ENABLEBUTTON,IDM_FILE_EXIT,MAKELPARAM(TRUE,0));
	SendDlgItemMessage(hwndMain,IDC_TOOLBAR,TB_ENABLEBUTTON,IDM_FILE_SAVE,MAKELPARAM(TRUE,0));
	SendDlgItemMessage(hwndMain,IDC_TOOLBAR,TB_ENABLEBUTTON,IDM_FILE_RELOADSYSTEM,
		      MAKELPARAM(TRUE,0));
	SendDlgItemMessage(hwndMain,IDC_TOOLBAR,TB_ENABLEBUTTON,IDM_MESSAGES_MESSAGEOFTHEDAY,
		      MAKELPARAM(TRUE,0));
   SendDlgItemMessage(hwndMain, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REGISTER_CALLBACK,
      MAKELPARAM(TRUE, 0));
}

void InterfaceCreate(HWND hwnd,UINT wParam,LONG lParam)
{
	SetWindowText(hwnd,BlakServNameString());
}

void InterfaceCreateTabControl(HWND hwnd) 
{ 
    TC_ITEM tie; 
    char s[100];
    HWND hwndCtl;
    LOGFONT lf;
    HFONT font;
    
    hwndTab = GetDlgItem(hwndMain,IDC_TAB_MAIN);
	
    tie.mask = TCIF_TEXT | TCIF_IMAGE; 
    tie.iImage = -1; 
    tie.pszText = s;
	
    tie.pszText = "&Status";
    TabCtrl_InsertItem(hwndTab,0, &tie);
    tie.pszText = "&Channels";
    TabCtrl_InsertItem(hwndTab,1, &tie);
    tie.pszText = "&Administration";
    TabCtrl_InsertItem(hwndTab,2, &tie);
	
    tab_pages[0] = CreateDialog(hInst,MAKEINTRESOURCE(IDD_TAB_PAGE_STATUS),hwndTab,
		InterfaceDialogTabPage);
    tab_pages[1] = CreateDialog(hInst,MAKEINTRESOURCE(IDD_TAB_PAGE_CHANNELS),hwndTab,
		InterfaceDialogTabPage);
    tab_pages[2] = CreateDialog(hInst,MAKEINTRESOURCE(IDD_TAB_PAGE_ADMINISTRATION),hwndTab,
		InterfaceDialogTabPage);
	
    /* keep about box page ready */
    tab_about = CreateDialog(hInst,MAKEINTRESOURCE(IDD_ABOUT),hwnd,
			     InterfaceDialogAbout);
	
	
    /* set admin page font */
    hwndCtl = GetDlgItem(HWND_ADMIN,IDC_ADMIN_RESPONSE);
    SendMessage(hwndCtl,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),
		MAKELPARAM(TRUE,0));
	
    lf.lfHeight = 13;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = 800;
    lf.lfItalic = 0; 
    lf.lfUnderline = 0;
    lf.lfStrikeOut = 0;
    lf.lfCharSet = 255;
    lf.lfOutPrecision = 1;
    lf.lfClipPrecision = 2;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = 49;
    strcpy(lf.lfFaceName,"Consolas");
    
    font = CreateFontIndirect(&lf);
    if (font != NULL)
    {
		SendMessage(hwndCtl,WM_SETFONT,(WPARAM)font,MAKELPARAM(TRUE,0));
    }

    /* Set text buffer to be large */
    SendMessage(hwndCtl, EM_EXLIMITTEXT, ADMIN_RESPONSE_SIZE, 0);
    
    lpfnDefAdminResponseProc = SubclassWindow(hwndCtl,InterfaceAdminResponseProc);
	
    hwndCtl = GetDlgItem(HWND_ADMIN,IDC_ADMIN_COMMAND);
    lpfnDefAdminInputProc = SubclassWindow(hwndCtl,InterfaceAdminInputProc);
	
    if (font != NULL)
    {
       SendMessage(hwndCtl, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
    }

    hwndTab_page = NULL;
	
    InterfaceTabChange();
}

void InterfaceSetTab(int sel)
{
	if (sel == TabCtrl_GetCurSel(hwndTab))
		return;
	TabCtrl_SetCurSel(hwndTab,sel);
	InterfaceTabChange();
	if (hwndTab_page == HWND_ADMIN)
		SetFocus(GetDlgItem(HWND_ADMIN,IDC_ADMIN_COMMAND));
}

void InterfaceTabChange() 
{ 
	int sel;
	
	if (is_about)
	{
		/* currently showing about box, so undo that */
		ShowWindow(tab_about,FALSE);
	}   
	
	sel = TabCtrl_GetCurSel(hwndTab); 
	
	/* Hide the current child dialog box, if any. */
	if (hwndTab_page != NULL)
	{
		ShowWindow(hwndTab_page,FALSE);
	}
	
	/* Show the new child dialog box. */
	hwndTab_page = tab_pages[sel];
	ShowWindow(hwndTab_page,TRUE);
} 


void InterfaceSetup()
{
	HWND hwndTB; 
	TBADDBITMAP tbab; 
	TBBUTTON tbb[9];
	
	RECT rcClient;
	int status_widths[2];
	
	hwndTB = CreateWindowEx(0,TOOLBARCLASSNAME,NULL,WS_CHILD | TBSTYLE_TOOLTIPS,
		0,0,0,0,hwndMain,(HMENU) IDC_TOOLBAR,hInst, NULL); 
	
	
	
	SendMessage(hwndTB,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0); 
	/*
	tbab.hInst = hInst;
	tbab.nID   = IDB_TOOLBAR;
	*/
	tbab.hInst = HINST_COMMCTRL;
	tbab.nID   = IDB_STD_SMALL_COLOR;
	
	SendMessage(hwndTB, TB_ADDBITMAP,15,(LPARAM)&tbab); 
	
	/* buttons to the toolbar */
	tbb[0].iBitmap = STD_DELETE; 
	tbb[0].idCommand = IDM_FILE_EXIT; 
	tbb[0].fsState = 0; 
	tbb[0].fsStyle = TBSTYLE_BUTTON; 
	tbb[0].dwData = 0; 
	tbb[0].iString = 0;
	
	tbb[1].iBitmap = STD_FILESAVE; 
	tbb[1].idCommand = IDM_FILE_SAVE; 
	tbb[1].fsState = 0; 
	tbb[1].fsStyle = TBSTYLE_BUTTON; 
	tbb[1].dwData = 0; 
	tbb[1].iString = 0;
	
	tbb[2].iBitmap = STD_UNDO; 
	tbb[2].idCommand = IDM_FILE_RELOADSYSTEM; 
	tbb[2].fsState = 0; 
	tbb[2].fsStyle = TBSTYLE_BUTTON; 
	tbb[2].dwData = 0; 
	tbb[2].iString = 0;
	
	tbb[3].iBitmap = 0;
	tbb[3].idCommand = 0;
	tbb[3].fsState = 0; 
	tbb[3].fsStyle = TBSTYLE_SEP; 
	tbb[3].dwData = 0; 
	tbb[3].iString = 0;
	
	tbb[4].iBitmap = STD_PROPERTIES; 
	tbb[4].idCommand = IDM_MESSAGES_MESSAGEOFTHEDAY;
	tbb[4].fsState = 0; 
	tbb[4].fsStyle = TBSTYLE_BUTTON; 
	tbb[4].dwData = 0; 
	tbb[4].iString = 0;
	
   tbb[5].iBitmap = STD_FILENEW;
   tbb[5].idCommand = IDM_REGISTER_CALLBACK;
   tbb[5].fsState = 0;
   tbb[5].fsStyle = TBSTYLE_BUTTON;
   tbb[5].dwData = 0;
   tbb[5].iString = 0;

	tbb[6].iBitmap = 0;
	tbb[6].idCommand = 0;
	tbb[6].fsState = 0; 
	tbb[6].fsStyle = TBSTYLE_SEP; 
	tbb[6].dwData = 0; 
	tbb[6].iString = 0;
	
	tbb[7].iBitmap = STD_FIND;
	tbb[7].idCommand = IDM_HELP_ABOUT;
	tbb[7].fsState = TBSTATE_ENABLED; 
	tbb[7].fsStyle = TBSTYLE_BUTTON; 
	tbb[7].dwData = 0; 
	tbb[7].iString = 0;
	


	SendMessage(hwndTB,TB_ADDBUTTONS,8,(LPARAM)&tbb); 
	
	ShowWindow(hwndTB,SW_SHOW);
	
	CreateStatusWindow(WS_CHILD | WS_VISIBLE,"",hwndMain,IDS_STATUS_WINDOW);
	GetClientRect(hwndMain,&rcClient); 
	status_widths[0] = rcClient.right - STATUS_CONNECTION_WIDTH;
	status_widths[1] = rcClient.right - 1;
	
	SendDlgItemMessage(hwndMain,IDS_STATUS_WINDOW,SB_SETPARTS,2,(LPARAM)status_widths);
	SendDlgItemMessage(hwndMain,IDS_STATUS_WINDOW,SB_SETTEXT,1,(LPARAM)"  0");
	
	
}

void InterfaceCreateListControl()
{
   LV_COLUMN lvc;
   LOGFONT lf;
   HFONT font;

   hwndLV = GetDlgItem(HWND_STATUS, IDC_CONNECTION_LIST);

   lf.lfHeight = 14;
   lf.lfWidth = 0;
   lf.lfEscapement = 0;
   lf.lfOrientation = 0;
   lf.lfWeight = 800;
   lf.lfItalic = 0;
   lf.lfUnderline = 0;
   lf.lfStrikeOut = 0;
   lf.lfCharSet = 255;
   lf.lfOutPrecision = 1;
   lf.lfClipPrecision = 2;
   lf.lfQuality = CLEARTYPE_QUALITY;
   lf.lfPitchAndFamily = 49;
   strcpy(lf.lfFaceName, "Consolas");

   font = CreateFontIndirect(&lf);
   if (font != NULL)
      SendMessage(hwndLV, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));

   /* Initialize the LV_COLUMN structure. */
   lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
   lvc.fmt = LVCFMT_LEFT;

   /* make the columns */

   lvc.pszText = "#";
   lvc.iSubItem = 0;
   lvc.cx = 29;
   ListView_InsertColumn(hwndLV, 0, &lvc);

   lvc.pszText = "Acct Name";
   lvc.iSubItem = 1;
   lvc.cx = 125;
   ListView_InsertColumn(hwndLV, 1, &lvc);

   lvc.pszText = "Since";
   lvc.iSubItem = 2;
   lvc.cx = 106;
   ListView_InsertColumn(hwndLV, 2, &lvc);

   lvc.pszText = "State";
   lvc.iSubItem = 3;
   lvc.cx = 110;
   ListView_InsertColumn(hwndLV, 3, &lvc);

   lvc.pszText = "From";
   lvc.iSubItem = 4;
   lvc.cx = 160;
   ListView_InsertColumn(hwndLV, 4, &lvc);
}

void InterfaceCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
	case IDC_SET_PAGE_STATUS :
		InterfaceSetTab(0);
		return;
	case IDC_SET_PAGE_CHANNELS :
		InterfaceSetTab(1);
		return;
	case IDC_SET_PAGE_ADMINISTRATION :
		InterfaceSetTab(2);
		return;
	case IDC_SET_PAGE_NEXT :
		InterfaceSetTab((TabCtrl_GetCurSel(hwndTab)+1) % NUM_TAB_PAGES);
		return;
	case IDC_SET_PAGE_PREV :
		InterfaceSetTab((TabCtrl_GetCurSel(hwndTab)+NUM_TAB_PAGES-1) % NUM_TAB_PAGES);
		return;
		
	}
	
	switch (id)
	{
		/* toolbars send many of these now */
		/* buttons */
	case IDB_EXIT :
		PostMessage(hwndMain,WM_CLOSE,0,0);
		return;
		
		/* menu selections */
	case IDM_FILE_EXIT :
		PostMessage(hwndMain,WM_CLOSE,0,0);
		return;
	case IDM_FILE_SAVE :
		InterfaceSave();
		return;
	case IDM_FILE_RELOADSYSTEM :
		InterfaceReloadSystem();
		return;
	case IDM_MESSAGES_MESSAGEOFTHEDAY :
		DialogBox(hInst,MAKEINTRESOURCE(IDD_MOTD),hwnd,InterfaceDialogMotd);
		return;
   case IDM_REGISTER_CALLBACK:
      DialogBox(hInst, MAKEINTRESOURCE(IDD_REGCALLBACK), hwnd, InterfaceDialogRegCallback);
      return;
	case IDM_HELP_ABOUT :
		ShowWindow(tab_about,TRUE);
		ShowWindow(hwndTab_page,FALSE);
		return;
	}
}

void InterfaceDrawText(HWND hwnd)
{
	char s[500];
	kod_statistics *kstat;
	
	if (TryEnterServerLock())
	{
		sprintf(s,"%i bytes",GetMemoryTotal());
		SetDlgItemText(HWND_STATUS,IDC_MEMORY_VALUE,s);
		
		kstat = GetKodStats();
		sprintf(s,"%s",TimeStr(kstat->system_start_time));
		SetDlgItemText(HWND_STATUS,IDC_STARTED_VALUE,s);
		
		sprintf(s,"%-200s",RelativeTimeStr(GetTime()-kstat->system_start_time));
		SetDlgItemText(HWND_STATUS,IDC_UP_FOR_VALUE,s);
		
		if (kstat->interpreting_time/1000.0 < 0.01) 
			sprintf(s,"0/second");
		else
			sprintf(s,"%i/second",(int)(kstat->num_interpreted/(kstat->interpreting_time/1000.0)));
		SetDlgItemText(HWND_STATUS,IDC_SPEED_VALUE,s);
		
		if (IsGameLocked())
			SetDlgItemText(hwndMain,IDC_GAME_LOCKED,"The game is locked.");
		else
			SetDlgItemText(hwndMain,IDC_GAME_LOCKED,"");
		
		SetDlgItemInt(HWND_STATUS,IDC_OBJECTS_VALUE,GetObjectsUsed(),FALSE);
		SetDlgItemInt(HWND_STATUS,IDC_LISTNODES_VALUE,GetListNodesUsed(),FALSE);
		SetDlgItemInt(HWND_STATUS,IDC_STRINGS_VALUE,GetStringsUsed(),FALSE);
		SetDlgItemInt(HWND_STATUS,IDC_TIMERS_VALUE,GetNumActiveTimers(),FALSE);
		
		LeaveServerLock();
	}
}

void InterfaceCheckChannels()
{
   int num_items;
   channel_buffer_node *cb;
   HWND hwndList;

   while (IsNewChannelText())
   {
      cb = GetChannelBuffer();
      switch (cb->channel_id)
      {
      case CHANNEL_L:
         /* show channel text in status window */
         SendDlgItemMessage(hwndMain, IDS_STATUS_WINDOW, SB_SETTEXT, 0, (LPARAM)cb->buf);
         if (is_timer_pending)
            KillTimer(hwndMain, WIN_TIMER_ID);
         SetTimer(hwndMain, WIN_TIMER_ID, STATUS_CLEAR_TIME, NULL);

         hwndList = GetDlgItem(HWND_CHANNEL, IDC_LOG_LIST);
         break;
      case CHANNEL_E:
         hwndList = GetDlgItem(HWND_CHANNEL, IDC_ERROR_LIST);
         break;
      case CHANNEL_A:
         hwndList = NULL;
         break;
      case CHANNEL_G:
         hwndList = GetDlgItem(HWND_CHANNEL, IDC_GODLOG_LIST);
         break;
      default:
         hwndList = GetDlgItem(HWND_CHANNEL, IDC_DEBUG_LIST);
         break;
      }

      if (hwndList)
      {
         SendMessage(hwndList, WM_SETREDRAW, FALSE, 0);
         num_items = ListBox_GetCount(hwndList);
         if (num_items >= CHANNEL_INTERFACE_LINES)
            ListBox_DeleteString(hwndList, 0);
         ListBox_AddString(hwndList, cb->buf);
         ListBox_SetTopIndex(hwndList, std::max(0, num_items - 1));
         SendMessage(hwndList, WM_SETREDRAW, TRUE, 0);
      }
      DoneChannelBuffer();
   }

   UpdateWindow(GetDlgItem(HWND_CHANNEL, IDC_LOG_LIST));
   UpdateWindow(GetDlgItem(HWND_CHANNEL, IDC_ERROR_LIST));
   UpdateWindow(GetDlgItem(HWND_CHANNEL, IDC_DEBUG_LIST));
   UpdateWindow(GetDlgItem(HWND_CHANNEL, IDC_GODLOG_LIST));
}

void InterfaceSave()
{
   UINT64 start_time, total_time;

	EnterServerLock();
	
	lprintf("InterfaceSave saving\n");
	
	total_time = GetMilliCount();
	PauseTimers();
	SendBlakodBeginSystemEvent(SYSEVENT_SAVE);
	/* ResetRoomData(); */
	start_time = GetMilliCount();
	GarbageCollect();
	dprintf("GC completed in %ld ms.\n", GetMilliCount() - start_time);
	start_time = GetMilliCount();
	SaveAll();
	dprintf("Save all completed in %ld ms.\n", GetMilliCount() - start_time);
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	SendBlakodEndSystemEvent(SYSEVENT_SAVE);
	UnpauseTimers();
	
	dprintf("Total interface save time %ld ms.\n", GetMilliCount() - total_time);
	LeaveServerLock();
}

void InterfaceReloadSystem()
{
	EnterServerLock();
	
	lprintf("InterfaceReloadSystem reloading system\n");
	
	PauseTimers();
	
	SendBlakodBeginSystemEvent(SYSEVENT_RELOAD_SYSTEM);
	
	GarbageCollect();
	SaveAll();
	
	// Reload game data.
	MainReloadGameData();
	
	/* can't reload accounts because sessions have pointers to accounts */
	if (!LoadAllButAccount()) 
		eprintf("InterfaceReloadSystem couldn't load game.  You are dead.\n");
	
	AllocateParseClientListNodes(); /* it needs a list to send to users */
	AddBuiltInDLlist();
	
	SendBlakodEndSystemEvent(SYSEVENT_RELOAD_SYSTEM);
	
	UnpauseTimers();
	
	LeaveServerLock();
}

/* stolen from client, 9/19/95
*
* CenterWindow: Center one window over another.  Also ensure that the
*   centered window (hwnd) is completely on the screen.  
*   Call when processing the WM_INITDIALOG message of dialogs, or
*   WM_CREATE in WndProcs. 
*/
void CenterWindow(HWND hwnd, HWND hwndParent)
{
	RECT rcDlg, rcParent;
	int screen_width, screen_height, x, y;
	
	/* If dialog has no parent, then its parent is really the desktop */
	if (hwndParent == NULL)
		hwndParent = GetDesktopWindow();
	
	GetWindowRect(hwndParent, &rcParent);
	GetWindowRect(hwnd, &rcDlg);
	
	/* Move dialog rectangle to upper left (0, 0) for ease of calculation */
	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
	
	x = rcParent.left + (rcParent.right - rcParent.left)/2 - rcDlg.right/2;
	y = rcParent.top + (rcParent.bottom - rcParent.top)/2 - rcDlg.bottom/2;
	
	
	/* Make sure that child window is completely on the screen */
	screen_width  = GetSystemMetrics(SM_CXSCREEN);
	screen_height = GetSystemMetrics(SM_CYSCREEN);
	
	x = std::max(0, std::min(x, (int) (screen_width  - rcDlg.right)));
   y = std::max(0, std::min(y, (int) (screen_height - rcDlg.bottom)));
	
	SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

/*
 * RegCallbackGetIntData: Get the data from an integer editbox field.
 *   Returns TRUE if successful, else FALSE. Displays an error box
 *   to the user if something goes wrong.
 */
BOOL RegCallbackGetIntData(HWND hwnd, int *retval, int dlg_field, char *errormsg, int min, int max)
{
   BOOL dResult;
   if (!errormsg)
   {
      MessageBox(hwnd, "Missing error message in RegCallbackGetData\n",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   *retval = GetDlgItemInt(hwnd, dlg_field, &dResult, FALSE);
   if (!dResult)
   {
      MessageBox(hwnd, errormsg, BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   
   if (*retval < min || *retval > max)
   {
      MessageBox(hwnd, errormsg, BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   return dResult;
}

/*
 * RegCallbackVerifyDate: Verify that the date/time in a blakod callback
 *   structure is valid and in the future. Returns TRUE for success, does
 *   not display its own error box.
 */
BOOL RegCallbackVerifyDate(blakod_reg_callback *b)
{
   tm time;
   time.tm_year = b->year - 1900; // tm year is from 1900
   time.tm_mon = b->month - 1; // tm month is 0-11
   time.tm_mday = b->day;
   time.tm_hour = b->hour;
   time.tm_min = b->minute;
   time.tm_sec = b->second;
   time_t epoch = mktime(&time);

   if (epoch == -1)
      return FALSE;

   return epoch > (time_t)GetTime();
}

/*
 * RegCallbackStrToKodInt: Converts a string received from an editbox
 *   to a kod int, and places it in the given val_type structure. Checks
 *   that the number is within valid kod int bounds. Returns TRUE for
 *   success, and displays an error box if something goes wrong.
 */
BOOL RegCallbackStrToKodInt(HWND hwnd, char *str, val_type *v)
{
   if (!str)
   {
      MessageBox(hwnd, "RegCallbackStrToKodInt received no data!",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   long num = strtol(str, NULL, 10);
   if (num > MAX_KOD_INT)
   {
      MessageBox(hwnd, "Got parameter over max kod int (134,217,727)",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   if (num < -MIN_KOD_INT)
   {
      MessageBox(hwnd, "Got parameter less than min kod int (-134,217,728)",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   v->v.data = (int)num;
   return TRUE;
}

/*
 * RegCallbackGetParmData: Gets the data for a given dialog/type box combo.
 *   Verifies the data is valid kod data and places it in the given val_type
 *   structure. Returns TRUE for success, FALSE for failure. Displays an error
 *   box if something goes wrong.
 */
BOOL RegCallbackGetParmData(HWND hwnd, val_type *v, int dlg_field, int type_field)
{
   char dlg_data[64], type_data[32];

   // Get data editbox field.
   Edit_GetText(GetDlgItem(hwnd, dlg_field), dlg_data, sizeof(dlg_data) - 1);

   // Get type from combobox.
   int index = ComboBox_GetCurSel(GetDlgItem(hwnd, type_field));
   if (index == CB_ERR)
   {
      MessageBox(hwnd, "Error getting type data in RegCallbackGetParmData",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   int result = ComboBox_GetLBText(GetDlgItem(hwnd, type_field), index, type_data);
   if (result == CB_ERR)
   {
      MessageBox(hwnd, "Error in type parameter in RegCallbackGetParmData",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   v->v.tag = GetTagNum(type_data);
   if (v->v.tag == INVALID_TAG)
   {
      MessageBox(hwnd, "Error getting tag for type parm in RegCallbackGetParmData",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   // Decide what to do with the data from editbox.
   if (v->v.tag == TAG_NIL)
   {
      v->v.data = 0;
      return TRUE;
   }
   if (v->v.tag == TAG_CLASS)
   {
      // Got class name, get ID.
      v->v.data = GetClassIDByName(dlg_data);
      if (v->v.data == INVALID_CLASS)
      {
         MessageBox(hwnd, "Invalid class ID in parameter field",
            BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
         return FALSE;
      }
      return TRUE;
   }

   // Other parameters use kod int data.
   if (!RegCallbackStrToKodInt(hwnd, dlg_data, v))
   {
      // Error message already handled.
      return FALSE;
   }

   if (v->v.tag == TAG_OBJECT && !IsObjectByID(v->v.data))
   {
      MessageBox(hwnd, "Invalid object ID in parameter field",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   else if (v->v.tag == TAG_RESOURCE && !IsResourceByID(v->v.data))
   {
      MessageBox(hwnd, "Invalid resource ID in parameter field",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   else if (v->v.tag == TAG_LIST && !IsListNodeByID(v->v.data))
   {
      MessageBox(hwnd, "Invalid list ID in parameter field",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }
   else if (v->v.tag == TAG_STRING && !IsStringByID(v->v.data))
   {
      MessageBox(hwnd, "Invalid string ID in parameter field",
         BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
      return FALSE;
   }

   return TRUE;
}

/*
* RegCallbackSetupComboBox: Set the strings and initial selection for
*   the parm type comboboxes.
*/
void RegCallbackSetupComboBox(HWND hwnd, int combo_id)
{
   HWND combo_hwnd = GetDlgItem(hwnd, combo_id);

   if (!combo_hwnd)
      return;

   ComboBox_ResetContent(combo_hwnd);
   ComboBox_AddString(combo_hwnd, "Nil");
   ComboBox_AddString(combo_hwnd, "Class Name");
   ComboBox_AddString(combo_hwnd, "Integer");
   ComboBox_AddString(combo_hwnd, "Object ID");
   ComboBox_AddString(combo_hwnd, "List ID");
   ComboBox_AddString(combo_hwnd, "Resource ID");
   ComboBox_AddString(combo_hwnd, "String ID");
   ComboBox_SetCurSel(combo_hwnd, 0);
   EnableWindow(combo_hwnd, TRUE);
}

BOOL CALLBACK InterfaceDialogRegCallback(HWND hwnd, UINT message, UINT wParam, LONG lParam)
{
   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hwnd, NULL);

      // Limit entry amount.
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTOBJECTID), 8);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTMESSAGE), 32);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTYEAR), 4);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTMONTH), 2);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTDAY), 2);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTHOUR), 2);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTMINUTE), 2);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTSECOND), 2);
      // Parms limited to 64 chars
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTPARM1), 64);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTPARM2), 64);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTPARM3), 64);
      Edit_LimitText(GetDlgItem(hwnd, IDC_INPUTPARM4), 64);

      // Set defaults.
      SetDlgItemInt(hwnd, IDC_INPUTOBJECTID, 0, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTYEAR, 2017, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTMONTH, 0, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTDAY, 0, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTHOUR, 0, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTMINUTE, 0, FALSE);
      SetDlgItemInt(hwnd, IDC_INPUTSECOND, 0, FALSE);
      Edit_SetText(GetDlgItem(hwnd, IDC_INPUTPARM1), "0");
      Edit_SetText(GetDlgItem(hwnd, IDC_INPUTPARM2), "0");
      Edit_SetText(GetDlgItem(hwnd, IDC_INPUTPARM3), "0");
      Edit_SetText(GetDlgItem(hwnd, IDC_INPUTPARM4), "0");

      // Since this will probably be most common.
      Edit_SetText(GetDlgItem(hwnd, IDC_INPUTMESSAGE), "StartChaosNight");

      // Combobox text.
      RegCallbackSetupComboBox(hwnd, IDC_INPUTPARM1TYPE);
      RegCallbackSetupComboBox(hwnd, IDC_INPUTPARM2TYPE);
      RegCallbackSetupComboBox(hwnd, IDC_INPUTPARM3TYPE);
      RegCallbackSetupComboBox(hwnd, IDC_INPUTPARM4TYPE);

      return TRUE;

   case WM_COMMAND:
      switch (wParam)
      {
      case IDOK:
         blakod_reg_callback reg;
         char msg_name[64];

         // Get time/date and verify sane data (RegCallbackGetData handles error message box).
         if (!RegCallbackGetIntData(hwnd, &reg.year, IDC_INPUTYEAR, "Invalid data in year field!", 2017, 9999))
            return TRUE;
         if (!RegCallbackGetIntData(hwnd, &reg.month, IDC_INPUTMONTH, "Invalid data in month field!", 1, 12))
            return TRUE;
         if (!RegCallbackGetIntData(hwnd, &reg.day, IDC_INPUTDAY, "Invalid data in day field!", 1, 31))
            return TRUE;
         if (!RegCallbackGetIntData(hwnd, &reg.hour, IDC_INPUTHOUR, "Invalid data in hour field!", 0, 23))
            return TRUE;
         if (!RegCallbackGetIntData(hwnd, &reg.minute, IDC_INPUTMINUTE, "Invalid data in minute field!", 0, 59))
            return TRUE;
         if (!RegCallbackGetIntData(hwnd, &reg.second, IDC_INPUTSECOND, "Invalid data in second field!", 0, 59))
            return TRUE;

         // Verify date is a valid future date and not in the past.
         if (!RegCallbackVerifyDate(&reg))
         {
            MessageBox(hwnd, "Got invalid date, or date in the past!",
               BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
         }

         // Get object ID and verify it is a valid object.
         if (!RegCallbackGetIntData(hwnd, &reg.object_id, IDC_INPUTOBJECTID,
            "Invalid data in object ID field!", 0, MAX_KOD_INT))
         {
            return TRUE;
         }
         if (!IsObjectByID(reg.object_id))
         {
            MessageBox(hwnd, "Cannot find object for given object ID!",
               BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
         }

         // Get message name and verify it is a valid message.
         Edit_GetText(GetDlgItem(hwnd, IDC_INPUTMESSAGE), msg_name, sizeof(msg_name) - 1);
         reg.message_id = GetIDByName(msg_name);
         if (reg.message_id == INVALID_ID)
         {
            MessageBox(hwnd, "Cannot find ID for given message name!",
               BlakServNameString(), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
         }

         // Optional parameters to the message to call (set to NIL by default).
         if (!RegCallbackGetParmData(hwnd, &reg.parm1, IDC_INPUTPARM1, IDC_INPUTPARM1TYPE))
         {
            // Error message handled in RegCallbackGetParmData.
            return TRUE;
         }
         if (!RegCallbackGetParmData(hwnd, &reg.parm2, IDC_INPUTPARM2, IDC_INPUTPARM2TYPE))
         {
            // Error message handled in RegCallbackGetParmData.
            return TRUE;
         }
         if (!RegCallbackGetParmData(hwnd, &reg.parm3, IDC_INPUTPARM3, IDC_INPUTPARM3TYPE))
         {
            // Error message handled in RegCallbackGetParmData.
            return TRUE;
         }
         if (!RegCallbackGetParmData(hwnd, &reg.parm4, IDC_INPUTPARM4, IDC_INPUTPARM4TYPE))
         {
            // Error message handled in RegCallbackGetParmData.
            return TRUE;
         }

         // Success! Log it.
         gprintf("Registering callback of %s to obj %i, to activate %.2i:%i:%.2i on %i %s %i\n",
            msg_name, reg.object_id, reg.hour, reg.minute, reg.second, reg.day,
            GetShortMonthStr(reg.month),reg.year);

         EnterServerLock();
         SendBlakodRegisterCallback(&reg);
         LeaveServerLock();

         EndDialog(hwnd, 0);
         return TRUE;

      case IDCANCEL:
         EndDialog(hwnd, 0);
         return TRUE;

      case IDHELP:
         MessageBox(hwnd,
            "Enter a date in the future to send the entered message name to the entered "
            "object ID (look the ID up beforehand).\n\nParameters are optional, and "
            "correspond to the literal 'parm1', 'parm2' etc names in kod message headers."
            "\n\nClass names can be entered as parameters, but all other data must be "
            "numbers (integer, or an ID).\n\nTo start a 1 hour frenzy, use the message "
            "StartChaosNight, object ID 0 and set the time to whenever you'd like the "
            "frenzy to start.",
            BlakServNameString(), MB_OK | MB_ICONQUESTION);
         return TRUE;
      }
   }
   return FALSE;
}

BOOL CALLBACK InterfaceDialogMotd(HWND hwnd,UINT message,UINT wParam,LONG lParam)
{
	char s[2000];
	
	switch (message)
	{
	case WM_INITDIALOG :
		CenterWindow(hwnd,NULL);
		
		Edit_LimitText(GetDlgItem(hwnd,IDC_MOTD),sizeof(s)-1);
		
		EnterServerLock();
		Edit_SetText(GetDlgItem(hwnd,IDC_MOTD),GetMotd());
		LeaveServerLock();
		
		return TRUE;
		
	case WM_COMMAND :
		switch (wParam)
		{
		case IDOK :
			Edit_GetText(GetDlgItem(hwnd,IDC_MOTD),s,sizeof(s)-1);
			
			EnterServerLock();
			SetMotd(s);
			LeaveServerLock();
			
			EndDialog(hwnd,0);
			return TRUE;
			
		case IDCANCEL :
			EndDialog(hwnd,0);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CALLBACK InterfaceDialogAbout(HWND hwnd,UINT message,UINT wParam,LONG lParam)
{
	
	switch (message)
	{
	case WM_INITDIALOG :
		is_about = True;
		
		SetWindowPos(hwnd,HWND_TOP,7,54,0,0,SWP_NOSIZE);
		SetWindowText(GetDlgItem(hwnd,IDC_ABOUT_TITLE),BlakServLongVersionString());
		return TRUE;
		
	case WM_COMMAND :
		switch (wParam)
		{
		case IDOK :
		case IDCANCEL :
			InterfaceTabChange();
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CALLBACK InterfaceDialogTabPage(HWND hwnd,UINT message,UINT wParam,LONG lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SetWindowPos(hwnd,HWND_TOP,7,24,0,0,SWP_NOSIZE);
		return TRUE;
		
		HANDLE_MSG(hwnd,WM_COMMAND,InterfaceTabPageCommand);
		
	}
	return FALSE;
}

void InterfaceTabPageCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
   char s[4096];

   switch (codeNotify)
   {
   case LBN_DBLCLK:
      if (id == IDC_LOG_LIST || id == IDC_ERROR_LIST || id == IDC_DEBUG_LIST || id == IDC_GODLOG_LIST)
      {
         int len = ListBox_GetTextLen(hwndCtl, ListBox_GetCurSel(hwndCtl));
         if (len >= 4096)
         {
            MessageBox(hwndMain, "Selected item too long to display!\n",
               BlakServNameString(), MB_OK | MB_ICONINFORMATION);
            break;
         }
         ListBox_GetText(hwndCtl, ListBox_GetCurSel(hwndCtl), s);
         s[4095] = 0;
         MessageBox(hwndMain, s, BlakServNameString(), MB_OK | MB_ICONINFORMATION);
      }
      break;
   }
}

long CALLBACK InterfaceAdminInputProc(HWND hwnd, UINT message, UINT wParam, LONG lParam)
{
	char buf[200];
	
	switch (message)
	{
	case WM_CHAR :
		if (wParam == '\r')
		{
			if (InMainLoop() && !GetQuit())
			{
				/* make sure we've started already, so that session id is valid */
				Edit_SetSel(hwnd,0,-1);
				Edit_GetText(hwnd,buf,sizeof buf);
				cprintf(console_session_id,"%s\n",buf);
				
				EnterServerLock();
				TryAdminCommand(console_session_id,buf);
				LeaveServerLock();
			}
			return 0;
		}
		if (wParam == '\t')
		{
			SetFocus(GetDlgItem(HWND_ADMIN,IDC_ADMIN_RESPONSE));
			return 0;      
		}
		
		
	}
	return CallWindowProc(lpfnDefAdminInputProc,hwnd,message,wParam,lParam);
}

long CALLBACK InterfaceAdminResponseProc(HWND hwnd, UINT message, UINT wParam, LONG lParam)
{
	switch (message)
	{
	case WM_CHAR :
		if (wParam == '\t')
		{
			SetFocus(GetDlgItem(HWND_ADMIN,IDC_ADMIN_COMMAND));
			return 0;      
		}
		
	}
	return CallWindowProc(lpfnDefAdminResponseProc,hwnd,message,wParam,lParam);
}

/* called in main server thread */
void InterfaceSendBufferList(buffer_node *blist)
{
	buffer_node *bn;
	
	bn = blist;
	while (bn != NULL)
	{
		InterfaceAddAdminBuffer(bn->buf,bn->len_buf);
		bn = bn->next;
	}
	
	DeleteBufferList(blist);
}

/* called in main server thread */
void InterfaceSendBytes(char *buf,int len_buf)
{
	InterfaceAddAdminBuffer(buf,len_buf);
	
	/* remember, main server thread can't do windows functions (deadlock potential) */
	PostMessage(hwndMain,WM_BLAK_UPDATE_ADMIN,0,0);
}

void InterfaceAddAdminBuffer(char *buf,int len_buf)
{
	if (len_buf > ADMIN_RESPONSE_SIZE)
		len_buf = ADMIN_RESPONSE_SIZE;
	
	if (len_admin_response_buf + len_buf > ADMIN_RESPONSE_SIZE)
	{
		len_admin_response_buf = 0;
	}
	memcpy(admin_response_buf+len_admin_response_buf,buf,len_buf);
	len_admin_response_buf += len_buf;
	admin_response_buf[len_admin_response_buf] = 0;
}

void InterfaceUpdateAdmin()
{
	HWND hwnd;
	hwnd = GetDlgItem(HWND_ADMIN,IDC_ADMIN_RESPONSE);
	
	SetWindowRedraw(hwnd,FALSE);
	
	Edit_SetText(hwnd,admin_response_buf);
   SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	
	SetWindowRedraw(hwnd,TRUE);
	InvalidateRect(hwnd,NULL,TRUE);
}

void FatalErrorShow(const char *filename,int line,const char *str)
{
	char s[5000];
	
	sprintf(s,"File %s line %i\r\n\r\n%s",filename,line,str);
	MessageBox(hwndMain,s,"Fatal Error",MB_ICONSTOP);
	
	exit(1);
}
