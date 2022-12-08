// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * maindlg.c:  Main menu dialogs, not associated with the game.
 */

#include "client.h"

#define MINPASSWORD 6       // Minimum password length

static HWND hPasswdDialog = NULL;
static HWND hPreferencesDialog = NULL;
static HWND hGraphicsDialog = NULL;

BOOL CALLBACK ProfanityDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);

// XXX Would be nice to load this from resource file, but that doesn't seem to be possible
static char EXE_filter[] = "Programs (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0";
/*****************************************************************************/
BOOL CALLBACK PasswordDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   static HWND hOldPasswd, hNewPasswd1, hNewPasswd2;
   char oldpasswd[MAXPASSWORD + 1], newpasswd1[MAXPASSWORD + 1], newpasswd2[MAXPASSWORD + 1];
   char buf1[ENCRYPT_LEN + 1], buf2[ENCRYPT_LEN + 1];

   switch (message)
   {
   case WM_INITDIALOG:
      if (hPasswdDialog)
      {
         DestroyWindow(hDlg);
         return TRUE;
      }

      CenterWindow(hDlg, GetParent(hDlg));

      hOldPasswd = GetDlgItem(hDlg, IDC_OLDPASSWD);
      hNewPasswd1 = GetDlgItem(hDlg, IDC_NEWPASSWD1);
      hNewPasswd2 = GetDlgItem(hDlg, IDC_NEWPASSWD2);

      Edit_LimitText(hOldPasswd, MAXPASSWORD);
      Edit_LimitText(hNewPasswd1, MAXPASSWORD);
      Edit_LimitText(hNewPasswd2, MAXPASSWORD);

      SetWindowFont(hOldPasswd, GetFont(FONT_INPUT), FALSE);
      SetWindowFont(hNewPasswd1, GetFont(FONT_INPUT), FALSE);
      SetWindowFont(hNewPasswd2, GetFont(FONT_INPUT), FALSE);
      hPasswdDialog = hDlg;
      return TRUE;

   case WM_COMMAND:
      switch(GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDOK:
         /* User has pressed return on one of the edit boxes */
         if (GetFocus() == hOldPasswd)
         {
            SetFocus(hNewPasswd1);
            return TRUE;
         }

         if (GetFocus() == hNewPasswd1)
         {
            SetFocus(hNewPasswd2);
            return TRUE;
         }

         if (GetFocus() == hNewPasswd2)
            PostMessage(hDlg, WM_COMMAND, IDC_OK, 0);
         return TRUE;

      case IDC_OK:
         /* Send results to server */
         Edit_GetText(hOldPasswd, oldpasswd, MAXPASSWORD + 1);
         Edit_GetText(hNewPasswd1, newpasswd1, MAXPASSWORD + 1);
         Edit_GetText(hNewPasswd2, newpasswd2, MAXPASSWORD + 1);

         if (0 != strcmp(newpasswd1, newpasswd2))
         {
            ClientError(hInst, hDlg, IDS_PASSWDMATCH);
            return TRUE;
         }

         if (strlen(newpasswd1) < MINPASSWORD)
         {
            ClientError(hInst, hDlg, IDS_PASSWDLENGTH, MINPASSWORD);
            return TRUE;
         }

         // Recall this was the last attempt to change a password.
         // It's just stopping the auto-nagging feature from popping
         // up a dialog box later.
         //
         // The config.password is checked elsewhere, such as by
         // modules that need confirmation for drastic features.
         //
         // To improve the security, we only update these if we *think*
         // they're right about the password.  Not perfect, but we
         // never get word from the server that the password was right.
         //
         if (0 == strcmp(oldpasswd, config.password))
         {
            config.lastPasswordChange = time(NULL);
            strcpy(config.password, newpasswd1);
         }

         // Encrypt old and new passwords for the server.
         // It's up to the server to check if we are allowed to change
         // the password, based on the correct old password.
         //
         MDString(oldpasswd, (unsigned char *) buf1);
         buf1[ENCRYPT_LEN] = 0;
         MDString(newpasswd1, (unsigned char *) buf2);
         buf2[ENCRYPT_LEN] = 0;
         RequestChangePassword(buf1, buf2);
         
         hPasswdDialog = NULL;
         EndDialog(hDlg, IDOK);
         return TRUE;

      case IDCANCEL:
         hPasswdDialog = NULL;
         EndDialog(hDlg, IDCANCEL);
         return TRUE;
      }
      break;
   }
   return FALSE;
}
/****************************************************************************/
void AbortPasswordDialog(void)
{
   if (hPasswdDialog != NULL)
      EndDialog(hPasswdDialog, IDCANCEL);
}
/*****************************************************************************/
void AbortPreferencesDialog(void)
{
   if (hPreferencesDialog != NULL)
      EndDialog(hPreferencesDialog, IDCANCEL);
}
/*****************************************************************************/
void AbortGraphicsDialog(void)
{
   if (hGraphicsDialog != NULL)
      EndDialog(hGraphicsDialog, IDCANCEL);
}
/*****************************************************************************/
BOOL CALLBACK PreferencesDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   static HWND hBrowser;
   static Bool browser_changed;
   Bool toolbar_changed, lagbox_changed, fps_changed, temp;
   CommSettings *comm = &config.comm;
   OPENFILENAME ofn;
   static char *dir;   // Working directory before dialog (OpenFile may change it)
   int new_val;

   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hDlg, GetParent(hDlg));
      if (hPreferencesDialog != NULL)
      {
         EndDialog(hDlg, IDCANCEL);
         return FALSE;
      }
      hPreferencesDialog = hDlg;

      hBrowser = GetDlgItem(hDlg, IDC_BROWSER);

      Edit_LimitText(hBrowser, MAX_PATH);

      SetWindowFont(hBrowser, GetFont(FONT_INPUT), FALSE);

      SetWindowText(hBrowser, config.browser);

      CheckDlgButton(hDlg, IDC_SCROLLLOCK, config.scroll_lock);
      CheckDlgButton(hDlg, IDC_DRAWNAMES, config.draw_player_names);
      CheckDlgButton(hDlg, IDC_DRAWNPCNAMES, config.draw_npc_names);
      CheckDlgButton(hDlg, IDC_DRAWSIGNNAMES, config.draw_sign_names);
      CheckDlgButton(hDlg, IDC_TARGETLIGHT, config.target_highlight);
      CheckDlgButton(hDlg, IDC_TOOLTIPS, config.tooltips);
      CheckDlgButton(hDlg, IDC_PAIN, config.pain);
      CheckDlgButton(hDlg, IDC_INVNUM, config.inventory_num);
      CheckDlgButton(hDlg, IDC_SAFETY, config.preferences & CF_SAFETY_OFF);
      CheckDlgButton(hDlg, IDC_TEMPSAFE, config.preferences & CF_TEMPSAFE);
      CheckDlgButton(hDlg, IDC_GROUPING, config.preferences & CF_GROUPING);
      CheckDlgButton(hDlg, IDC_AUTOLOOT, config.preferences & CF_AUTOLOOT);
      CheckDlgButton(hDlg, IDC_AUTOCOMBINE, config.preferences & CF_AUTOCOMBINE);
      CheckDlgButton(hDlg, IDC_REAGENTBAG, config.preferences & CF_REAGENTBAG);
      CheckDlgButton(hDlg, IDC_SPELLPOWER, config.preferences & CF_SPELLPOWER);
      CheckDlgButton(hDlg, IDC_SHOWFPS, config.showFPS);
      CheckDlgButton(hDlg, IDC_BOUNCE, config.bounce);
      CheckDlgButton(hDlg, IDC_WEATHER, config.weather);
      CheckDlgButton(hDlg, IDC_TOOLBAR, config.toolbar);
      CheckDlgButton(hDlg, IDS_LATENCY0, config.lagbox);
      CheckDlgButton(hDlg, IDC_PROFANE, config.antiprofane);
      CheckDlgButton(hDlg, IDC_DRAWMAP, config.drawmap);
      CheckDlgButton(hDlg, IDC_MAP_ANNOTATIONS, config.map_annotations);
      CheckDlgButton(hDlg, IDC_XP_AS_PERCENT, config.xp_display_percent);
      CheckDlgButton(hDlg, IDC_TIMESTAMPS, config.chat_time_stamps);
      CheckDlgButton(hDlg, IDC_MUSIC, config.play_music);
      CheckDlgButton(hDlg, IDC_SOUNDFX, config.play_sound);
      CheckDlgButton(hDlg, IDC_LOOPSOUNDS, config.play_loop_sounds);
      CheckDlgButton(hDlg, IDC_RANDSOUNDS, config.play_random_sounds);

      EnableWindow(GetDlgItem(hDlg, IDC_LOOPSOUNDS), IsDlgButtonChecked(hDlg, IDC_SOUNDFX));
      EnableWindow(GetDlgItem(hDlg, IDC_RANDSOUNDS), IsDlgButtonChecked(hDlg, IDC_SOUNDFX));

      CheckRadioButton(hDlg, IDC_TARGETHALO1, IDC_TARGETHALO3, config.halocolor + IDC_TARGETHALO1);

      CheckDlgButton(hDlg, IDC_COLORCODES, config.colorcodes);

      Trackbar_SetRange(GetDlgItem(hDlg, IDC_PARTICLENUM), 25, CONFIG_MAX_PARTICLES, FALSE);
      Trackbar_SetPos(GetDlgItem(hDlg, IDC_PARTICLENUM), config.particles);

      Trackbar_SetRange(GetDlgItem(hDlg, IDC_SOUND_VOLUME), 0, CONFIG_MAX_VOLUME, FALSE);
      Trackbar_SetRange(GetDlgItem(hDlg, IDC_MUSIC_VOLUME), 0, CONFIG_MAX_VOLUME, FALSE);
      Trackbar_SetPos(GetDlgItem(hDlg, IDC_SOUND_VOLUME), config.sound_volume);
      Trackbar_SetPos(GetDlgItem(hDlg, IDC_MUSIC_VOLUME), config.music_volume);

      dir = (char *) SafeMalloc(MAX_PATH + 1);
      GetWorkingDirectory(dir, MAX_PATH);

      browser_changed = False;
      return TRUE;

   case WM_COMMAND:
      switch(GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDC_BROWSER:
         if (GET_WM_COMMAND_CMD(wParam, lParam) != EN_CHANGE)
            break;

         browser_changed = True;
         return TRUE;

      case IDC_FIND:
         memset(&ofn, 0, sizeof(OPENFILENAME));
         ofn.lStructSize = sizeof(OPENFILENAME);
         ofn.hwndOwner = hDlg;
         ofn.lpstrFilter = EXE_filter;
         ofn.lpstrFile = config.browser;
         ofn.nMaxFile = MAX_PATH;
         ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
         if (GetOpenFileName(&ofn))
         {
            SetWindowText(hBrowser, config.browser);
            browser_changed = True;
         }
         else debug(("GetOpenFileName failed, error = %d\n", CommDlgExtendedError()));
         return TRUE;

      case IDCANCEL:
         EndDialog(hDlg, IDCANCEL);
         return TRUE;

      case IDC_SOUNDFX:
         EnableWindow(GetDlgItem(hDlg, IDC_LOOPSOUNDS), IsDlgButtonChecked(hDlg, IDC_SOUNDFX));
         EnableWindow(GetDlgItem(hDlg, IDC_RANDSOUNDS), IsDlgButtonChecked(hDlg, IDC_SOUNDFX));
         return TRUE;

      case IDC_PROFANESETTINGS:
         if (IDOK == DialogBox(hInst, MAKEINTRESOURCE(IDC_PROFANESETTINGS), hDlg, ProfanityDialogProc))
            CheckDlgButton(hDlg, IDC_PROFANE, TRUE);
         return TRUE;

      case IDOK:
         Edit_GetText(hBrowser, config.browser, MAX_PATH);

         if (browser_changed)
            config.default_browser = False;

         config.scroll_lock       = IsDlgButtonChecked(hDlg, IDC_SCROLLLOCK);
         config.draw_player_names = IsDlgButtonChecked(hDlg, IDC_DRAWNAMES);
         config.draw_npc_names    = IsDlgButtonChecked(hDlg, IDC_DRAWNPCNAMES);
         config.draw_sign_names   = IsDlgButtonChecked(hDlg, IDC_DRAWSIGNNAMES);
         config.target_highlight  = IsDlgButtonChecked(hDlg, IDC_TARGETLIGHT);
         config.tooltips          = IsDlgButtonChecked(hDlg, IDC_TOOLTIPS);
         config.pain              = IsDlgButtonChecked(hDlg, IDC_PAIN);
         config.inventory_num     = IsDlgButtonChecked(hDlg, IDC_INVNUM);

         if (IsDlgButtonChecked(hDlg, IDC_SAFETY))
            config.preferences |= CF_SAFETY_OFF;
         else
            config.preferences &= ~CF_SAFETY_OFF;

         if (IsDlgButtonChecked(hDlg, IDC_TEMPSAFE))
            config.preferences |= CF_TEMPSAFE;
         else
            config.preferences &= ~CF_TEMPSAFE;

         if (IsDlgButtonChecked(hDlg, IDC_GROUPING))
            config.preferences |= CF_GROUPING;
         else
            config.preferences &= ~CF_GROUPING;

         if (IsDlgButtonChecked(hDlg, IDC_AUTOLOOT))
            config.preferences |= CF_AUTOLOOT;
         else
            config.preferences &= ~CF_AUTOLOOT;

         if (IsDlgButtonChecked(hDlg, IDC_AUTOCOMBINE))
            config.preferences |= CF_AUTOCOMBINE;
         else
            config.preferences &= ~CF_AUTOCOMBINE;

         if (IsDlgButtonChecked(hDlg, IDC_REAGENTBAG))
            config.preferences |= CF_REAGENTBAG;
         else
            config.preferences &= ~CF_REAGENTBAG;

         if (IsDlgButtonChecked(hDlg, IDC_SPELLPOWER))
            config.preferences |= CF_SPELLPOWER;
         else
            config.preferences &= ~CF_SPELLPOWER;

         config.bounce        = IsDlgButtonChecked(hDlg, IDC_BOUNCE);
         config.weather       = IsDlgButtonChecked(hDlg, IDC_WEATHER);
         config.antiprofane   = IsDlgButtonChecked(hDlg, IDC_PROFANE);
         config.drawmap         = IsDlgButtonChecked(hDlg, IDC_DRAWMAP);
         config.map_annotations = IsDlgButtonChecked(hDlg, IDC_MAP_ANNOTATIONS);
         config.xp_display_percent = IsDlgButtonChecked(hDlg, IDC_XP_AS_PERCENT);
         config.chat_time_stamps = IsDlgButtonChecked(hDlg, IDC_TIMESTAMPS);

         temp                 = IsDlgButtonChecked(hDlg, IDC_TOOLBAR);
         toolbar_changed = (temp != config.toolbar);
         config.toolbar = temp;
         temp                 = IsDlgButtonChecked(hDlg, IDS_LATENCY0);
         lagbox_changed = (temp != config.lagbox);
         config.lagbox = temp;
         temp                 = IsDlgButtonChecked(hDlg, IDC_SHOWFPS);
         fps_changed = (temp != config.showFPS);
         config.showFPS = temp;

         // Set music volume first, music might be turned off.
         new_val = Trackbar_GetPos(GetDlgItem(hDlg, IDC_MUSIC_VOLUME));
         if (new_val != config.music_volume)
         {
            config.music_volume = new_val;
            MusicSetVolume();
         }

         // Possibly turn music on/off.
         temp = IsDlgButtonChecked(hDlg, IDC_MUSIC);
         if (temp != config.play_music)
         {
            config.play_music = temp;
            UserToggleMusic(config.play_music);
         }

         // Sound effects volume.
         new_val = Trackbar_GetPos(GetDlgItem(hDlg, IDC_SOUND_VOLUME));
         if (new_val != config.sound_volume)
         {
            config.sound_volume = new_val;
            SoundSetVolume();
         }

         // TODO: Update specific sounds based on the loop/random flags.
         // Need to keep track of these flags in audio.c, and also keep track
         // of which sounds should be playing but aren't due to one of these
         // values unset (currently they aren't added at all).
         config.play_sound = IsDlgButtonChecked(hDlg, IDC_SOUNDFX);
         config.play_loop_sounds = IsDlgButtonChecked(hDlg, IDC_LOOPSOUNDS);
         config.play_random_sounds = IsDlgButtonChecked(hDlg, IDC_RANDSOUNDS);
         if (!config.play_sound)
            SoundStopAll();

         new_val = Trackbar_GetPos(GetDlgItem(hDlg, IDC_PARTICLENUM));
         if (new_val != config.particles)
         {
            config.particles = new_val;
            // Reset particle system with new max num.
            D3DParticlesInit(true);
         }

         if( IsDlgButtonChecked( hDlg, IDC_TARGETHALO1 ) == BST_CHECKED )
            config.halocolor = 0;
         else if( IsDlgButtonChecked( hDlg, IDC_TARGETHALO2 ) == BST_CHECKED )
            config.halocolor = 1;
         else
            config.halocolor = 2;

         config.colorcodes = IsDlgButtonChecked(hDlg, IDC_COLORCODES);

         // Redraw main window to reflect new settings
         if (toolbar_changed || lagbox_changed || fps_changed)
         {
            ResizeAll();
         }
         else
         {
            InvalidateRect(hMain, NULL, TRUE);
            RedrawAll();
         }

         EditBoxSetNormalFormat();

         EndDialog(hDlg, IDOK);
         return TRUE;
      }
      break;

   case WM_DESTROY:
      // Restore working drive and directory
      if (chdir(dir) != 0)
         debug(("chdir failed to %s\n", dir));
      SafeFree(dir);
      
      hPreferencesDialog = NULL;
      return TRUE;
   }
   return FALSE;
}
/*****************************************************************************/
BOOL CALLBACK GraphicsDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   char retStr[12];
   retStr[0] = 0;
   HWND hWndComboBox;
   int aaList, index, aaMode;
   Bool temp;
   bool changed = false;

   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hDlg, GetParent(hDlg));
      if (hGraphicsDialog != NULL)
      {
         EndDialog(hDlg, IDCANCEL);

         return FALSE;
      }

      if (!D3DRenderIsEnabled())
      {
         EnableWindow(GetDlgItem(hDlg, IDC_MIPMAPS), FALSE);
         EnableWindow(GetDlgItem(hDlg, IDC_DYNLIGHTS), FALSE);
         EnableWindow(GetDlgItem(hDlg, IDC_WIREFRAME), FALSE);
         EnableWindow(GetDlgItem(hDlg, IDC_AA_TEXT), FALSE);
         EnableWindow(GetDlgItem(hDlg, IDC_ANTI_ALIAS), FALSE);

         return FALSE;
      }

      hGraphicsDialog = hDlg;
      CheckDlgButton(hDlg, IDC_MIPMAPS, config.mipMaps);
      CheckDlgButton(hDlg, IDC_WIREFRAME, config.drawWireframe);
      CheckDlgButton(hDlg, IDC_DYNLIGHTS, config.dynamicLights);
      hWndComboBox = GetDlgItem(hDlg, IDC_ANTI_ALIAS);
      SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"No AA");
      if (config.aaMode == D3DMULTISAMPLE_NONE)
         SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"No AA");

      aaList = D3DGetAvailableAAOptions();

      if (aaList & (1 << D3DMULTISAMPLE_2_SAMPLES))
      {
         SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"2x AA");
         if (config.aaMode == D3DMULTISAMPLE_2_SAMPLES)
            SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"2x AA");
      }
      if (aaList & (1 << D3DMULTISAMPLE_3_SAMPLES))
      {
         SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"3x AA");
         if (config.aaMode == D3DMULTISAMPLE_3_SAMPLES)
            SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"3x AA");
      }
      if (aaList & (1 << D3DMULTISAMPLE_4_SAMPLES))
      {
         SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"4x AA");
         if (config.aaMode == D3DMULTISAMPLE_4_SAMPLES)
            SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"4x AA");
      }
      if (aaList & (1 << D3DMULTISAMPLE_8_SAMPLES))
      {
         SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"8x AA");
         if (config.aaMode == D3DMULTISAMPLE_8_SAMPLES)
            SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"8x AA");
      }
      if (aaList & (1 << D3DMULTISAMPLE_16_SAMPLES))
      {
         SendMessage(hWndComboBox, CB_ADDSTRING, NULL, (LPARAM)"16x AA");
         if (config.aaMode == D3DMULTISAMPLE_16_SAMPLES)
            SendMessage(hWndComboBox, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)"16x AA");
      }

      return TRUE;

   case WM_COMMAND:
      switch (GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDCANCEL:
         EndDialog(hDlg, IDCANCEL);
         return TRUE;
      case IDOK:
         temp = IsDlgButtonChecked(hDlg, IDC_MIPMAPS);
         if (temp != config.mipMaps)
         {
            config.mipMaps = temp;
            changed = true;
         }

         temp = IsDlgButtonChecked(hDlg, IDC_WIREFRAME);
         if (temp != config.drawWireframe)
         {
            config.drawWireframe = temp;
         }

         temp = IsDlgButtonChecked(hDlg, IDC_DYNLIGHTS);
         if (temp != config.dynamicLights)
         {
            config.dynamicLights = temp;
         }

         hWndComboBox = GetDlgItem(hDlg, IDC_ANTI_ALIAS);
         index = SendMessage(hWndComboBox, CB_GETCURSEL, 0, 0);
         if (index != CB_ERR)
         {
            SendMessage(hWndComboBox, CB_GETLBTEXT, index, (LPARAM)retStr);
            if (strcmp(retStr, "2x AA") == 0)
               aaMode = 2;
            else if (strcmp(retStr, "4x AA") == 0)
               aaMode = 4;
            else if (strcmp(retStr, "8x AA") == 0)
               aaMode = 8;
            else if (strcmp(retStr, "16x AA") == 0)
               aaMode = 16;
            else
               aaMode = 0;
            if (config.aaMode != aaMode)
            {
               config.aaMode = aaMode;
               changed = true;
            }
         }
         if (changed)
            D3DRenderReset();

         EndDialog(hDlg, IDOK);

         return TRUE;
      }
      break;

   case WM_DESTROY:
      hGraphicsDialog = NULL;

      return TRUE;
   }

   return FALSE;
}
/*****************************************************************************/
BOOL CALLBACK ProfanityDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   char term[MAXPROFANETERM+1];

   switch (message)
   {
   case WM_INITDIALOG:
      CenterWindow(hDlg, GetParent(hDlg));
      Edit_LimitText(GetDlgItem(hDlg, IDC_EDIT1), MAXPROFANETERM);
      CheckRadioButton(hDlg, IDC_RADIO1, IDC_RADIO2, IDC_RADIO1+!config.ignoreprofane);
      CheckDlgButton(hDlg, IDC_CHECK1, config.extraprofane);
      SetFocus(GetDlgItem(hDlg, IDC_RADIO1+!config.ignoreprofane));
      return FALSE; // return TRUE unless we set the focus

   case WM_COMMAND:
      switch(GET_WM_COMMAND_ID(wParam, lParam))
      {
      case IDC_BUTTON1:
         GetDlgItemText(hDlg, IDC_EDIT1, term, sizeof(term));
         AddProfaneTerm(term);
         SetDlgItemText(hDlg, IDC_EDIT1, "");
         SetFocus(GetDlgItem(hDlg, IDC_EDIT1));
         return TRUE;

      case IDC_BUTTON2:
         GetDlgItemText(hDlg, IDC_EDIT1, term, sizeof(term));
         RemoveProfaneTerm(term);
         SetDlgItemText(hDlg, IDC_EDIT1, "");
         SetFocus(GetDlgItem(hDlg, IDC_EDIT1));
         return TRUE;

      case IDCANCEL:
         EndDialog(hDlg, IDCANCEL);
         return TRUE;

      case IDOK:
         config.ignoreprofane = IsDlgButtonChecked(hDlg, IDC_RADIO1);
         config.extraprofane = IsDlgButtonChecked(hDlg, IDC_CHECK1);
         RecompileAllProfaneExpressions();
         EndDialog(hDlg, IDOK);
         return TRUE;
      }
      break;
   }

   return FALSE;
}
