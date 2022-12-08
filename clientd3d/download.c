// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* download.c:  Handle downloading files.  The actual file reading is done in transfer.c.
*/

#include "client.h"

static DownloadInfo *info;  // Info on download

static char download_dir[] = "download";
static char resource_dir[] = "resource";
static char help_dir[] = "help";
static char mail_dir[] = "mail";
static char run_dir[] = ".";
static char ad_dir[] = "ads";

static HWND hDownloadDialog = NULL;   /* Non-NULL if download dialog is up */

static Bool abort_download = FALSE;   // True if user aborts download
static Bool advert = FALSE;           // True if user aborts download
bool download_in_progress = false;
bool retry_download = false;

static int total = 0;
static char format[256];

static char update_program[] = "club.exe";  // Program to run to update the client executable

#define PING_DELAY 30000       // # of milliseconds between pings to server
static int  timer_id;          // Timer for sending pings to server during download (0 if none)

HANDLE hThread = NULL;         // Handle of transfer thread

/* local function prototypes */
static BOOL CALLBACK DownloadDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
static BOOL CALLBACK AskDownloadDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
static void _cdecl TransferMessage(char *fmt, ...);
static void AbortDownloadDialog(void);
static Bool DownloadDone(DownloadFileInfo *file_info);

static void DownloadUpdater(DownloadInfo *params);
static void DownloadUpdaterFile(DownloadInfo *params);
/*****************************************************************************/
bool FileExists(const char *filename)
{
   struct stat buffer;
   return stat(filename, &buffer) == 0;
}
/*****************************************************************************/
/*
 * DownloadFiles:  Bring up download dialog.
 */
void DownloadFiles(DownloadInfo *params)
{
   int retval, dialog, i;

   info = params;
   info->current_file = 0;
   MainSetState(STATE_DOWNLOAD);

   debug(("machine = %s\n", info->machine));
   debug(("path = %s\n", info->path));
   for (i = 0; i < info->num_files; i++)
      debug(("file = %s, time = %d, flags = %d\n",
      info->files[i].filename, info->files[i].time, info->files[i].flags));

   // If downloading only advertisements, show a different dialog to avoid the appearance
   // of a "real" download.
   dialog = IDD_DOWNLOADAD;
   for (i = 0; i < info->num_files; i++)
      if (DownloadLocation(info->files[i].flags) != DF_ADVERTISEMENT)
      {
         dialog = IDD_DOWNLOAD;
         break;
      }

   advert = (IDD_DOWNLOADAD == dialog);
   if (!advert  && !config.avoidDownloadAskDialog)
   {
      retval = DialogBox(hInst, MAKEINTRESOURCE(IDD_ASKDOWNLOAD), NULL, AskDownloadDialogProc);
      switch (retval) {
      case 3: // do the demo button
         WebLaunchBrowser(info->demoPath);
         SendMessage(hMain, WM_SYSCOMMAND, SC_CLOSE, 0);
         return;
      case IDOK: // proceed with download
         retval = DialogBox(hInst, MAKEINTRESOURCE(dialog), NULL, DownloadDialogProc);
         advert = FALSE;
         if (retval == IDOK)
         {
            MainSetState(STATE_LOGIN);
            i = (((MAJOR_REV * 100) + MINOR_REV) * P_CATCH) + P_CATCH;
            RequestGame(config.download_time, i, config.comm.hostname);
            return;
         }
         break;
      case IDCANCEL: // cancel
      default:
         break;
      }
   }
   abort_download = True;
   config.quickstart = FALSE;
   //MainSetState(STATE_OFFLINE);
   Logoff();
   ShowWindow(hMain, SW_SHOW);
   UpdateWindow(hMain);

#if 0
   // If we were hung up, just leave
   if (state != STATE_DOWNLOAD)
      return;

   MainSetState(STATE_LOGIN);

   switch (retval)
   {
   case IDOK:
      RequestGame(config.download_time);
      break;

   case IDCANCEL:
      Logoff();
      break;
   }
#endif
}
/*****************************************************************************/
/*
 * DownloadCheckDirs:  Make sure that directories needed for downloading exist.
 *   hParent is parent for error dialog.
 *   Return True iff they all exist.
 */
Bool DownloadCheckDirs(HWND hParent)
{
   // Make sure that necessary subdirectories exist
   if (MakeDirectory(download_dir) == False)
   {
      ClientError(hInst, hMain, IDS_CANTMAKEDIR, download_dir, GetLastErrorStr());
      return False;
   }
   if (MakeDirectory(resource_dir) == False)
   {
      ClientError(hInst, hMain, IDS_CANTMAKEDIR, resource_dir, GetLastErrorStr());
      return False;
   }
   if (MakeDirectory(help_dir) == False)
   {
      ClientError(hInst, hMain, IDS_CANTMAKEDIR, help_dir, GetLastErrorStr());
      return False;
   }
   if (MakeDirectory(mail_dir) == False)
   {
      ClientError(hInst, hMain, IDS_CANTMAKEDIR, mail_dir, GetLastErrorStr());
      return False;
   }
   if (MakeDirectory(ad_dir) == False)
   {
      ClientError(hInst, hMain, IDS_CANTMAKEDIR, ad_dir, GetLastErrorStr());
      return False;
   }
   return True;
}

BOOL CALLBACK AskDownloadDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   char buffer[256];
   int i, size;
   double bytes, kb, mb;

   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hDlg, hMain);
      ShowWindow(hMain, SW_HIDE);
      hDownloadDialog = hDlg;
      SetWindowText(GetDlgItem(hDlg, IDC_ASK_DOWNLOAD_REASON), info->reason);
      size = 0;
      for (i = 0; i < (int)info->num_files; i++)
         size += info->files[i].size;

      bytes = (double)size;
      kb = bytes / 1024;
      mb = kb / 1024;

      if ((int)info->num_files < 2)
         sprintf(buffer, GetString(hInst, IDC_SIZE_UPDATE_ONE_FILE), size);
      else if (kb < 1.0)
         sprintf(buffer, GetString(hInst, IDC_SIZE_UPDATE_FILES_BYTES), (int)info->num_files, size);
      else if (mb < 1.0)
         sprintf(buffer, GetString(hInst, IDC_SIZE_UPDATE_FILES_KB), (int)info->num_files, kb);
      else
         sprintf(buffer, GetString(hInst, IDC_SIZE_UPDATE_FILES_MB), (int)info->num_files, mb);
      SetWindowText(GetDlgItem(hDlg, IDC_SIZE_UPDATE), buffer);

      //SetWindowText(GetDlgItem(hDlg,IDC_BTN_DEMO),info->demoPath);

      break;
   case WM_COMMAND:
      switch (GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDC_BTN_DEMO:
         EndDialog(hDlg, 3);
         return TRUE;
      case IDCANCEL:
         abort_download = True;
         EndDialog(hDlg, IDCANCEL);
         return TRUE;
      case IDC_UPDATE:
         EndDialog(hDlg, IDOK);
         return TRUE;
      }
   }
   return FALSE;
}
/*****************************************************************************/
/*
 * DownloadDialogProc:  Dialog procedure for displaying downloading progress.
 */
BOOL CALLBACK DownloadDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   int fraction;
   HWND hGraph;
   BOOL bResult = FALSE;
   char temp[256];

   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hDlg, hMain);
      if (!advert)
      {
         ShowWindow(hMain, SW_HIDE);
      }
      hDownloadDialog = hDlg;

      // Set up graph bar limits
      hGraph = GetDlgItem(hDlg, IDC_GRAPH);
      SendMessage(hGraph, GRPH_RANGESET, 0, 100);
      SendMessage(hGraph, GRPH_POSSET, 0, 0);
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR1));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR2));

      hGraph = GetDlgItem(hDlg, IDC_FILEGRAPH);
      SendMessage(hGraph, GRPH_RANGESET, 0, 100);
      SendMessage(hGraph, GRPH_POSSET, 0, 0);
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR1));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR2));

      hGraph = GetDlgItem(hDlg, IDC_ANIMATE1);
      bResult = Animate_Open(hGraph, MAKEINTRESOURCE(IDA_DOWNLOAD));

      abort_download = False;
      PostMessage(hDlg, BK_TRANSFERSTART, 0, 0);

      GetDlgItemText(hDlg, IDC_FILESIZE, format, sizeof(format));
      sprintf(temp, format, (int)0, (int)0);
      SetDlgItemText(hDlg, IDC_FILESIZE, temp);

      return TRUE;

   case WM_DESTROY:
      if (!advert)
      {
         ShowWindow(hMain, SW_SHOW);
         UpdateWindow(hMain);
      }
      hDownloadDialog = NULL;
      return TRUE;

   case BK_TRANSFERSTART:
      info->hPostWnd = hDlg;
      hThread = (HANDLE)(unsigned long)_beginthread(TransferStart, 0, info);
      TransferMessage(GetString(hInst, IDS_CONNECTING), info->machine);
      return TRUE;

   case BK_FILESIZE:  // wParam is file index, lParam is file size
      if (wParam == 0)
         TransferMessage(GetString(hInst, IDS_RETRIEVING));

      SetDlgItemText(hDlg, IDC_FILENAME, info->files[wParam].filename);
      total = lParam;
      sprintf(temp, format, 0, total);
      SetDlgItemText(hDlg, IDC_FILESIZE, temp);
      SendDlgItemMessage(hDlg, IDC_GRAPH, GRPH_POSSET, 0, 0);
      SendDlgItemMessage(hDlg, IDC_GRAPH, GRPH_RANGESET, 0, total);
      return TRUE;

   case BK_PROGRESS:

      // Update this file's progress indicator.
      SendDlgItemMessage(hDlg, IDC_GRAPH, GRPH_POSSET, 0, lParam);

      // Update this file's progress text message.
      sprintf(temp, format, (int)lParam, (int)total);
      SetDlgItemText(hDlg, IDC_FILESIZE, temp);

      // Compute the fraction for the overall graph.
      fraction = 0;
      if (total != 0)
         fraction = lParam * 100 / total;
      fraction = (fraction + 100 * info->current_file) / info->num_files;

      // Update overall progress indicator.
      SendDlgItemMessage(hDlg, IDC_FILEGRAPH, GRPH_POSSET, 0, fraction);

      return TRUE;

   case BK_FILEDONE:  /* lParam is index of file in info */
      if (abort_download)
      {
         AbortDownloadDialog();
         return TRUE;
      }

      if (DownloadDone(&info->files[lParam]))
      {
         if (abort_download)
         {
            AbortDownloadDialog();
            return TRUE;
         }

         // Set download time
         DownloadSetTime(info->files[lParam].time);

         info->current_file++;

         // Tell transfer thread to continue
         TransferContinue();

         TransferMessage(GetString(hInst, IDS_RETRIEVING));
      }
      else AbortDownloadDialog();
      return TRUE;

   case BK_TRANSFERDONE:
      EndDialog(hDlg, IDOK);
      return TRUE;

   case WM_COMMAND:
      switch (GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDCANCEL:
         abort_download = True;
         EndDialog(hDlg, IDCANCEL);
         return TRUE;
      }
   }
   return FALSE;
}
/*****************************************************************************/
/*
 * TransferMessage:  Display given message in download dialog.
 */
void _cdecl TransferMessage(char *fmt, ...)
{
   char s[200];
   va_list marker;

   if (hDownloadDialog == NULL)
      return;

   va_start(marker, fmt);
   vsprintf(s, fmt, marker);
   va_end(marker);

   SetDlgItemText(hDownloadDialog, IDC_MESSAGE, s);
}
/*****************************************************************************/
/*
 * DownloadSetTime:  Save the given time as the time of the last successful download.
 */
void DownloadSetTime(int new_time)
{
   TimeSettingsSave(new_time);
}
/*****************************************************************************/
/*
 * AbortDownloadDialog:  Kill off download dialog
 */
void AbortDownloadDialog(void)
{
   if (hDownloadDialog != NULL)
      SendMessage(hDownloadDialog, WM_COMMAND, IDCANCEL, 0);
   TransferAbort();
}
/*****************************************************************************/
/*
 * DownloadDone:  Unpack archive and move downloaded files into correct directory,
 *   if appropriate.
 *   zip_filename is the name of the file on the disk.
 *   file_info gives info about downloaded file.
 *   Return True iff successful.
 */
Bool DownloadDone(DownloadFileInfo *file_info)
{
   download_in_progress = false;

   debug(("Got file successfully\n"));

   return True;
}
/*****************************************************************************/
/*
 * DownloadPingProc:  In response to a timer going off, send a ping message to the server.
 */
void CALLBACK DownloadPingProc(HWND hwnd, UINT msg, UINT timer, DWORD dwTime)
{
   RequestLoginPing();
}
/*****************************************************************************/
/*
 * DownloadInit:  Enter STATE_DOWNLOAD.
 */
void DownloadInit(void)
{
   TransferInit();

   // Start ping timer
   timer_id = SetTimer(NULL, 0, PING_DELAY, DownloadPingProc);
}
/*****************************************************************************/
/*
 * DownloadExit:  Leave STATE_DOWNLOAD.
 */
void DownloadExit(void)
{
   AbortDownloadDialog();
   TransferClose();

   // Free structure from server message that started download
   SafeFree(info->files);
   SafeFree(info);
   info = NULL;

   // Kill ping timer
   if (timer_id != 0)
   {
      KillTimer(NULL, timer_id);
      timer_id = 0;
   }
}
/*****************************************************************************/
/*
 * DownloadClientPatch:  Got version mismatch with the server, so we need to
 *   update. The client downloads a new version of the updater (given in
 *   clubfile), saves the update info and its own filename into a text file
 *   then runs the updater with the location of the text file.
 */
void DownloadClientPatch(char *patchhost, char *patchpath, char *patchcachepath,
                         char *cachefile, char *clubfile, char *reason)
{
   FILE *fp;
   SHELLEXECUTEINFO shExecInfo;
   char dl_info[MAX_CMDLINE];
   char exe_name[MAX_PATH];
   char client_directory[MAX_PATH];
   char update_program_path[MAX_PATH];
   char *ptr;

   if (AreYouSure(hInst, hMain, YES_BUTTON, IDS_NEEDNEWVERSION))
   {
      // Make download dir if not already there
      DownloadCheckDirs(hMain);

      // Destination directory is wherever client executable is running.
      GetModuleFileName(NULL, exe_name, MAX_PATH);
      strcpy(client_directory, exe_name);
      ptr = strrchr(client_directory, '\\');
      if (ptr != NULL)
         *ptr = 0;

      DownloadInfo *dinfo;
      dinfo = (DownloadInfo *)ZeroSafeMalloc(sizeof(DownloadInfo));
      dinfo->files = (DownloadFileInfo *)ZeroSafeMalloc(sizeof(DownloadFileInfo));
      dinfo->num_files = 1;
      strcpy(dinfo->machine, patchhost); // Host machine to connect to.
      strcpy(dinfo->path, patchpath); // Path to get updater.
      strcpy(dinfo->reason, reason); // Download reason.
      strcpy(dinfo->files[0].filename, clubfile); // Updater filename.
      strcpy(dinfo->files[0].path, client_directory); // Download to client dir.
      dinfo->files[0].size = 0; // Size is updated when file download starts.
      dinfo->files[0].flags = 0; // Saved to client dir, no flags.
      DownloadUpdater(dinfo);

      // Create strings for updater program and arguments (download info).
      // Save download info to a file so club can restart a failed download,
      // but also use these to start the program (in case the file can't be saved).

      sprintf(update_program_path, "%s\\%s", client_directory, update_program);
      sprintf(dl_info, "\"%s\" UPDATE \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
         exe_name, patchhost, patchpath, patchcachepath, cachefile,
         client_directory);

      // Save download info to a file.
      fp = fopen("dlinfo.txt", "w");
      if (fp)
      {
         fwrite(dl_info, strlen(dl_info), 1, fp);
         fclose(fp);
      }

      shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
      shExecInfo.fMask = 0;
      shExecInfo.hwnd = NULL;
      shExecInfo.lpVerb = "runas";
      shExecInfo.lpFile = update_program_path;
      // Run in parent of resource directory; club will take care of copying
      // exes to Program Files if necessary.
      // Running club with no parameters will cause it to search for dlinfo.txt.
      // Use parameters here in case the file couldn't be saved.
      shExecInfo.lpParameters = dl_info;
      shExecInfo.lpDirectory = NULL;
      shExecInfo.nShow = SW_NORMAL;
      shExecInfo.hInstApp = NULL;
      if (!ShellExecuteEx(&shExecInfo))
         ClientError(hInst, hMain, IDS_CANTUPDATE, update_program);
   }

   // Quit client
   PostMessage(hMain, WM_DESTROY, 0, 0);
}

void DownloadUpdater(DownloadInfo *params)
{
   DownloadUpdaterFile(params);
   while (download_in_progress)
   {
   }
}

void DownloadUpdaterFile(DownloadInfo *params)
{
   int retval;

   info = params;
   info->current_file = 0;
   MainSetState(STATE_DOWNLOAD);

   download_in_progress = true;
   retval = DialogBox(hInst, MAKEINTRESOURCE(IDD_DOWNLOAD), NULL, DownloadDialogProc);
   if (retval == IDOK)
      return;

   download_in_progress = false;
   abort_download = True;
   config.quickstart = FALSE;
   //MainSetState(STATE_OFFLINE);
   Logoff();
   ShowWindow(hMain, SW_SHOW);
   UpdateWindow(hMain);
}
