/************************************************************************
 * gtk_client.c   dopewars client using the GTK+ toolkit                *
 * Copyright (C)  1998-2022  Ben Webb                                   *
 *                Email: benwebb@users.sf.net                           *
 *                WWW: https://dopewars.sourceforge.io/                 *
 *                                                                      *
 * This program is free software; you can redistribute it and/or        *
 * modify it under the terms of the GNU General Public License          *
 * as published by the Free Software Foundation; either version 2       *
 * of the License, or (at your option) any later version.               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, write to the Free Software          *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,               *
 *                   MA  02111-1307, USA.                               *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "configfile.h"
#include "convert.h"
#include "dopewars.h"
#include "gtk_client.h"
#include "message.h"
#include "nls.h"
#include "serverside.h"
#include "sound.h"
#include "tstring.h"
#include "util.h"
#include "gtkport/gtkport.h"
#include "dopewars-pill.xpm"
#include "optdialog.h"
#include "newgamedia.h"

#define BT_BUY  (GINT_TO_POINTER(1))
#define BT_SELL (GINT_TO_POINTER(2))
#define BT_DROP (GINT_TO_POINTER(3))

#define ET_SPY    0
#define ET_TIPOFF 1

struct InventoryWidgets {
  GtkWidget *HereList, *CarriedList;
  GtkWidget *HereFrame, *CarriedFrame;
  GtkWidget *BuyButton, *SellButton, *DropButton;
  GtkWidget *vbbox;
};

struct StatusWidgets {
  GtkWidget *Location, *Date, *SpaceName, *SpaceValue, *CashName;
  GtkWidget *CashValue, *DebtName, *DebtValue, *BankName, *BankValue;
  GtkWidget *GunsName, *GunsValue, *BitchesName, *BitchesValue;
  GtkWidget *HealthName, *HealthValue;
};

struct ClientDataStruct {
  GtkWidget *window, *messages;
  Player *Play;
  DPGtkItemFactory *Menu;
  struct StatusWidgets Status;
  struct InventoryWidgets Drug, Gun, InvenDrug, InvenGun;
  GtkWidget *JetButton, *vbox, *PlayerList, *TalkList;
  guint JetAccel;
  struct CMDLINE *cmdline;
};

struct DealDiaStruct {
  GtkWidget *dialog, *cost, *carrying, *space, *afford, *amount;
  gint DrugInd;
  gpointer Type;
};
static struct DealDiaStruct DealDialog;

GtkWidget *MainWindow = NULL;

static struct ClientDataStruct ClientData;
static gboolean InGame = FALSE;

static GtkWidget *FightDialog = NULL, *SpyReportsDialog;
static gboolean IsShowingPlayerList = FALSE, IsShowingTalkList = FALSE;
static gboolean IsShowingInventory = FALSE, IsShowingGunShop = FALSE;
static gboolean IsShowingDealDrugs = FALSE;

static void display_intro(GtkWidget *widget, gpointer data);
static void QuitGame(GtkWidget *widget, gpointer data);
static void DestroyGtk(GtkWidget *widget, gpointer data);
static void NewGame(GtkWidget *widget, gpointer data);
static void AbandonGame(GtkWidget *widget, gpointer data);
static void ToggleSound(GtkWidget *widget, gpointer data);
static void ListScores(GtkWidget *widget, gpointer data);
static void ListInventory(GtkWidget *widget, gpointer data);
static void EndGame(void);
static void Jet(GtkWidget *parent);
static void UpdateMenus(void);

#ifdef NETWORKING
gboolean GetClientMessage(GIOChannel *source, GIOCondition condition,
                          gpointer data);
void SocketStatus(NetworkBuffer *NetBuf, gboolean Read, gboolean Write,
                  gboolean Exception, gboolean CallNow);

/* Data waiting to be sent to/read from the metaserver */
CurlConnection MetaConn;
#endif /* NETWORKING */

static void HandleClientMessage(char *buf, Player *Play);
static void PrepareHighScoreDialog(void);
static void AddScoreToDialog(char *Data);
static void CompleteHighScoreDialog(gboolean AtEnd);
static void PrintMessage(char *Data, char *tagname);
static void DisplayFightMessage(char *Data);
static GtkWidget *CreateStatusWidgets(struct StatusWidgets *Status);
static void DisplayStats(Player *Play, struct StatusWidgets *Status);
static void UpdateStatus(Player *Play);
static void SetJetButtonTitle(GtkAccelGroup *accel_group);
static void UpdateInventory(struct InventoryWidgets *Inven,
                            Inventory *Objects, int NumObjects,
                            gboolean AreDrugs);
static void JetButtonPressed(GtkWidget *widget, gpointer data);
static void DealDrugs(GtkWidget *widget, gpointer data);
static void DealGuns(GtkWidget *widget, gpointer data);
static void QuestionDialog(char *Data, Player *From);
static void TransferDialog(gboolean Debt);
static void ListPlayers(GtkWidget *widget, gpointer data);
static void TalkToAll(GtkWidget *widget, gpointer data);
static void TalkToPlayers(GtkWidget *widget, gpointer data);
static void TalkDialog(gboolean TalkToAll);
static GtkWidget *CreatePlayerList(void);
static void UpdatePlayerList(GtkWidget *clist, gboolean IncludeSelf);
static void TipOff(GtkWidget *widget, gpointer data);
static void SpyOnPlayer(GtkWidget *widget, gpointer data);
static void ErrandDialog(gint ErrandType);
static void SackBitch(GtkWidget *widget, gpointer data);
static void DestroyShowing(GtkWidget *widget, gpointer data);
static void SetShowing(GtkWidget *window, gboolean *showing);
static gint DisallowDelete(GtkWidget *widget, GdkEvent * event,
                           gpointer data);
static void GunShopDialog(void);
static void NewNameDialog(void);
static void UpdatePlayerLists(void);
static void CreateInventory(GtkWidget *hbox, gchar *Objects,
                            GtkAccelGroup *accel_group,
                            gboolean CreateButtons, gboolean CreateHere,
                            struct InventoryWidgets *widgets,
                            GCallback CallBack);
static void GetSpyReports(GtkWidget *widget, gpointer data);
static void DisplaySpyReports(Player *Play);

static DPGtkItemFactoryEntry menu_items[] = {
  /* The names of the menus and their items in the GTK+ client */
  {N_("/_Game"), NULL, NULL, 0, "<Branch>"},
  {N_("/Game/_New..."), "<control>N", NewGame, 0, NULL},
  {N_("/Game/_Abandon..."), "<control>A", AbandonGame, 0, NULL},
  {N_("/Game/_Options..."), "<control>O", OptDialog, 0, NULL},
  {N_("/Game/Enable _sound"), NULL, ToggleSound, 0, "<CheckItem>"},
  {N_("/Game/_Quit..."), "<control>Q", QuitGame, 0, NULL},
  {N_("/_Talk"), NULL, NULL, 0, "<Branch>"},
  {N_("/Talk/To _All..."), NULL, TalkToAll, 0, NULL},
  {N_("/Talk/To _Player..."), NULL, TalkToPlayers, 0, NULL},
  {N_("/_List"), NULL, NULL, 0, "<Branch>"},
  {N_("/List/_Players..."), NULL, ListPlayers, 0, NULL},
  {N_("/List/_Scores..."), NULL, ListScores, 0, NULL},
  {N_("/List/_Inventory..."), NULL, ListInventory, 0, NULL},
  {N_("/_Errands"), NULL, NULL, 0, "<Branch>"},
  {N_("/Errands/_Spy..."), NULL, SpyOnPlayer, 0, NULL},
  {N_("/Errands/_Tipoff..."), NULL, TipOff, 0, NULL},
  /* N.B. "Sack Bitch" has to be recreated (and thus translated) at the
   * start of each game, below, so is not marked for gettext here */
  {"/Errands/S_ack Bitch...", NULL, SackBitch, 0, NULL},
  {N_("/Errands/_Get spy reports..."), NULL, GetSpyReports, 0, NULL},
  {N_("/_Help"), NULL, NULL, 0, "<Branch>"},
  {N_("/Help/_About..."), "F1", display_intro, 0, NULL}
};

static gchar *MenuTranslate(const gchar *path, gpointer func_data)
{
  /* Translate menu items, using gettext */
  return _(path);
}

static void LogMessage(const gchar *log_domain, GLogLevelFlags log_level,
                       const gchar *message, gpointer user_data)
{
  GtkMessageBox(MainWindow, message,
                /* Titles of the message boxes for warnings and errors */
                log_level & G_LOG_LEVEL_WARNING ? _("Warning") :
                log_level & G_LOG_LEVEL_CRITICAL ? _("Error") :
                _("Message"), GTK_MESSAGE_INFO,
                MB_OK | (gtk_main_level() > 0 ? MB_IMMRETURN : 0));
}

/*
 * Creates an hbutton_box widget, and sets a sensible spacing and layout.
 */
GtkWidget *my_hbbox_new(void)
{
  GtkWidget *hbbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(hbbox), 8);
  return hbbox;
}

/*
 * Do the equivalent of gtk_box_pack_start_defaults().
 * This has been removed from GTK+3.
 */
void my_gtk_box_pack_start_defaults(GtkBox *box, GtkWidget *child)
{
#ifdef CYGWIN
  /* For compatibility with older dopewars */
  gtk_box_pack_start(box, child, FALSE, FALSE, 0);
#else
  gtk_box_pack_start(box, child, TRUE, TRUE, 0);
#endif
}

/*
 * Sets the initial size and window manager hints of a dialog.
 */
void my_set_dialog_position(GtkWindow *dialog)
{
  gtk_window_set_type_hint(dialog, GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(dialog, GTK_WIN_POS_CENTER_ON_PARENT);
}

void QuitGame(GtkWidget *widget, gpointer data)
{
  if (!InGame || GtkMessageBox(ClientData.window,
                               /* Prompt in 'quit game' dialog */
                               _("Abandon current game?"),
                               /* Title of 'quit game' dialog */
                               _("Quit Game"), GTK_MESSAGE_QUESTION,
                               MB_YESNO) == IDYES) {
    gtk_main_quit();
  }
}

void DestroyGtk(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

gint MainDelete(GtkWidget *widget, GdkEvent * event, gpointer data)
{
  return (InGame
          && GtkMessageBox(ClientData.window, _("Abandon current game?"),
                           _("Quit Game"), GTK_MESSAGE_QUESTION,
                           MB_YESNO) == IDNO);
}


void NewGame(GtkWidget *widget, gpointer data)
{
  if (InGame) {
    if (GtkMessageBox(ClientData.window, _("Abandon current game?"),
                      /* Title of 'stop game to start a new game' dialog */
                      _("Start new game"), GTK_MESSAGE_QUESTION,
                      MB_YESNO) == IDYES)
      EndGame();
    else
      return;
  }

  /* Save the configuration, so we can restore those elements that get
   * overwritten when we connect to a dopewars server */
  BackupConfig();

#ifdef NETWORKING
  NewGameDialog(ClientData.Play, SocketStatus, &MetaConn);
#else
  NewGameDialog(ClientData.Play);
#endif
}

void AbandonGame(GtkWidget *widget, gpointer data)
{
  if (InGame && GtkMessageBox(ClientData.window, _("Abandon current game?"),
                              /* Title of 'abandon game' dialog */
                              _("Abandon game"), GTK_MESSAGE_QUESTION,
                              MB_YESNO) == IDYES) {
    EndGame();
  }
}

void ToggleSound(GtkWidget *widget, gpointer data)
{
  gboolean enable;

  widget = dp_gtk_item_factory_get_widget(ClientData.Menu,
                                          "<main>/Game/Enable sound");
  if (widget) {
    enable = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
    SoundEnable(enable);
  }
}

void ListScores(GtkWidget *widget, gpointer data)
{
  if (InGame) {
    SendClientMessage(ClientData.Play, C_NONE, C_REQUESTSCORE, NULL, NULL);
  } else {
    SendNullClientMessage(ClientData.Play, C_NONE, C_REQUESTSCORE, NULL, NULL);
  }
}

void ListInventory(GtkWidget *widget, gpointer data)
{
  GtkWidget *window, *button, *hsep, *vbox, *hbox, *hbbox;
  GtkAccelGroup *accel_group;

  if (IsShowingInventory)
    return;
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 550, 120);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Title of inventory window */
  gtk_window_set_title(GTK_WINDOW(window), _("Inventory"));
  my_set_dialog_position(GTK_WINDOW(window));

  SetShowing(window, &IsShowingInventory);

  gtk_window_set_transient_for(GTK_WINDOW(window),
                               GTK_WINDOW(ClientData.window));
  gtk_container_set_border_width(GTK_CONTAINER(window), 7);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  CreateInventory(hbox, Names.Drugs, accel_group, FALSE, FALSE,
                  &ClientData.InvenDrug, NULL);
  CreateInventory(hbox, Names.Guns, accel_group, FALSE, FALSE,
                  &ClientData.InvenGun, NULL);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(window));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  UpdateInventory(&ClientData.InvenDrug, ClientData.Play->Drugs, NumDrug,
                  TRUE);
  UpdateInventory(&ClientData.InvenGun, ClientData.Play->Guns, NumGun,
                  FALSE);

  gtk_widget_show_all(window);
}

#ifdef NETWORKING
gboolean GetClientMessage(GIOChannel *source, GIOCondition condition,
                          gpointer data)
{
  gchar *pt;
  NetworkBuffer *NetBuf;
  gboolean DoneOK, datawaiting;
  NBStatus status, oldstatus;
  NBSocksStatus oldsocks;

  NetBuf = &ClientData.Play->NetBuf;

  oldstatus = NetBuf->status;
  oldsocks = NetBuf->sockstat;

  datawaiting =
      PlayerHandleNetwork(ClientData.Play, condition & G_IO_IN,
                          condition & G_IO_OUT,
                          condition & G_IO_ERR, &DoneOK);
  status = NetBuf->status;

  /* Handle pre-game stuff */
  if (status != NBS_CONNECTED) {
    /* The start game dialog isn't visible once we're connected... */
    DisplayConnectStatus(oldstatus, oldsocks);
  }
  if (oldstatus != NBS_CONNECTED && (status == NBS_CONNECTED || !DoneOK)) {
    FinishServerConnect(DoneOK);
  }

  if (status == NBS_CONNECTED && datawaiting) {
    while ((pt = GetWaitingPlayerMessage(ClientData.Play)) != NULL) {
      HandleClientMessage(pt, ClientData.Play);
      g_free(pt);
    }
  }
  if (!DoneOK) {
    if (status == NBS_CONNECTED) {
      /* The network connection to the server was dropped unexpectedly */
      g_warning(_("Connection to server lost - switching to "
                  "single player mode"));
      SwitchToSinglePlayer(ClientData.Play);
      UpdatePlayerLists();
      UpdateMenus();
    } else {
      ShutdownNetworkBuffer(&ClientData.Play->NetBuf);
    }
  }
  return TRUE;
}

void SocketStatus(NetworkBuffer *NetBuf, gboolean Read, gboolean Write,
                  gboolean Exception, gboolean CallNow)
{
  if (NetBuf->InputTag)
    dp_g_source_remove(NetBuf->InputTag);
  NetBuf->InputTag = 0;
  if (Read || Write || Exception) {
    NetBuf->InputTag = dp_g_io_add_watch(NetBuf->ioch,
                                     (Read ? G_IO_IN : 0) |
                                     (Write ? G_IO_OUT : 0) |
                                     (Exception ? G_IO_ERR : 0),
                                     GetClientMessage,
                                     NetBuf->CallBackData);
  }
  if (CallNow)
    GetClientMessage(NetBuf->ioch, 0, NetBuf->CallBackData);
}
#endif /* NETWORKING */

void HandleClientMessage(char *pt, Player *Play)
{
  char *Data;
  DispMode DisplayMode;
  AICode AI;
  MsgCode Code;
  Player *From, *tmp;
  gchar *text;
  gboolean Handled;
  GtkWidget *MenuItem;
  GSList *list;

  if (ProcessMessage(pt, Play, &From, &AI, &Code,
                     &Data, FirstClient) == -1) {
    return;
  }

  Handled =
      HandleGenericClientMessage(From, AI, Code, Play, Data, &DisplayMode);
  switch (Code) {
  case C_STARTHISCORE:
    PrepareHighScoreDialog();
    break;
  case C_HISCORE:
    AddScoreToDialog(Data);
    break;
  case C_ENDHISCORE:
    CompleteHighScoreDialog((strcmp(Data, "end") == 0));
    break;
  case C_PRINTMESSAGE:
    PrintMessage(Data, NULL);
    break;
  case C_FIGHTPRINT:
    DisplayFightMessage(Data);
    break;
  case C_PUSH:
    /* The server admin has asked us to leave - so warn the user, and do
       so */
    g_warning(_("You have been pushed from the server.\n"
                "Switching to single player mode."));
    SwitchToSinglePlayer(Play);
    UpdatePlayerLists();
    UpdateMenus();
    break;
  case C_QUIT:
    /* The server has sent us notice that it is shutting down */
    g_warning(_("The server has terminated.\n"
                "Switching to single player mode."));
    SwitchToSinglePlayer(Play);
    UpdatePlayerLists();
    UpdateMenus();
    break;
  case C_NEWNAME:
    NewNameDialog();
    break;
  case C_BANK:
    TransferDialog(FALSE);
    break;
  case C_LOANSHARK:
    TransferDialog(TRUE);
    break;
  case C_GUNSHOP:
    GunShopDialog();
    break;
  case C_MSG:
    text = g_strdup_printf("%s: %s", GetPlayerName(From), Data);
    PrintMessage(text, "talk");
    g_free(text);
    SoundPlay(Sounds.TalkToAll);
    break;
  case C_MSGTO:
    text = g_strdup_printf("%s->%s: %s", GetPlayerName(From),
                           GetPlayerName(Play), Data);
    PrintMessage(text, "page");
    g_free(text);
    SoundPlay(Sounds.TalkPrivate);
    break;
  case C_JOIN:
    text = g_strdup_printf(_("%s joins the game!"), Data);
    PrintMessage(text, "join");
    g_free(text);
    SoundPlay(Sounds.JoinGame);
    UpdatePlayerLists();
    UpdateMenus();
    break;
  case C_LEAVE:
    if (From != &Noone) {
      text = g_strdup_printf(_("%s has left the game."), Data);
      PrintMessage(text, "leave");
      g_free(text);
      SoundPlay(Sounds.LeaveGame);
      UpdatePlayerLists();
      UpdateMenus();
    }
    break;
  case C_QUESTION:
    QuestionDialog(Data, From == &Noone ? NULL : From);
    break;
  case C_SUBWAYFLASH:
    DisplayFightMessage(NULL);
    for (list = FirstClient; list; list = g_slist_next(list)) {
      tmp = (Player *)list->data;
      tmp->Flags &= ~FIGHTING;
    }
    /* Message displayed when the player "jets" to a new location */
    text = dpg_strdup_printf(_("Jetting to %tde"),
                             Location[(int)Play->IsAt].Name);
    PrintMessage(text, "jet");
    g_free(text);
    SoundPlay(Sounds.Jet);
    break;
  case C_ENDLIST:
    MenuItem = dp_gtk_item_factory_get_widget(ClientData.Menu,
                                              "<main>/Errands/Sack Bitch...");

    /* Text for the Errands/Sack Bitch menu item */
    text = dpg_strdup_printf(_("%/Sack Bitch menu item/S_ack %Tde..."),
                             Names.Bitch);
    SetAccelerator(MenuItem, text, NULL, NULL, NULL, FALSE);
    g_free(text);

    MenuItem = dp_gtk_item_factory_get_widget(ClientData.Menu,
                                              "<main>/Errands/Spy...");

    /* Text to update the Errands/Spy menu item with the price for spying */
    text = dpg_strdup_printf(_("_Spy (%P)"), Prices.Spy);
    SetAccelerator(MenuItem, text, NULL, NULL, NULL, FALSE);
    g_free(text);

    /* Text to update the Errands/Tipoff menu item with the price for a
       tipoff */
    text = dpg_strdup_printf(_("_Tipoff (%P)"), Prices.Tipoff);
    MenuItem = dp_gtk_item_factory_get_widget(ClientData.Menu,
                                              "<main>/Errands/Tipoff...");
    SetAccelerator(MenuItem, text, NULL, NULL, NULL, FALSE);
    g_free(text);
    if (FirstClient->next)
      ListPlayers(NULL, NULL);
    UpdateMenus();
    break;
  case C_UPDATE:
    if (From == &Noone) {
      ReceivePlayerData(Play, Data, Play);
      UpdateStatus(Play);
    } else {
      ReceivePlayerData(Play, Data, From);
      DisplaySpyReports(From);
    }
    break;
  case C_DRUGHERE:
    UpdateInventory(&ClientData.Drug, Play->Drugs, NumDrug, TRUE);
    if (IsShowingInventory) {
      UpdateInventory(&ClientData.InvenDrug, Play->Drugs, NumDrug, TRUE);
    }
    if (IsShowingDealDrugs) {
      gtk_widget_destroy(DealDialog.dialog);
    }
    break;
  default:
    if (!Handled) {
      g_print("Unknown network message received: %s^%c^%s^%s",
              GetPlayerName(From), Code, GetPlayerName(Play), Data);
    }
    break;
  }
}

struct HiScoreDiaStruct {
  GtkWidget *dialog, *grid, *vbox;
  GtkAccelGroup *accel_group;
};
static struct HiScoreDiaStruct HiScoreDialog = { NULL, NULL, NULL, NULL };

/* 
 * Creates an empty dialog to display high scores.
 */
void PrepareHighScoreDialog(void)
{
  GtkWidget *dialog, *vbox, *hsep, *grid;

  /* Make sure the server doesn't fool us into creating multiple dialogs */
  if (HiScoreDialog.dialog)
    return;

  HiScoreDialog.dialog = dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  HiScoreDialog.accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), HiScoreDialog.accel_group);

  /* Title of the GTK+ high score dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("High Scores"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));

  HiScoreDialog.vbox = vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
  HiScoreDialog.grid = grid = dp_gtk_grid_new(NUMHISCORE, 4, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 30);

  gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);
  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);
}

/* 
 * Adds a single high score (coded in "Data", which is the information
 * received in the relevant network message) to the dialog created by
 * PrepareHighScoreDialog(), above.
 */
void AddScoreToDialog(char *Data)
{
  GtkWidget *label;
  char *cp;
  gchar **spl1, **spl2;
  int index, slen;
  gboolean bold;

  if (!HiScoreDialog.dialog)
    return;

  cp = Data;
  index = GetNextInt(&cp, 0);
  if (!cp || strlen(cp) < 3)
    return;

  bold = (*cp == 'B');          /* Is this score "our" score? (Currently
                                 * ignored) */

  /* Step past the 'bold' character, and the initial '>' (if present) */
  cp += 2;
  g_strchug(cp);

  /* Get the first word - the score */
  spl1 = g_strsplit(cp, " ", 2);
  if (!spl1 || !spl1[0] || !spl1[1]) {
    /* Error - the high score from the server is invalid */
    g_warning(_("Corrupt high score!"));
    g_strfreev(spl1);
    return;
  }
  label = make_bold_label(spl1[0], bold);
  set_label_alignment(label, 1.0, 0.5);
  dp_gtk_grid_attach(GTK_GRID(HiScoreDialog.grid), label, 0, index, 1, 1, TRUE);
  gtk_widget_show(label);

  /* Remove any leading whitespace from the remainder, since g_strsplit
   * will split at every space character, not at a run of them */
  g_strchug(spl1[1]);

  /* Get the second word - the date */
  spl2 = g_strsplit(spl1[1], " ", 2);
  if (!spl2 || !spl2[0] || !spl2[1]) {
    g_warning(_("Corrupt high score!"));
    g_strfreev(spl2);
    return;
  }
  label = make_bold_label(spl2[0], bold);
  set_label_alignment(label, 0.5, 0.5);
  dp_gtk_grid_attach(GTK_GRID(HiScoreDialog.grid), label, 1, index, 1, 1, TRUE);
  gtk_widget_show(label);

  /* The remainder is the name, terminated with (R.I.P.) if the player
   * died, and '<' for the 'current' score */
  g_strchug(spl2[1]);

  /* Remove '<' suffix if present */
  slen = strlen(spl2[1]);
  if (slen >= 1 && spl2[1][slen - 1] == '<') {
    spl2[1][slen - 1] = '\0';
  }
  slen--;

  /* Check for (R.I.P.) suffix, and add it to the 4th column if found */
  if (slen > 8 && spl2[1][slen - 1] == ')' && spl2[1][slen - 8] == '(') {
    label = make_bold_label(&spl2[1][slen - 8], bold);
    set_label_alignment(label, 0.5, 0.5);
    dp_gtk_grid_attach(GTK_GRID(HiScoreDialog.grid), label, 3, index, 1, 1,
                       TRUE);
    gtk_widget_show(label);
    spl2[1][slen - 8] = '\0';   /* Remove suffix from the player name */
  }

  /* Finally, add in what's left of the player name */
  g_strchomp(spl2[1]);
  label = make_bold_label(spl2[1], bold);
  set_label_alignment(label, 0, 0.5);
  dp_gtk_grid_attach(GTK_GRID(HiScoreDialog.grid), label, 2, index, 1, 1, TRUE);
  gtk_widget_show(label);

  g_strfreev(spl1);
  g_strfreev(spl2);
}

/* 
 * If the high scores are being displayed at the end of the game,
 * this function is used to end the game when the high score dialog's
 * "OK" button is pressed.
 */
static void EndHighScore(GtkWidget *widget)
{
  EndGame();
}

/* 
 * Called when all high scores have been received. Finishes off the
 * high score dialog by adding an "OK" button. If the game has ended,
 * then "AtEnd" is TRUE, and clicking this button will end the game.
 */
void CompleteHighScoreDialog(gboolean AtEnd)
{
  GtkWidget *button, *dialog, *hbbox;

  dialog = HiScoreDialog.dialog;

  if (!HiScoreDialog.dialog) {
    return;
  }

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  if (AtEnd) {
    InGame = FALSE;
    g_signal_connect(G_OBJECT(dialog), "destroy",
                     G_CALLBACK(EndHighScore), NULL);
  }
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  gtk_box_pack_start(GTK_BOX(HiScoreDialog.vbox), hbbox, FALSE, FALSE, 0);

  gtk_widget_set_can_default(button, TRUE);
  gtk_widget_grab_default(button);
  gtk_widget_show_all(hbbox);

  /* OK, we're done - allow the creation of new high score dialogs */
  HiScoreDialog.dialog = NULL;
}

/* 
 * Prints an information message in the display area of the GTK+ client.
 * This area is used for displaying drug busts, messages from other
 * players, etc. The message is passed in as the string "text".
 */
void PrintMessage(char *text, char *tagname)
{
  GtkTextView *messages = GTK_TEXT_VIEW(ClientData.messages);

  g_strdelimit(text, "^", '\n');
  TextViewAppend(messages, text, tagname, FALSE);
  TextViewAppend(messages, "\n", NULL, TRUE);
}

static void FreeCombatants(void);

/* 
 * Called when one of the action buttons in the Fight dialog is clicked.
 * "data" specifies which button (Deal Drugs/Run/Fight/Stand) was pressed.
 */
static void FightCallback(GtkWidget *widget, gpointer data)
{
  gint Answer;
  Player *Play;
  gchar text[4];
  GtkWidget *window;
  gpointer CanRunHere = NULL;

  window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
  if (window) {
    CanRunHere = g_object_get_data(G_OBJECT(window), "CanRunHere");
  }

  Answer = GPOINTER_TO_INT(data);
  Play = ClientData.Play;
  switch (Answer) {
  case 'D':
    gtk_widget_hide(FightDialog);
    if (!(Play->Flags & FIGHTING)) {
      FreeCombatants();
      gtk_widget_destroy(FightDialog);
      FightDialog = NULL;
      if (HaveAbility(Play, A_DONEFIGHT)) {
        SendClientMessage(Play, C_NONE, C_DONE, NULL, NULL);
      }
    }
    break;
  case 'R':
    if (CanRunHere) {
      SendClientMessage(Play, C_NONE, C_FIGHTACT, NULL, "R");
    } else {
      Jet(FightDialog);
    }
    break;
  case 'F':
  case 'S':
    text[0] = Answer;
    text[1] = '\0';
    SendClientMessage(Play, C_NONE, C_FIGHTACT, NULL, text);
    break;
  }
}

/* 
 * Adds an action button to the hbox at the base of the Fight dialog.
 * The button's caption is given by "Text", and the keyboard shortcut
 * (if any) is added to "accel_group". "Answer" gives the identifier
 * passed to FightCallback, above.
 */
static GtkWidget *AddFightButton(gchar *Text, GtkAccelGroup *accel_group,
                                 GtkBox *box, gint Answer)
{
  GtkWidget *button;

  button = gtk_button_new_with_label("");
  SetAccelerator(button, Text, button, "clicked", accel_group, FALSE);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(FightCallback),
                   GINT_TO_POINTER(Answer));
  gtk_box_pack_start(box, button, TRUE, TRUE, 0);
  return button;
}

/* Data used to keep track of the widgets giving the information about a
 * player/cop involved in a fight */
struct combatant {
  GtkWidget *name, *bitches, *healthprog, *healthlabel;
};

/* 
 * Creates an empty Fight dialog. Usually this only needs to be done once,
 * as when the user "closes" it, it is only hidden, ready to be reshown
 * later. Buttons for all actions are added here, and are hidden/shown
 * as necessary.
 */
static void CreateFightDialog(void)
{
  GtkWidget *dialog, *vbox, *button, *hbox, *hbbox, *hsep, *text, *grid;
  GtkAccelGroup *accel_group;
  GArray *combatants;
  gchar *buf;

  FightDialog = dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 350, 250);
  g_signal_connect(G_OBJECT(dialog), "delete_event",
                   G_CALLBACK(DisallowDelete), NULL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);
  gtk_window_set_title(GTK_WINDOW(dialog), _("Fight"));
  my_set_dialog_position(GTK_WINDOW(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  grid = dp_gtk_grid_new(2, 4, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 7);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  dp_gtk_grid_attach(GTK_GRID(grid), hsep, 0, 1, 3, 1, TRUE);
  gtk_widget_show_all(grid);
  gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(dialog), "grid", grid);

  combatants = g_array_new(FALSE, TRUE, sizeof(struct combatant));
  g_array_set_size(combatants, 1);
  g_object_set_data(G_OBJECT(dialog), "combatants", combatants);

  text = gtk_scrolled_text_view_new(&hbox);
  gtk_widget_set_size_request(text, 150, 120);

  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
  g_object_set_data(G_OBJECT(dialog), "text", text);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);
  gtk_widget_show(hsep);

  hbbox = my_hbbox_new();

  /* Button for closing the "Fight" dialog and going back to dealing drugs
     (%Tde = "Drugs" by default) */
  buf = dpg_strdup_printf(_("_Deal %Tde"), Names.Drugs);
  button = AddFightButton(buf, accel_group, GTK_BOX(hbbox), 'D');
  g_object_set_data(G_OBJECT(dialog), "deal", button);
  g_free(buf);

  /* Button for shooting at other players in the "Fight" dialog, or for
     popping up the "Fight" dialog from the main window */
  button = AddFightButton(_("_Fight"), accel_group, GTK_BOX(hbbox), 'F');
  g_object_set_data(G_OBJECT(dialog), "fight", button);

  /* Button to stand and take it in the "Fight" dialog */
  button = AddFightButton(_("_Stand"), accel_group, GTK_BOX(hbbox), 'S');
  g_object_set_data(G_OBJECT(dialog), "stand", button);

  /* Button to run from combat in the "Fight" dialog */
  button = AddFightButton(_("_Run"), accel_group, GTK_BOX(hbbox), 'R');
  g_object_set_data(G_OBJECT(dialog), "run", button);

  gtk_widget_show(hsep);
  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_widget_show(hbbox);
  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show(dialog);
}

/* 
 * Updates the display of information for a player/cop in the Fight dialog.
 * If the player's name (DefendName) already exists, updates the display of
 * total health and number of bitches - otherwise, adds a new entry. If
 * DefendBitches is -1, then the player has left.
 */
static void UpdateCombatant(gchar *DefendName, int DefendBitches,
                            gchar *BitchName, int DefendHealth)
{
  guint i, RowIndex;
  const gchar *name;
  struct combatant *compt;
  GArray *combatants;
  GtkWidget *grid;
  gchar *BitchText, *HealthText;
  gfloat ProgPercent;

  combatants = (GArray *)g_object_get_data(G_OBJECT(FightDialog),
                                           "combatants");
  grid = GTK_WIDGET(g_object_get_data(G_OBJECT(FightDialog), "grid"));
  if (!combatants) {
    return;
  }

  if (DefendName[0]) {
    compt = NULL;
    for (i = 1, RowIndex = 2; i < combatants->len; i++, RowIndex++) {
      compt = &g_array_index(combatants, struct combatant, i);

      if (!compt || !compt->name) {
        compt = NULL;
        continue;
      }
      name = gtk_label_get_text(GTK_LABEL(compt->name));
      if (name && strcmp(name, DefendName) == 0) {
        break;
      }
      compt = NULL;
    }
    if (!compt) {
      i = combatants->len;
      g_array_set_size(combatants, i + 1);
      compt = &g_array_index(combatants, struct combatant, i);

      dp_gtk_grid_resize(GTK_GRID(grid), i + 2, 4);
      RowIndex = i + 1;
    }
  } else {
    compt = &g_array_index(combatants, struct combatant, 0);

    RowIndex = 0;
  }

  /* Display of number of bitches or deputies during combat
     (%tde="bitches" or "deputies" (etc.) by default) */
  BitchText = dpg_strdup_printf(_("%/Combat: Bitches/%d %tde"),
                                DefendBitches, BitchName);

  /* Display of health during combat */
  if (DefendBitches == -1) {
    HealthText = g_strdup(_("(Left)"));
  } else if (DefendHealth == 0 && DefendBitches == 0) {
    HealthText = g_strdup(_("(Dead)"));
  } else {
    HealthText = g_strdup_printf(_("Health: %d"), DefendHealth);
  }

  ProgPercent = (gfloat)DefendHealth / 100.0;

  if (compt->name) {
    if (DefendName[0]) {
      gtk_label_set_text(GTK_LABEL(compt->name), DefendName);
    }
    if (DefendBitches >= 0) {
      gtk_label_set_text(GTK_LABEL(compt->bitches), BitchText);
    }
    gtk_label_set_text(GTK_LABEL(compt->healthlabel), HealthText);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(compt->healthprog),
                                  ProgPercent);
  } else {
    /* Display of the current player's name during combat */
    compt->name = gtk_label_new(DefendName[0] ? DefendName : _("You"));

    dp_gtk_grid_attach(GTK_GRID(grid), compt->name, 0, RowIndex, 1, 1, FALSE);
    compt->bitches = gtk_label_new(DefendBitches >= 0 ? BitchText : "");
    dp_gtk_grid_attach(GTK_GRID(grid), compt->bitches, 1, RowIndex, 1, 1,
                       FALSE);
    compt->healthprog = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(compt->healthprog),
                                  ProgPercent);
    dp_gtk_grid_attach(GTK_GRID(grid), compt->healthprog, 2, RowIndex, 1, 1,
                       TRUE);
    compt->healthlabel = gtk_label_new(HealthText);
    dp_gtk_grid_attach(GTK_GRID(grid), compt->healthlabel, 3, RowIndex, 1, 1,
                       FALSE);
    gtk_widget_show(compt->name);
    gtk_widget_show(compt->bitches);
    gtk_widget_show(compt->healthprog);
    gtk_widget_show(compt->healthlabel);
  }

  g_free(BitchText);
  g_free(HealthText);
}

/* 
 * Cleans up the list of all players/cops involved in a fight.
 */
static void FreeCombatants(void)
{
  GArray *combatants;

  combatants = (GArray *)g_object_get_data(G_OBJECT(FightDialog),
                                           "combatants");
  if (combatants) {
    g_array_free(combatants, TRUE);
  }
}

static void EnableFightButton(GtkWidget *button, gboolean enable)
{
  if (enable) {
    gtk_widget_set_sensitive(button, TRUE);
    gtk_widget_show(button);
  } else {
    gtk_widget_hide(button);
    gtk_widget_set_sensitive(button, FALSE);
  }
}

/* 
 * Given the network message "Data" concerning some happening during
 * combat, extracts the relevant data and updates the Fight dialog,
 * creating and/or showing it if necessary.
 * If "Data" is NULL, then closes the dialog. If "Data" is a blank
 * string, then just shows the dialog, displaying no new messages.
 */
void DisplayFightMessage(char *Data)
{
  Player *Play;
  GtkAccelGroup *accel_group;
  GtkWidget *Deal, *Fight, *Stand, *Run;
  GtkTextView *textview;
  gchar *AttackName, *DefendName, *BitchName, *Message;
  FightPoint fp;
  int DefendHealth, DefendBitches, BitchesKilled, ArmPercent;
  gboolean CanRunHere, Loot, CanFire;

  if (!Data) {
    if (FightDialog) {
      FreeCombatants();
      gtk_widget_destroy(FightDialog);
      FightDialog = NULL;
    }
    return;
  }
  if (FightDialog) {
    if (IsShowingDealDrugs) {
      gtk_widget_destroy(DealDialog.dialog);
    }
    if (!gtk_widget_get_visible(FightDialog)) {
      gtk_widget_show(FightDialog);
    }
  } else {
    CreateFightDialog();
  }
  if (!FightDialog || !Data[0]) {
    return;
  }

  Deal = GTK_WIDGET(g_object_get_data(G_OBJECT(FightDialog), "deal"));
  Fight = GTK_WIDGET(g_object_get_data(G_OBJECT(FightDialog), "fight"));
  Stand = GTK_WIDGET(g_object_get_data(G_OBJECT(FightDialog), "stand"));
  Run = GTK_WIDGET(g_object_get_data(G_OBJECT(FightDialog), "run"));
  textview = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(FightDialog), "text"));

  Play = ClientData.Play;

  if (HaveAbility(Play, A_NEWFIGHT)) {
    ReceiveFightMessage(Data, &AttackName, &DefendName, &DefendHealth,
                        &DefendBitches, &BitchName, &BitchesKilled,
                        &ArmPercent, &fp, &CanRunHere, &Loot, &CanFire,
                        &Message);
    Play->Flags |= FIGHTING;
    switch (fp) {
    case F_HIT:
    case F_ARRIVED:
    case F_MISS:
      UpdateCombatant(DefendName, DefendBitches, BitchName, DefendHealth);
      break;
    case F_LEAVE:
      if (AttackName[0]) {
        UpdateCombatant(AttackName, -1, BitchName, 0);
      }
      break;
    case F_LASTLEAVE:
      Play->Flags &= ~FIGHTING;
      break;
    default:
      break;
    }
    accel_group = (GtkAccelGroup *)
        g_object_get_data(G_OBJECT(ClientData.window), "accel_group");
    SetJetButtonTitle(accel_group);
  } else {
    Message = Data;
    if (Play->Flags & FIGHTING) {
      fp = F_MSG;
    } else {
      fp = F_LASTLEAVE;
    }
    CanFire = (Play->Flags & CANSHOOT);
    CanRunHere = FALSE;
  }
  g_object_set_data(G_OBJECT(FightDialog), "CanRunHere",
                    GINT_TO_POINTER(CanRunHere));

  g_strdelimit(Message, "^", '\n');
  if (strlen(Message) > 0) {
    TextViewAppend(textview, Message, NULL, FALSE);
    TextViewAppend(textview, "\n", NULL, TRUE);
  }

  EnableFightButton(Deal, !CanRunHere || fp == F_LASTLEAVE);
  EnableFightButton(Fight, CanFire && TotalGunsCarried(Play) > 0);
  EnableFightButton(Stand, CanFire && TotalGunsCarried(Play) == 0);
  EnableFightButton(Run, fp != F_LASTLEAVE);
}

/* 
 * Updates the display of pertinent data about player "Play" (location,
 * health, etc. in the status widgets given by "Status". This can point
 * to the widgets at the top of the main window, or those in a Spy
 * Reports dialog.
 */
void DisplayStats(Player *Play, struct StatusWidgets *Status)
{
  gchar *prstr;
  GString *text;

  text = g_string_new(NULL);

  dpg_string_printf(text, _("%/Current location/%tde"),
                     Location[Play->IsAt].Name);
  gtk_label_set_text(GTK_LABEL(Status->Location), text->str);

  GetDateString(text, Play);
  gtk_label_set_text(GTK_LABEL(Status->Date), text->str);

  g_string_printf(text, "%d", Play->CoatSize);
  gtk_label_set_text(GTK_LABEL(Status->SpaceValue), text->str);

  prstr = FormatPrice(Play->Cash);
  gtk_label_set_text(GTK_LABEL(Status->CashValue), prstr);
  g_free(prstr);

  prstr = FormatPrice(Play->Bank);
  gtk_label_set_text(GTK_LABEL(Status->BankValue), prstr);
  g_free(prstr);

  prstr = FormatPrice(Play->Debt);
  gtk_label_set_text(GTK_LABEL(Status->DebtValue), prstr);
  g_free(prstr);

  /* Display of the total number of guns carried (%Tde="Guns" by default) */
  dpg_string_printf(text, _("%/Stats: Guns/%Tde"), Names.Guns);
  gtk_label_set_text(GTK_LABEL(Status->GunsName), text->str);
  g_string_printf(text, "%d", TotalGunsCarried(Play));
  gtk_label_set_text(GTK_LABEL(Status->GunsValue), text->str);

  if (!WantAntique) {
    /* Display of number of bitches in GTK+ client status window
       (%Tde="Bitches" by default) */
    dpg_string_printf(text, _("%/GTK Stats: Bitches/%Tde"),
                       Names.Bitches);
    gtk_label_set_text(GTK_LABEL(Status->BitchesName), text->str);
    g_string_printf(text, "%d", Play->Bitches.Carried);
    gtk_label_set_text(GTK_LABEL(Status->BitchesValue), text->str);
  } else {
    gtk_label_set_text(GTK_LABEL(Status->BitchesName), NULL);
    gtk_label_set_text(GTK_LABEL(Status->BitchesValue), NULL);
  }

  g_string_printf(text, "%d", Play->Health);
  gtk_label_set_text(GTK_LABEL(Status->HealthValue), text->str);

  g_string_free(text, TRUE);
}

/* 
 * Updates all of the player status in response to a message from the
 * server. This includes the main window display, the gun shop (if
 * displayed) and the inventory (if displayed).
 */
void UpdateStatus(Player *Play)
{
  GtkAccelGroup *accel_group;

  DisplayStats(Play, &ClientData.Status);
  UpdateInventory(&ClientData.Drug, ClientData.Play->Drugs, NumDrug, TRUE);
  accel_group = (GtkAccelGroup *)
      g_object_get_data(G_OBJECT(ClientData.window), "accel_group");
  SetJetButtonTitle(accel_group);
  if (IsShowingGunShop) {
    UpdateInventory(&ClientData.Gun, ClientData.Play->Guns, NumGun, FALSE);
  }
  if (IsShowingInventory) {
    UpdateInventory(&ClientData.InvenDrug, ClientData.Play->Drugs,
                    NumDrug, TRUE);
    UpdateInventory(&ClientData.InvenGun, ClientData.Play->Guns,
                    NumGun, FALSE);
  }
}

/* Columns in inventory list */
enum {
  INVEN_COL_NAME = 0,
  INVEN_COL_NUM,
  INVEN_COL_INDEX,
  INVEN_NUM_COLS
};

/* Get the currently selected inventory item (drug/gun) as an index into
   the drug/gun array, or -1 if none is selected */
static int get_selected_inventory(GtkTreeSelection *treesel)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(treesel, &model, &iter)) {
    int ind;
    gtk_tree_model_get(model, &iter, INVEN_COL_INDEX, &ind, -1);
    return ind;
  } else {
    return -1;
  }
}

static void scroll_to_selection(GtkTreeModel *model, GtkTreePath *path,
                                GtkTreeIter *iter, gpointer data)
{
  GtkTreeView *tv = data;
  gtk_tree_view_scroll_to_cell(tv, path, NULL, FALSE, 0., 0.);
}

void UpdateInventory(struct InventoryWidgets *Inven,
                     Inventory *Objects, int NumObjects, gboolean AreDrugs)
{
  GtkWidget *herelist, *carrylist;
  gint i;
  price_t price;
  gchar *titles[2];
  gboolean CanBuy = FALSE, CanSell = FALSE, CanDrop = FALSE;
  GtkTreeIter iter;
  GtkTreeView *tv[2];
  GtkListStore *carrystore, *herestore;
  int numlist, selectrow[2];

  herelist = Inven->HereList;
  carrylist = Inven->CarriedList;

  numlist = (herelist ? 2 : 1);

  /* Get current selections */
  tv[0] = GTK_TREE_VIEW(carrylist);
  carrystore = GTK_LIST_STORE(gtk_tree_view_get_model(tv[0]));
  if (herelist) {
    tv[1] = GTK_TREE_VIEW(herelist);
    herestore = GTK_LIST_STORE(gtk_tree_view_get_model(tv[1]));
  } else {
    tv[1] = NULL;
    herestore = NULL;
  }

  for (i = 0; i < numlist; i++) {
    selectrow[i] = get_selected_inventory(gtk_tree_view_get_selection(tv[i]));
  }

  gtk_list_store_clear(carrystore);

  if (herelist) {
    gtk_list_store_clear(herestore);
  }

  for (i = 0; i < NumObjects; i++) {
    if (AreDrugs) {
      titles[0] = dpg_strdup_printf(_("%/Inventory drug name/%tde"),
                                    Drug[i].Name);
      price = Objects[i].Price;
    } else {
      titles[0] = dpg_strdup_printf(_("%/Inventory gun name/%tde"),
                                    Gun[i].Name);
      price = Gun[i].Price;
    }

    if (herelist && price > 0) {
      CanBuy = TRUE;
      titles[1] = FormatPrice(price);
      gtk_list_store_append(herestore, &iter);
      gtk_list_store_set(herestore, &iter, INVEN_COL_NAME, titles[0],
                         INVEN_COL_NUM, titles[1], INVEN_COL_INDEX, i, -1);
      g_free(titles[1]);
      if (i == selectrow[1]) {
        gtk_tree_selection_select_iter(gtk_tree_view_get_selection(tv[1]),
                                       &iter);
      }
    }

    if (Objects[i].Carried > 0) {
      if (price > 0) {
        CanSell = TRUE;
      } else {
        CanDrop = TRUE;
      }
      if (HaveAbility(ClientData.Play, A_DRUGVALUE) && AreDrugs) {
        titles[1] = dpg_strdup_printf("%d @ %P", Objects[i].Carried,
                                      Objects[i].TotalValue /
                                      Objects[i].Carried);
      } else {
        titles[1] = g_strdup_printf("%d", Objects[i].Carried);
      }
      gtk_list_store_append(carrystore, &iter);
      gtk_list_store_set(carrystore, &iter, INVEN_COL_NAME, titles[0],
                         INVEN_COL_NUM, titles[1], INVEN_COL_INDEX, i, -1);
      g_free(titles[1]);
      if (i == selectrow[0]) {
        gtk_tree_selection_select_iter(gtk_tree_view_get_selection(tv[0]),
                                       &iter);
      }
    }
    g_free(titles[0]);
  }

#ifdef CYGWIN
  /* Our Win32 GtkTreeView implementation doesn't auto-sort, so force it */
  if (herelist) {
    gtk_tree_view_sort(GTK_TREE_VIEW(herelist));
  }
#endif

  /* Scroll so that selection is visible */
  for (i = 0; i < numlist; i++) {
    gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection(tv[i]),
		    scroll_to_selection, tv[i]);
  }

  if (Inven->vbbox) {
    gtk_widget_set_sensitive(Inven->BuyButton, CanBuy);
    gtk_widget_set_sensitive(Inven->SellButton, CanSell);
    gtk_widget_set_sensitive(Inven->DropButton, CanDrop);
  }
}

static void JetCallback(GtkWidget *widget, gpointer data)
{
  int NewLocation;
  gchar *text;
  GtkWidget *JetDialog;

  JetDialog = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "dialog"));
  NewLocation = GPOINTER_TO_INT(data);
  gtk_widget_destroy(JetDialog);
  text = g_strdup_printf("%d", NewLocation);
  SendClientMessage(ClientData.Play, C_NONE, C_REQUESTJET, NULL, text);
  g_free(text);
}

void JetButtonPressed(GtkWidget *widget, gpointer data)
{
  if (InGame) {
    if (ClientData.Play->Flags & FIGHTING) {
      DisplayFightMessage("");
    } else {
      Jet(NULL);
    }
  }
}

void Jet(GtkWidget *parent)
{
  GtkWidget *dialog, *grid, *button, *label, *vbox;
  GtkAccelGroup *accel_group;
  gint boxsize, i, row, col;
  gchar *name, AccelChar;

  accel_group = gtk_accel_group_new();

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  /* Title of 'Jet' dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("Jet to location"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               parent ? GTK_WINDOW(parent)
                               : GTK_WINDOW(ClientData.window));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  /* Prompt in 'Jet' dialog */
  label = gtk_label_new(_("Where to, dude ? "));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  /* Generate a square box of buttons for all locations */
  boxsize = 1;
  while (boxsize * boxsize < NumLocation) {
    boxsize++;
  }
  col = boxsize;
  row = 1;

  /* Avoid creating a box with an entire row empty at the bottom */
  while (row * col < NumLocation) {
    row++;
  }

  grid = dp_gtk_grid_new(row, col, TRUE);

  for (i = 0; i < NumLocation; i++) {
    if (i < 9) {
      AccelChar = '1' + i;
    } else if (i < 35) {
      AccelChar = 'A' + i - 9;
    } else {
      AccelChar = '\0';
    }

    row = i / boxsize;
    col = i % boxsize;
    if (AccelChar == '\0') {
      name = dpg_strdup_printf(_("%/Location to jet to/%tde"),
                               Location[i].Name);
      button = gtk_button_new_with_label(name);
      g_free(name);
    } else {
      button = gtk_button_new_with_label("");

      /* Display of locations in 'Jet' window (%tde="The Bronx" etc. by
         default) */
      name = dpg_strdup_printf(_("_%c. %tde"), AccelChar, Location[i].Name);
      SetAccelerator(button, name, button, "clicked", accel_group, FALSE);
      /* Add keypad shortcuts as well */
      if (i < 9) {
        gtk_widget_add_accelerator(button, "clicked", accel_group,
                                   GDK_KEY_KP_1 + i, 0,
                                   GTK_ACCEL_VISIBLE);
      }
      g_free(name);
    }
    gtk_widget_set_sensitive(button, i != ClientData.Play->IsAt);
    g_object_set_data(G_OBJECT(button), "dialog", dialog);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(JetCallback), GINT_TO_POINTER(i));
    dp_gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1, TRUE);
  }
  gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);
}

static void UpdateDealDialog(void)
{
  GString *text;
  GtkAdjustment *spin_adj;
  gint DrugInd, CanDrop, CanCarry, CanAfford, MaxDrug;
  Player *Play;

  text = g_string_new(NULL);
  DrugInd = DealDialog.DrugInd;
  Play = ClientData.Play;

  /* Display of the current price of the selected drug in 'Deal Drugs'
     dialog */
  dpg_string_printf(text, _("at %P"), Play->Drugs[DrugInd].Price);
  gtk_label_set_text(GTK_LABEL(DealDialog.cost), text->str);

  CanDrop = Play->Drugs[DrugInd].Carried;

  /* Display of current inventory of the selected drug in 'Deal Drugs'
     dialog (%tde="Opium" etc. by default) */
  dpg_string_printf(text, _("You are currently carrying %d %tde"),
                     CanDrop, Drug[DrugInd].Name);
  gtk_label_set_text(GTK_LABEL(DealDialog.carrying), text->str);

  CanCarry = Play->CoatSize;

  /* Available space for drugs in 'Deal Drugs' dialog */
  g_string_printf(text, _("Available space: %d"), CanCarry);
  gtk_label_set_text(GTK_LABEL(DealDialog.space), text->str);

  if (DealDialog.Type == BT_BUY) {
    /* Just in case a price update from the server slips through */
    if (Play->Drugs[DrugInd].Price == 0) {
      CanAfford = 0;
    } else {
      CanAfford = Play->Cash / Play->Drugs[DrugInd].Price;
    }

    /* Number of the selected drug that you can afford in 'Deal Drugs'
       dialog */
    g_string_printf(text, _("You can afford %d"), CanAfford);
    gtk_label_set_text(GTK_LABEL(DealDialog.afford), text->str);
    MaxDrug = MIN(CanCarry, CanAfford);
  } else {
    MaxDrug = CanDrop;
  }

  spin_adj = (GtkAdjustment *)gtk_adjustment_new(MaxDrug, 0.0, MaxDrug,
                                                 1.0, 10.0, 0.0);
  gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(DealDialog.amount),
                                 spin_adj);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(DealDialog.amount), MaxDrug);

  g_string_free(text, TRUE);
}

/* Columns in deal list */
enum {
  DEAL_COL_NAME = 0,
  DEAL_COL_INDEX = 1,
  DEAL_NUM_COLS
};

static void DealSelectCallback(GtkWidget *widget, gpointer data)
{
  GtkTreeIter iter;
  if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)) {
    GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
    gtk_tree_model_get(model, &iter, DEAL_COL_INDEX, &DealDialog.DrugInd, -1);
    UpdateDealDialog();
  }
}

static void DealOKCallback(GtkWidget *widget, gpointer data)
{
  GtkWidget *spinner;
  gint amount;
  gchar *text;

  spinner = DealDialog.amount;

  gtk_spin_button_update(GTK_SPIN_BUTTON(spinner));
  amount = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));

  text = g_strdup_printf("drug^%d^%d", DealDialog.DrugInd,
                         data == BT_BUY ? amount : -amount);

  gtk_widget_destroy(DealDialog.dialog);

  SendClientMessage(ClientData.Play, C_NONE, C_BUYOBJECT, NULL, text);
  g_free(text);
}

void DealDrugs(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog, *label, *hbox, *hbbox, *button, *spinner, *combo_box,
      *vbox, *hsep, *defbutton;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkAdjustment *spin_adj;
  GtkAccelGroup *accel_group;
  GtkWidget *tv;
  gchar *Action;
  GString *text;
  Player *Play;
  gint DrugInd, i, SelIndex, FirstInd;
  gboolean DrugIndOK;

  g_assert(!IsShowingDealDrugs);

  /* Action in 'Deal Drugs' dialog - "Buy/Sell/Drop Drugs" */
  if (data == BT_BUY) {
    Action = _("Buy");
  } else if (data == BT_SELL) {
    Action = _("Sell");
  } else if (data == BT_DROP) {
    Action = _("Drop");
  } else {
    g_warning("Bad DealDrug type");
    return;
  }

  DealDialog.Type = data;
  Play = ClientData.Play;

  if (data == BT_BUY) {
    tv = ClientData.Drug.HereList;
  } else {
    tv = ClientData.Drug.CarriedList;
  }
  DrugInd = get_selected_inventory(
                     gtk_tree_view_get_selection(GTK_TREE_VIEW(tv)));

  DrugIndOK = FALSE;
  FirstInd = -1;
  for (i = 0; i < NumDrug; i++) {
    if ((data == BT_DROP && Play->Drugs[i].Carried > 0
         && Play->Drugs[i].Price == 0)
        || (data == BT_SELL && Play->Drugs[i].Carried > 0
         && Play->Drugs[i].Price != 0)
        || (data == BT_BUY && Play->Drugs[i].Price != 0)) {
      if (FirstInd == -1) {
        FirstInd = i;
      }
      if (DrugInd == i) {
        DrugIndOK = TRUE;
      }
    }
  }
  if (!DrugIndOK) {
    if (FirstInd == -1) {
      return;
    } else {
      DrugInd = FirstInd;
    }
  }

  text = g_string_new(NULL);
  accel_group = gtk_accel_group_new();

  dialog = DealDialog.dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(dialog), Action);
  my_set_dialog_position(GTK_WINDOW(dialog));
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));
  SetShowing(dialog, &IsShowingDealDrugs);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);

  label = gtk_label_new(Action);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  store = gtk_list_store_new(DEAL_NUM_COLS, G_TYPE_STRING, G_TYPE_INT);
  SelIndex = -1;
  for (i = 0; i < NumDrug; i++) {
    if ((data == BT_DROP && Play->Drugs[i].Carried > 0
         && Play->Drugs[i].Price == 0)
        || (data == BT_SELL && Play->Drugs[i].Carried > 0
         && Play->Drugs[i].Price != 0)
        || (data == BT_BUY && Play->Drugs[i].Price != 0)) {
      dpg_string_printf(text, _("%/DealDrugs drug name/%tde"), Drug[i].Name);
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, DEAL_COL_NAME, text->str,
                         DEAL_COL_INDEX, i, -1);
      if (DrugInd >= i) {
        SelIndex++;
      }
    }
  }
  combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(store);
  renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), renderer,
                                 "text", DEAL_COL_NAME, NULL);
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), SelIndex);
  gtk_box_pack_start(GTK_BOX(hbox), combo_box, TRUE, TRUE, 0);

  DealDialog.DrugInd = DrugInd;

  label = DealDialog.cost = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = DealDialog.carrying = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  label = DealDialog.space = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  if (data == BT_BUY) {
    label = DealDialog.afford = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  }
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  if (data == BT_BUY) {
    /* Prompts for action in the "deal drugs" dialog */
    g_string_printf(text, _("Buy how many?"));
  } else if (data == BT_SELL) {
    g_string_printf(text, _("Sell how many?"));
  } else {
    g_string_printf(text, _("Drop how many?"));
  }
  label = gtk_label_new(text->str);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  spin_adj = (GtkAdjustment *)gtk_adjustment_new(1.0, 0.0, 2.0,
                                                 1.0, 10.0, 0.0);
  spinner = DealDialog.amount = gtk_spin_button_new(spin_adj, 1.0, 0);
  g_signal_connect(G_OBJECT(spinner), "activate",
                   G_CALLBACK(DealOKCallback), data);
  gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(DealOKCallback), data);
  gtk_widget_set_can_default(button, TRUE);
  defbutton = button;
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  button = gtk_button_new_with_mnemonic(_("_Cancel"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);

  g_signal_connect(G_OBJECT(combo_box), "changed",
                   G_CALLBACK(DealSelectCallback), NULL);

  g_string_free(text, TRUE);
  UpdateDealDialog();

  gtk_widget_show_all(dialog);
  gtk_widget_grab_default(defbutton);
}

void DealGuns(GtkWidget *widget, gpointer data)
{
  GtkWidget *tv, *dialog;
  gint GunInd;
  gchar *Title;
  GString *text;

  dialog = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

  if (data == BT_BUY) {
    tv = ClientData.Gun.HereList;
  } else {
    tv = ClientData.Gun.CarriedList;
  }
  GunInd = get_selected_inventory(
                     gtk_tree_view_get_selection(GTK_TREE_VIEW(tv)));
  if (GunInd < 0) {
    return;
  }


  /* Title of 'gun shop' dialog (%tde="guns" by default) "Buy/Sell/Drop
   * Guns" */
  if (data == BT_BUY) {
    Title = dpg_strdup_printf(_("Buy %tde"), Names.Guns);
  } else if (data == BT_SELL) {
    Title = dpg_strdup_printf(_("Sell %tde"), Names.Guns);
  } else {
    Title = dpg_strdup_printf(_("Drop %tde"), Names.Guns);
  }

  text = g_string_new("");

  if (data != BT_BUY && TotalGunsCarried(ClientData.Play) == 0) {
    dpg_string_printf(text, _("You don't have any %tde to sell!"),
                       Names.Guns);
    GtkMessageBox(dialog, text->str, Title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (data == BT_BUY && TotalGunsCarried(ClientData.Play) >=
             ClientData.Play->Bitches.Carried + 2) {
    dpg_string_printf(text,
                       _("You'll need more %tde to carry any more %tde!"),
                       Names.Bitches, Names.Guns);
    GtkMessageBox(dialog, text->str, Title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (data == BT_BUY
             && Gun[GunInd].Space > ClientData.Play->CoatSize) {
    dpg_string_printf(text,
                       _("You don't have enough space to carry that %tde!"),
                       Names.Gun);
    GtkMessageBox(dialog, text->str, Title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (data == BT_BUY && Gun[GunInd].Price > ClientData.Play->Cash) {
    dpg_string_printf(text,
                       _("You don't have enough cash to buy that %tde!"),
                       Names.Gun);
    GtkMessageBox(dialog, text->str, Title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (data == BT_SELL && ClientData.Play->Guns[GunInd].Carried == 0) {
    GtkMessageBox(dialog, _("You don't have any to sell!"), Title, 
                  GTK_MESSAGE_WARNING, MB_OK);
  } else {
    g_string_printf(text, "gun^%d^%d", GunInd, data == BT_BUY ? 1 : -1);
    SendClientMessage(ClientData.Play, C_NONE, C_BUYOBJECT, NULL,
                      text->str);
  }
  g_free(Title);
  g_string_free(text, TRUE);
}

static void QuestionCallback(GtkWidget *widget, gpointer data)
{
  gint Answer;
  gchar text[5];
  GtkWidget *dialog;
  Player *To;

  dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "dialog"));
  To = (Player *)g_object_get_data(G_OBJECT(dialog), "From");
  Answer = GPOINTER_TO_INT(data);

  text[0] = (gchar)Answer;
  text[1] = '\0';
  SendClientMessage(ClientData.Play, C_NONE, C_ANSWER, To, text);

  gtk_widget_destroy(dialog);
}

void QuestionDialog(char *Data, Player *From)
{
  GtkWidget *dialog, *label, *vbox, *hsep, *hbbox, *button;
  GtkAccelGroup *accel_group;
  gchar *Responses, **split, *LabelText, *trword, *underline;

  /* Button titles that correspond to the single-keypress options provided
     by the curses client (e.g. _Yes corresponds to 'Y' etc.) */
  gchar *Words[] = { N_("_Yes"), N_("_No"), N_("_Run"),
    N_("_Fight"), N_("_Attack"), N_("_Evade")
  };
  guint numWords = sizeof(Words) / sizeof(Words[0]);
  guint i, j;

  split = g_strsplit(Data, "^", 2);
  if (!split[0] || !split[1]) {
    g_warning("Bad QUESTION message %s", Data);
    return;
  }

  g_strdelimit(split[1], "^", '\n');

  Responses = split[0];
  LabelText = split[1];

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  g_signal_connect(G_OBJECT(dialog), "delete_event",
                   G_CALLBACK(DisallowDelete), NULL);
  g_object_set_data(G_OBJECT(dialog), "From", (gpointer)From);

  /* Title of the 'ask player a question' dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
  while (*LabelText == '\n')
    LabelText++;
  label = gtk_label_new(LabelText);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();

  for (i = 0; i < strlen(Responses); i++) {
    switch (Responses[i]) {
    case 'Y':
      button = gtk_button_new_with_mnemonic(_("_Yes"));
      break;
    case 'N':
      button = gtk_button_new_with_mnemonic(_("_No"));
      break;
    default:
      for (j = 0, trword = NULL; j < numWords && !trword; j++) {
        underline = strchr(Words[j], '_');
        if (underline && toupper(underline[1]) == Responses[i]) {
          trword = _(Words[j]);
        }
      }
      button = gtk_button_new_with_label("");
      if (trword) {
        SetAccelerator(button, trword, button, "clicked", accel_group, FALSE);
      } else {
        trword = g_strdup_printf("_%c", Responses[i]);
        SetAccelerator(button, trword, button, "clicked", accel_group, FALSE);
        g_free(trword);
      }
      break;
    }
    g_object_set_data(G_OBJECT(button), "dialog", (gpointer)dialog);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(QuestionCallback),
                     GINT_TO_POINTER((gint)Responses[i]));
    my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  }
  gtk_box_pack_start(GTK_BOX(vbox), hbbox, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);

  g_strfreev(split);
}

void GuiStartGame(void)
{
  Player *Play = ClientData.Play;

  if (!Network) {
    ClientData.cmdline->antique = WantAntique;
    InitConfiguration(ClientData.cmdline);
  }
  StripTerminators(GetPlayerName(Play));
  InitAbilities(Play);
  SendAbilities(Play);
  SendNullClientMessage(Play, C_NONE, C_NAME, NULL, GetPlayerName(Play));
  InGame = TRUE;
  UpdateMenus();
  gtk_widget_show_all(ClientData.vbox);
  UpdatePlayerLists();
  SoundPlay(Sounds.StartGame);
}

void EndGame(void)
{
  DisplayFightMessage(NULL);
  gtk_widget_hide(ClientData.vbox);
  TextViewClear(GTK_TEXT_VIEW(ClientData.messages));
  ShutdownNetwork(ClientData.Play);
  UpdatePlayerLists();
  CleanUpServer();
  RestoreConfig();
  InGame = FALSE;
  UpdateMenus();
  SoundPlay(Sounds.EndGame);
}

static gint DrugSortByName(GtkTreeModel *model, GtkTreeIter *a,
                           GtkTreeIter *b, gpointer data)
{
  int indexa, indexb;
  gtk_tree_model_get(model, a, INVEN_COL_INDEX, &indexa, -1);
  gtk_tree_model_get(model, b, INVEN_COL_INDEX, &indexb, -1);
  if (indexa < 0 || indexa >= NumDrug || indexb < 0 || indexb >= NumDrug) {
    return 0;
  }
  return g_ascii_strcasecmp(Drug[indexa].Name, Drug[indexb].Name);
}

static gint DrugSortByPrice(GtkTreeModel *model, GtkTreeIter *a,
                            GtkTreeIter *b, gpointer data)
{
  int indexa, indexb;
  price_t pricediff;
  gtk_tree_model_get(model, a, INVEN_COL_INDEX, &indexa, -1);
  gtk_tree_model_get(model, b, INVEN_COL_INDEX, &indexb, -1);
  if (indexa < 0 || indexa >= NumDrug || indexb < 0 || indexb >= NumDrug) {
    return 0;
  }
  pricediff = ClientData.Play->Drugs[indexa].Price -
              ClientData.Play->Drugs[indexb].Price;
  return pricediff == 0 ? 0 : pricediff < 0 ? -1 : 1;
}

void UpdateMenus(void)
{
  gboolean MultiPlayer;
  gint Bitches;

  MultiPlayer = (FirstClient && FirstClient->next != NULL);
  Bitches = InGame && ClientData.Play ? ClientData.Play->Bitches.Carried : 0;

  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget(ClientData.Menu,
                                                          "<main>/Talk"),
                           InGame && Network);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/Game/Options..."),
                           !InGame);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/Game/Abandon..."),
                           InGame);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/List/Inventory..."),
			   InGame);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/List/Players..."),
                           InGame && Network);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/Errands"), InGame);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/Errands/Spy..."),
                           InGame && MultiPlayer);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu, "<main>/Errands/Tipoff..."),
                           InGame && MultiPlayer);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu,
                            "<main>/Errands/Sack Bitch..."), Bitches > 0);
  gtk_widget_set_sensitive(dp_gtk_item_factory_get_widget
                           (ClientData.Menu,
                            "<main>/Errands/Get spy reports..."), InGame
                           && MultiPlayer);
}

GtkWidget *CreateStatusWidgets(struct StatusWidgets *Status)
{
  GtkWidget *grid, *label;

  grid = dp_gtk_grid_new(3, 6, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 3);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 3);

  label = Status->Location = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1, TRUE);

  label = Status->Date = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 2, 0, 2, 1, TRUE);

  /* Available space label in GTK+ client status display */
  label = Status->SpaceName = gtk_label_new(_("Space"));

  dp_gtk_grid_attach(GTK_GRID(grid), label, 4, 0, 1, 1, TRUE);
  label = Status->SpaceValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 5, 0, 1, 1, TRUE);

  /* Player's cash label in GTK+ client status display */
  label = Status->CashName = gtk_label_new(_("Cash"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1, TRUE);

  label = Status->CashValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 1, 1, TRUE);

  /* Player's debt label in GTK+ client status display */
  label = Status->DebtName = gtk_label_new(_("Debt"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 2, 1, 1, 1, TRUE);

  label = Status->DebtValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 3, 1, 1, 1, TRUE);

  /* Player's bank balance label in GTK+ client status display */
  label = Status->BankName = gtk_label_new(_("Bank"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 4, 1, 1, 1, TRUE);

  label = Status->BankValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 5, 1, 1, 1, TRUE);

  label = Status->GunsName = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1, TRUE);
  label = Status->GunsValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 1, 1, TRUE);

  label = Status->BitchesName = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 2, 2, 1, 1, TRUE);
  label = Status->BitchesValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 3, 2, 1, 1, TRUE);

  /* Player's health label in GTK+ client status display */
  label = Status->HealthName = gtk_label_new(_("Health"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 4, 2, 1, 1, TRUE);

  label = Status->HealthValue = gtk_label_new(NULL);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 5, 2, 1, 1, TRUE);
  return grid;
}

void SetJetButtonTitle(GtkAccelGroup *accel_group)
{
  GtkWidget *button;
  guint accel_key;
  gchar *caption;

  button = ClientData.JetButton;
  accel_key = ClientData.JetAccel;

  if (accel_key) {
    gtk_widget_remove_accelerator(button, accel_group, accel_key, 0);
  }

  if (ClientData.Play && ClientData.Play->Flags & FIGHTING) {
    caption = _("_Fight");
  } else {
    /* Caption of 'Jet' button in main window */
    caption = _("_Jet!");
  }
  ClientData.JetAccel = SetAccelerator(button, caption, button,
                                       "clicked", accel_group, FALSE);
}

static void SetIcon(GtkWidget *window, char **xpmdata)
{
#ifndef CYGWIN
  GdkPixbuf *icon;
  icon = gdk_pixbuf_new_from_xpm_data((const char**)xpmdata);
  gtk_window_set_icon(GTK_WINDOW(window), icon);
#endif
}

static void make_tags(GtkTextView *textview)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(textview);

  gtk_text_buffer_create_tag(buffer, "jet", "foreground",
                             "#00000000FFFF", NULL);
  gtk_text_buffer_create_tag(buffer, "talk", "foreground",
                             "#FFFF00000000", NULL);
  gtk_text_buffer_create_tag(buffer, "page", "foreground",
                             "#FFFF0000FFFF", NULL);
  gtk_text_buffer_create_tag(buffer, "join", "foreground",
                             "#000000008B8B", NULL);
  gtk_text_buffer_create_tag(buffer, "leave", "foreground",
                             "#8B8B00000000", NULL);
}

#ifdef CYGWIN
gboolean GtkLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                 struct CMDLINE *cmdline, gboolean ReturnOnFail)
#else
gboolean GtkLoop(int *argc, char **argv[],
                 struct CMDLINE *cmdline, gboolean ReturnOnFail)
#endif
{
  GtkWidget *window, *vbox, *vbox2, *hbox, *frame, *grid, *menubar, *text,
      *vpaned, *button, *tv, *widget;
  GtkAccelGroup *accel_group;
  GtkTreeSortable *sortable;
  int i;
  DPGtkItemFactory *item_factory;
  gint nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);

#ifdef CYGWIN
  win32_init(hInstance, hPrevInstance, "mainicon");
#else
  if (ReturnOnFail && !gtk_init_check(argc, argv))
    return FALSE;
  else if (!ReturnOnFail)
    gtk_init(argc, argv);
#endif

  /* GTK+2 (and the GTK emulation code on WinNT systems) expects all
   * strings to be UTF-8, so we force gettext to return all translations
   * in this encoding here. */
  bind_textdomain_codeset(PACKAGE, "UTF-8");

  Conv_SetInternalCodeset("UTF-8");
  WantUTF8Errors(TRUE);

  InitConfiguration(cmdline);
  ClientData.cmdline = cmdline;

  /* Set up message handlers */
  ClientMessageHandlerPt = HandleClientMessage;

  if (!CheckHighScoreFileConfig()) {
    return TRUE;
  }

  /* Have the GLib log messages pop up in a nice dialog box */
  g_log_set_handler(NULL,
                    G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
                    G_LOG_LEVEL_CRITICAL, LogMessage, NULL);

  SoundOpen(cmdline->plugin);

  /* Create the main player */
  ClientData.Play = g_new(Player, 1);
  FirstClient = AddPlayer(0, ClientData.Play, FirstClient);
  if (PlayerName && PlayerName[0]) {
    SetPlayerName(ClientData.Play, PlayerName);
  }

  window = MainWindow = ClientData.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  /* Title of main window in GTK+ client */
  gtk_window_set_title(GTK_WINDOW(window), _("dopewars"));
  gtk_window_set_default_size(GTK_WINDOW(window), 450, 390);
  g_signal_connect(G_OBJECT(window), "delete_event",
                   G_CALLBACK(MainDelete), NULL);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(DestroyGtk), NULL);

  accel_group = gtk_accel_group_new();
  g_object_set_data(G_OBJECT(window), "accel_group", accel_group);
  item_factory = ClientData.Menu = dp_gtk_item_factory_new("<main>",
                                                           accel_group);
  dp_gtk_item_factory_set_translate_func(item_factory, MenuTranslate, NULL,
                                         NULL);

  dp_gtk_item_factory_create_items(item_factory, nmenu_items, menu_items,
                                   NULL);
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
  menubar = dp_gtk_item_factory_get_widget(item_factory, "<main>");

  vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), menubar, FALSE, FALSE, 0);
  gtk_widget_show_all(menubar);
  UpdateMenus();
  SoundEnable(UseSounds);
  widget = dp_gtk_item_factory_get_widget(ClientData.Menu,
                                          "<main>/Game/Enable sound");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), UseSounds);

  vbox = ClientData.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  frame = gtk_frame_new(_("Stats"));
  gtk_container_set_border_width(GTK_CONTAINER(frame), 3);

  grid = CreateStatusWidgets(&ClientData.Status);

  gtk_container_add(GTK_CONTAINER(frame), grid);

  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

  vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

  text = ClientData.messages = gtk_scrolled_text_view_new(&hbox);
  make_tags(GTK_TEXT_VIEW(text));
  gtk_widget_set_size_request(text, 100, 80);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
  gtk_paned_pack1(GTK_PANED(vpaned), hbox, TRUE, TRUE);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  CreateInventory(hbox, Names.Drugs, accel_group, TRUE, TRUE,
                  &ClientData.Drug, G_CALLBACK(DealDrugs));
  tv = ClientData.Drug.HereList;
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tv), TRUE);
  sortable = GTK_TREE_SORTABLE(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)));
  gtk_tree_sortable_set_sort_func(sortable, 0, DrugSortByName, NULL, NULL);
  gtk_tree_sortable_set_sort_func(sortable, 1, DrugSortByPrice, NULL, NULL);
  for (i = 0; i < 2; ++i) {
    GtkTreeViewColumn *col = gtk_tree_view_get_column(GTK_TREE_VIEW(tv), i);
    gtk_tree_view_column_set_sort_column_id(col, i);
  }

  button = ClientData.JetButton = gtk_button_new_with_label("");
  ClientData.JetAccel = 0;
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(JetButtonPressed), NULL);
  gtk_box_pack_start(GTK_BOX(ClientData.Drug.vbbox), button, TRUE, TRUE, 0);
  SetJetButtonTitle(accel_group);

#ifdef CYGWIN
  /* GtkFrames don't look quite right in Win32 yet */
  gtk_paned_pack2(GTK_PANED(vpaned), hbox, TRUE, TRUE);
#else
  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(frame), hbox);
  gtk_paned_pack2(GTK_PANED(vpaned), frame, TRUE, TRUE);
#endif

  gtk_box_pack_start(GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox2), vbox, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox2);

  /* Just show the window, not the vbox - we'll do that when the game
   * starts */
  gtk_widget_show(vbox2);
  gtk_widget_show(window);

  gtk_widget_realize(window);

  SetIcon(window, dopewars_pill_xpm);

#ifdef NETWORKING
  CurlInit(&MetaConn);
#endif

  gtk_main();

#ifdef NETWORKING
  CurlCleanup(&MetaConn);
#endif

  /* Free the main player */
  FirstClient = RemovePlayer(ClientData.Play, FirstClient);

  return TRUE;
}

static void PackCentredURL(GtkWidget *vbox, gchar *title, gchar *target,
                           gchar *browser)
{
  GtkWidget *hbox, *label, *url;

  /* There must surely be a nicer way of making the URL centred - but I
   * can't think of one... */
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  url = gtk_url_new(title, target, browser);
  gtk_box_pack_start(GTK_BOX(hbox), url, FALSE, FALSE, 0);

  label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
}

void display_intro(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog, *label, *grid, *OKButton, *vbox, *hsep, *hbbox;
  gchar *VersionStr, *docindex;
  const int rows = 8, cols = 3;
  int i, j;
  GtkAccelGroup *accel_group;
  gchar *table_data[8][3] = {
    /* Credits labels in GTK+ 'about' dialog */
    {N_("English Translation"), N_("Ben Webb"), NULL},
    {N_("Icons and graphics"), "Ocelot Mantis", NULL},
    {N_("Sounds"), "Robin Kohli, 19.5degs.com", NULL},
    {N_("Drug Dealing and Research"), "Dan Wolf", NULL},
    {N_("Play Testing"), "Phil Davis", "Owen Walsh"},
    {N_("Extensive Play Testing"), "Katherine Holt",
     "Caroline Moore"},
    {N_("Constructive Criticism"), "Andrea Elliot-Smith",
     "Pete Winn"},
    {N_("Unconstructive Criticism"), "James Matthews", NULL}
  };

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  /* Title of GTK+ 'about' dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("About dopewars"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

  /* Main content of GTK+ 'about' dialog */
  label = gtk_label_new(_("Based on John E. Dell's old Drug Wars game, "
                          "dopewars is a simulation of an\nimaginary drug "
                          "market.  dopewars is an All-American game which "
                          "features\nbuying, selling, and trying to get "
                          "past the cops!\n\nThe first thing you need to "
                          "do is pay off your debt to the Loan Shark. "
                          "After\nthat, your goal is to make as much "
                          "money as possible (and stay alive)! You\n"
                          "have one month of game time to make "
                          "your fortune.\n"));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  /* Version and copyright notice in GTK+ 'about' dialog */
  VersionStr = g_strdup_printf(_("Version %s     "
                                 "Copyright (C) 1998-2022  "
                                 "Ben Webb benwebb@users.sf.net\n"
                                 "dopewars is released under the "
                                 "GNU General Public License\n"), VERSION);
  label = gtk_label_new(VersionStr);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  g_free(VersionStr);

  grid = dp_gtk_grid_new(rows, cols, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 3);
  for (i = 0; i < rows; i++) {
    if (i > 0 || strcmp(_(table_data[i][1]), "Ben Webb") != 0) {
      for (j = 0; j < cols; j++) {
        if (table_data[i][j]) {
          if (j == 0 || i == 0) {
            label = gtk_label_new(_(table_data[i][j]));
          } else {
            label = gtk_label_new(table_data[i][j]);
          }
          dp_gtk_grid_attach(GTK_GRID(grid), label, j, i, 1, 1, TRUE);
        }
      }
    }
  }
  gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

  /* Label at the bottom of GTK+ 'about' dialog */
  label = gtk_label_new(_("\nFor information on the command line "
                          "options, type dopewars -h at your\n"
                          "Unix prompt. This will display a help "
                          "screen, listing the available options.\n"));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  docindex = GetDocIndex();
  PackCentredURL(vbox, _("Local HTML documentation"), docindex, OurWebBrowser);
  g_free(docindex);

  PackCentredURL(vbox, "https://dopewars.sourceforge.io/",
                 "https://dopewars.sourceforge.io/", OurWebBrowser);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  OKButton = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect_swapped(G_OBJECT(OKButton), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), OKButton);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);

  gtk_widget_set_can_default(OKButton, TRUE);
  gtk_widget_grab_default(OKButton);

  gtk_widget_show_all(dialog);
}

static void SendDoneMessage(GtkWidget *widget, gpointer data)
{
  SendClientMessage(ClientData.Play, C_NONE, C_DONE, NULL, NULL);
}

static void TransferPayAll(GtkWidget *widget, GtkWidget *dialog)
{
  gchar *text;

  text = pricetostr(ClientData.Play->Debt);
  SendClientMessage(ClientData.Play, C_NONE, C_PAYLOAN, NULL, text);
  g_free(text);
  gtk_widget_destroy(dialog);
}

static void TransferOK(GtkWidget *widget, GtkWidget *dialog)
{
  gpointer Debt;
  GtkWidget *deposit, *entry;
  gchar *text, *title;
  price_t money;
  gboolean withdraw = FALSE;

  Debt = g_object_get_data(G_OBJECT(dialog), "debt");
  entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "entry"));
  text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  money = strtoprice(text);
  g_free(text);

  if (Debt) {
    /* Title of loan shark dialog - (%Tde="The Loan Shark" by default) */
    title = dpg_strdup_printf(_("%/LoanShark window title/%Tde"),
                              Names.LoanSharkName);
    if (money > ClientData.Play->Debt) {
      money = ClientData.Play->Debt;
    }
  } else {
    /* Title of bank dialog - (%Tde="The Bank" by default) */
    title = dpg_strdup_printf(_("%/BankName window title/%Tde"),
                              Names.BankName);
    deposit = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "deposit"));
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(deposit))) {
      withdraw = TRUE;
    }
  }

  if (money < 0) {
    GtkMessageBox(dialog, _("You must enter a positive amount of money!"),
                  title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (!Debt && withdraw && money > ClientData.Play->Bank) {
    GtkMessageBox(dialog, _("There isn't that much money available..."),
                  title, GTK_MESSAGE_WARNING, MB_OK);
  } else if (!withdraw && money > ClientData.Play->Cash) {
    GtkMessageBox(dialog, _("You don't have that much money!"),
                  title, GTK_MESSAGE_WARNING, MB_OK);
  } else {
    text = pricetostr(withdraw ? -money : money);
    SendClientMessage(ClientData.Play, C_NONE,
                      Debt ? C_PAYLOAN : C_DEPOSIT, NULL, text);
    g_free(text);
    gtk_widget_destroy(dialog);
  }
  g_free(title);
}

void TransferDialog(gboolean Debt)
{
  GtkWidget *dialog, *button, *label, *radio, *grid, *vbox;
  GtkWidget *hbbox, *hsep, *entry;
  GtkAccelGroup *accel_group;
  GSList *group;
  GString *text;

  text = g_string_new("");

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  g_signal_connect(G_OBJECT(dialog), "destroy",
                   G_CALLBACK(SendDoneMessage), NULL);
  if (Debt) {
    /* Title of loan shark dialog - (%Tde="The Loan Shark" by default) */
    dpg_string_printf(text, _("%/LoanShark window title/%Tde"),
                       Names.LoanSharkName);
  } else {
    /* Title of bank dialog - (%Tde="The Bank" by default) */
    dpg_string_printf(text, _("%/BankName window title/%Tde"),
                       Names.BankName);
  }
  gtk_window_set_title(GTK_WINDOW(dialog), text->str);
  my_set_dialog_position(GTK_WINDOW(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
  grid = dp_gtk_grid_new(4, 3, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 4);

  /* Display of player's cash in bank or loan shark dialog */
  dpg_string_printf(text, _("Cash: %P"), ClientData.Play->Cash);
  label = gtk_label_new(text->str);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 3, 1, TRUE);

  if (Debt) {
    /* Display of player's debt in loan shark dialog */
    dpg_string_printf(text, _("Debt: %P"), ClientData.Play->Debt);
  } else {
    /* Display of player's bank balance in bank dialog */
    dpg_string_printf(text, _("Bank: %P"), ClientData.Play->Bank);
  }
  label = gtk_label_new(text->str);
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 3, 1, TRUE);

  g_object_set_data(G_OBJECT(dialog), "debt", GINT_TO_POINTER(Debt));
  if (Debt) {
    /* Prompt for paying back a loan */
    label = gtk_label_new(_("Pay back:"));
    dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 2, FALSE);
  } else {
    /* Radio button selected if you want to pay money into the bank */
    radio = gtk_radio_button_new_with_label(NULL, _("Deposit"));
    g_object_set_data(G_OBJECT(dialog), "deposit", radio);
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
    dp_gtk_grid_attach(GTK_GRID(grid), radio, 0, 2, 1, 1, FALSE);

    /* Radio button selected if you want to withdraw money from the bank */
    radio = gtk_radio_button_new_with_label(group, _("Withdraw"));
    dp_gtk_grid_attach(GTK_GRID(grid), radio, 0, 3, 1, 1, FALSE);
  }
  label = gtk_label_new(Currency.Symbol);
  entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry), "0");
  g_object_set_data(G_OBJECT(dialog), "entry", entry);
  g_signal_connect(G_OBJECT(entry), "activate",
                   G_CALLBACK(TransferOK), dialog);

  if (Currency.Prefix) {
    dp_gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 1, 2, FALSE);
    dp_gtk_grid_attach(GTK_GRID(grid), entry, 2, 2, 1, 2, TRUE);
  } else {
    dp_gtk_grid_attach(GTK_GRID(grid), label, 2, 2, 1, 2, FALSE);
    dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 2, 1, 2, TRUE);
  }

  gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(TransferOK), dialog);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  if (Debt && ClientData.Play->Cash >= ClientData.Play->Debt) {
    /* Button to pay back the entire loan/debt */
    button = gtk_button_new_with_label(_("Pay all"));
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(TransferPayAll), dialog);
    my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  }
  button = gtk_button_new_with_mnemonic(_("_Cancel"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(dialog), vbox);

  gtk_widget_show_all(dialog);

  g_string_free(text, TRUE);
}

void ListPlayers(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog, *clist, *button, *vbox, *hsep, *hbbox;
  GtkAccelGroup *accel_group;

  if (IsShowingPlayerList)
    return;
  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  /* Title of player list dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("Player List"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_window_set_default_size(GTK_WINDOW(dialog), 200, 180);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);

  gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));
  SetShowing(dialog, &IsShowingPlayerList);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  clist = ClientData.PlayerList = CreatePlayerList();
  UpdatePlayerList(clist, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox), clist, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);
}

struct TalkStruct {
  GtkWidget *dialog, *clist, *entry, *checkbutton;
};

/* Columns in player list */
enum {
  PLAYER_COL_NAME = 0,
  PLAYER_COL_PT,
  PLAYER_NUM_COLS
};

static void TalkSendSelected(GtkTreeModel *model, GtkTreePath *path,
                             GtkTreeIter *iter, gpointer data)
{
  Player *Play;
  gchar *text = data;
  gtk_tree_model_get(model, iter, PLAYER_COL_PT, &Play, -1);
  if (Play) {
    gchar *msg = g_strdup_printf(
                     "%s->%s: %s", GetPlayerName(ClientData.Play),
                     GetPlayerName(Play), text);
    SendClientMessage(ClientData.Play, C_NONE, C_MSGTO, Play, text);
    PrintMessage(msg, "page");
    g_free(msg);
  }
}

static void TalkSend(GtkWidget *widget, struct TalkStruct *TalkData)
{
  gboolean AllPlayers;
  gchar *text;
  GString *msg;

  AllPlayers =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
                                   (TalkData->checkbutton));
  text = gtk_editable_get_chars(GTK_EDITABLE(TalkData->entry), 0, -1);
  gtk_editable_delete_text(GTK_EDITABLE(TalkData->entry), 0, -1);
  if (!text)
    return;

  msg = g_string_new("");

  if (AllPlayers) {
    SendClientMessage(ClientData.Play, C_NONE, C_MSG, NULL, text);
    g_string_printf(msg, "%s: %s", GetPlayerName(ClientData.Play), text);
    PrintMessage(msg->str, "talk");
  } else {
    GtkTreeSelection *tsel = gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW(TalkData->clist));
    gtk_tree_selection_selected_foreach(tsel, TalkSendSelected, text);
  }
  g_free(text);
  g_string_free(msg, TRUE);
}

void TalkToAll(GtkWidget *widget, gpointer data)
{
  TalkDialog(TRUE);
}

void TalkToPlayers(GtkWidget *widget, gpointer data)
{
  TalkDialog(FALSE);
}

void TalkDialog(gboolean TalkToAll)
{
  GtkWidget *dialog, *clist, *button, *entry, *label, *vbox, *hsep,
      *checkbutton, *hbbox;
  GtkAccelGroup *accel_group;
  static struct TalkStruct TalkData;

  if (IsShowingTalkList)
    return;
  dialog = TalkData.dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  /* Title of talk dialog */
  gtk_window_set_title(GTK_WINDOW(dialog), _("Talk to player(s)"));
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_window_set_default_size(GTK_WINDOW(dialog), 200, 190);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);

  gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));
  SetShowing(dialog, &IsShowingTalkList);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  clist = TalkData.clist = ClientData.TalkList = CreatePlayerList();
  UpdatePlayerList(clist, FALSE);
  gtk_tree_selection_set_mode(
          gtk_tree_view_get_selection(GTK_TREE_VIEW(clist)),
          GTK_SELECTION_MULTIPLE);
  gtk_box_pack_start(GTK_BOX(vbox), clist, TRUE, TRUE, 0);

  checkbutton = TalkData.checkbutton =
      /* Checkbutton set if you want to talk to all players */
      gtk_check_button_new_with_label(_("Talk to all players"));

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), TalkToAll);
  gtk_box_pack_start(GTK_BOX(vbox), checkbutton, FALSE, FALSE, 0);

  /* Prompt for you to enter the message to be sent to other players */
  label = gtk_label_new(_("Message:-"));

  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  entry = TalkData.entry = gtk_entry_new();
  g_signal_connect(G_OBJECT(entry), "activate",
                   G_CALLBACK(TalkSend), (gpointer)&TalkData);
  gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();

  /* Button to send a message to other players */
  button = gtk_button_new_with_label(_("Send"));

  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(TalkSend), (gpointer)&TalkData);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);
}

GtkWidget *CreatePlayerList(void)
{
  GtkWidget *view;
  GtkListStore *store;
  GtkCellRenderer *renderer;

  store = gtk_list_store_new(PLAYER_NUM_COLS, G_TYPE_STRING, G_TYPE_POINTER);

  view = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(
               GTK_TREE_VIEW(view), -1, "Name", renderer, "text",
               PLAYER_COL_NAME, NULL);
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
  g_object_unref(store);  /* so it is freed when the view is */
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), FALSE);
  return view;
}

void UpdatePlayerList(GtkWidget *clist, gboolean IncludeSelf)
{
  GtkListStore *store;
  GSList *list;
  GtkTreeIter iter;
  Player *Play;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));

  /* Don't update the widget until we're done */
  g_object_ref(store);
  gtk_tree_view_set_model(GTK_TREE_VIEW(clist), NULL);

  gtk_list_store_clear(store);
  for (list = FirstClient; list; list = g_slist_next(list)) {
    Play = (Player *)list->data;
    if (IncludeSelf || Play != ClientData.Play) {
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, PLAYER_COL_NAME, GetPlayerName(Play),
                         PLAYER_COL_PT, Play, -1);
    }
  }

  gtk_tree_view_set_model(GTK_TREE_VIEW(clist), GTK_TREE_MODEL(store));
  g_object_unref(store);
}

static void ErrandOK(GtkWidget *widget, GtkWidget *clist)
{
  GtkTreeSelection *treesel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *dialog;
  gint ErrandType;


  dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "dialog"));
  ErrandType = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
                                                 "errandtype"));
  treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(clist));
  if (gtk_tree_selection_get_selected(treesel, &model, &iter)) {
    Player *Play;
    gtk_tree_model_get(model, &iter, PLAYER_COL_PT, &Play, -1);
    if (ErrandType == ET_SPY) {
      SendClientMessage(ClientData.Play, C_NONE, C_SPYON, Play, NULL);
    } else {
      SendClientMessage(ClientData.Play, C_NONE, C_TIPOFF, Play, NULL);
    }
    gtk_widget_destroy(dialog);
  }
}

void SpyOnPlayer(GtkWidget *widget, gpointer data)
{
  ErrandDialog(ET_SPY);
}

void TipOff(GtkWidget *widget, gpointer data)
{
  ErrandDialog(ET_TIPOFF);
}

void ErrandDialog(gint ErrandType)
{
  GtkWidget *dialog, *clist, *button, *vbox, *hbbox, *hsep, *label;
  GtkAccelGroup *accel_group;
  gchar *text;

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(ClientData.window));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  if (ErrandType == ET_SPY) {
    /* Title of dialog to select a player to spy on */
    gtk_window_set_title(GTK_WINDOW(dialog), _("Spy On Player"));

    /* Informative text for "spy on player" dialog. (%tde = "bitch",
       "bitch", "guns", "drugs", respectively, by default) */
    text = dpg_strdup_printf(_("Please choose the player to spy on. "
                               "Your %tde will\nthen offer his "
                               "services to the player, and if "
                               "successful,\nyou will be able to "
                               "view the player's stats with the\n"
                               "\"Get spy reports\" menu. Remember "
                               "that the %tde will leave\nyou, so "
                               "any %tde or %tde that he's "
                               "carrying may be lost!"), Names.Bitch,
                             Names.Bitch, Names.Guns, Names.Drugs);
    label = gtk_label_new(text);
    g_free(text);
  } else {

    /* Title of dialog to select a player to tip the cops off to */
    gtk_window_set_title(GTK_WINDOW(dialog), _("Tip Off The Cops"));

    /* Informative text for "tip off cops" dialog. (%tde = "bitch",
       "bitch", "guns", "drugs", respectively, by default) */
    text = dpg_strdup_printf(_("Please choose the player to tip off "
                               "the cops to. Your %tde will\nhelp "
                               "the cops to attack that player, "
                               "and then report back to you\non "
                               "the encounter. Remember that the "
                               "%tde will leave you temporarily,\n"
                               "so any %tde or %tde that he's "
                               "carrying may be lost!"), Names.Bitch,
                             Names.Bitch, Names.Guns, Names.Drugs);
    label = gtk_label_new(text);
    g_free(text);
  }
  my_set_dialog_position(GTK_WINDOW(dialog));

  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  clist = ClientData.PlayerList = CreatePlayerList();
  UpdatePlayerList(clist, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox), clist, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_object_set_data(G_OBJECT(button), "dialog", dialog);
  g_object_set_data(G_OBJECT(button), "errandtype",
                    GINT_TO_POINTER(ErrandType));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(ErrandOK), (gpointer)clist);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);
  button = gtk_button_new_with_mnemonic(_("_Cancel"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_widget_show_all(dialog);
}

void SackBitch(GtkWidget *widget, gpointer data)
{
  char *title, *text;

  /* Cannot sack bitches if you don't have any! */
  if (ClientData.Play->Bitches.Carried <= 0)
    return;

  /* Title of dialog to sack a bitch (%Tde = "Bitch" by default) */
  title = dpg_strdup_printf(_("%/Sack Bitch dialog title/Sack %Tde"),
                            Names.Bitch);

  /* Confirmation message for sacking a bitch. (%tde = "guns", "drugs",
     "bitch", respectively, by default) */
  text = dpg_strdup_printf(_("Are you sure? (Any %tde or %tde carried\n"
                             "by this %tde may be lost!)"), Names.Guns,
                           Names.Drugs, Names.Bitch);

  if (GtkMessageBox(ClientData.window, text, title, GTK_MESSAGE_QUESTION,
                    MB_YESNO) == IDYES) {
    ClientData.Play->Bitches.Carried--;
    UpdateMenus();
    SendClientMessage(ClientData.Play, C_NONE, C_SACKBITCH, NULL, NULL);
  }
  g_free(text);
  g_free(title);
}

void CreateInventory(GtkWidget *hbox, gchar *Objects,
                     GtkAccelGroup *accel_group, gboolean CreateButtons,
                     gboolean CreateHere, struct InventoryWidgets *widgets,
                     GCallback CallBack)
{
  GtkWidget *scrollwin, *tv, *vbbox, *frame[2], *button[3];
  GtkCellRenderer *renderer;
  GtkListStore *store;
  GtkTreeSelection *treesel;
  gint i, mini, icol;
  GString *text;
  gchar *titles[2][2];
  gchar *button_text[3];
  gpointer button_type[3] = { BT_BUY, BT_SELL, BT_DROP };

  /* Column titles for display of drugs/guns carried or available for
     purchase */
  titles[0][0] = titles[1][0] = _("Name");
  titles[0][1] = _("Price");
  titles[1][1] = _("Number");

  /* Button titles for buying/selling/dropping guns or drugs */
  button_text[0] = _("_Buy ->");
  button_text[1] = _("<- _Sell");
  button_text[2] = _("_Drop <-");

  text = g_string_new("");

  if (CreateHere) {
    /* Title of the display of available drugs/guns (%Tde = "Guns" or
       "Drugs" by default) */
    dpg_string_printf(text, _("%Tde here"), Objects);
    widgets->HereFrame = frame[0] = gtk_frame_new(text->str);
  }

  /* Title of the display of carried drugs/guns (%Tde = "Guns" or "Drugs"
     by default) */
  dpg_string_printf(text, _("%Tde carried"), Objects);

  widgets->CarriedFrame = frame[1] = gtk_frame_new(text->str);

  widgets->HereList = widgets->CarriedList = NULL;
  mini = (CreateHere ? 0 : 1);
  for (i = mini; i < 2; i++) {
    GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(hbox2), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(frame[i]), 3);

    tv = gtk_scrolled_tree_view_new(&scrollwin);
    renderer = gtk_cell_renderer_text_new();
    store = gtk_list_store_new(INVEN_NUM_COLS, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_INT);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tv), GTK_TREE_MODEL(store));
    g_object_unref(store);
    for (icol = 0; icol < 2; ++icol) {
      GtkTreeViewColumn *col;
      if (i == 0 && icol == 1) {
        /* Right align prices */
        GtkCellRenderer *rren = gtk_cell_renderer_text_new();
        g_object_set(G_OBJECT(rren), "xalign", 1.0, NULL);
        col = gtk_tree_view_column_new_with_attributes(
                       titles[i][icol], rren, "text", icol, NULL);
        gtk_tree_view_column_set_alignment(col, 1.0);
      } else {
        col = gtk_tree_view_column_new_with_attributes(
                       titles[i][icol], renderer, "text", icol, NULL);
      }
      gtk_tree_view_insert_column(GTK_TREE_VIEW(tv), col, -1);
    }
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tv), FALSE);
    treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
    gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
    gtk_box_pack_start(GTK_BOX(hbox2), scrollwin, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox2), 3);
    gtk_container_add(GTK_CONTAINER(frame[i]), hbox2);
    if (i == 0) {
      widgets->HereList = tv;
    } else {
      widgets->CarriedList = tv;
    }
  }
  if (CreateHere) {
    gtk_box_pack_start(GTK_BOX(hbox), frame[0], TRUE, TRUE, 0);
  }

  if (CreateButtons) {
    widgets->vbbox = vbbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(vbbox), GTK_BUTTONBOX_SPREAD);


    for (i = 0; i < 3; i++) {
      button[i] = gtk_button_new_with_label("");
      SetAccelerator(button[i], _(button_text[i]), button[i],
                     "clicked", accel_group, FALSE);
      if (CallBack) {
        g_signal_connect(G_OBJECT(button[i]), "clicked",
                         G_CALLBACK(CallBack), button_type[i]);
      }
      gtk_box_pack_start(GTK_BOX(vbbox), button[i], TRUE, TRUE, 0);
    }
    widgets->BuyButton = button[0];
    widgets->SellButton = button[1];
    widgets->DropButton = button[2];
    gtk_box_pack_start(GTK_BOX(hbox), vbbox, FALSE, FALSE, 0);
  } else {
    widgets->vbbox = NULL;
  }

  gtk_box_pack_start(GTK_BOX(hbox), frame[1], TRUE, TRUE, 0);
  g_string_free(text, TRUE);
}

void SetShowing(GtkWidget *window, gboolean *showing)
{
  g_assert(showing);

  *showing = TRUE;
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(DestroyShowing), (gpointer)showing);
}

void DestroyShowing(GtkWidget *widget, gpointer data)
{
  gboolean *IsShowing = (gboolean *)data;

  if (IsShowing) {
    *IsShowing = FALSE;
  }
}

static void NewNameOK(GtkWidget *widget, GtkWidget *window)
{
  GtkWidget *entry;
  gchar *text;

  entry = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "entry"));
  text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  if (text[0]) {
    StripTerminators(text);
    SetPlayerName(ClientData.Play, text);
    SendNullClientMessage(ClientData.Play, C_NONE, C_NAME, NULL, text);
    gtk_widget_destroy(window);
  }
  g_free(text);
}

void NewNameDialog(void)
{
  GtkWidget *window, *button, *hsep, *vbox, *label, *entry;
  GtkAccelGroup *accel_group;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Title of dialog for changing a player's name */
  gtk_window_set_title(GTK_WINDOW(window), _("Change Name"));
  my_set_dialog_position(GTK_WINDOW(window));

  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(window),
                               GTK_WINDOW(ClientData.window));
  gtk_container_set_border_width(GTK_CONTAINER(window), 7);
  g_signal_connect(G_OBJECT(window), "delete_event",
                   G_CALLBACK(DisallowDelete), NULL);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  /* Informational text to prompt the player to change his/her name */
  label = gtk_label_new(_("Unfortunately, somebody else is already "
                          "using \"your\" name. Please change it:-"));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new();
  g_object_set_data(G_OBJECT(window), "entry", entry);
  g_signal_connect(G_OBJECT(entry), "activate",
                   G_CALLBACK(NewNameOK), window);
  gtk_entry_set_text(GTK_ENTRY(entry), GetPlayerName(ClientData.Play));
  gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(NewNameOK), window);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_set_can_default(button, TRUE);
  gtk_widget_grab_default(button);

  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show_all(window);
}

gint DisallowDelete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  return TRUE;
}

void GunShopDialog(void)
{
  GtkWidget *window, *button, *hsep, *vbox, *hbox, *hbbox;
  GtkAccelGroup *accel_group;
  gchar *text;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 600, 190);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(SendDoneMessage), NULL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Title of 'gun shop' dialog in GTK+ client (%Tde="Dan's House of Guns"
     by default) */
  text = dpg_strdup_printf(_("%/GTK GunShop window title/%Tde"),
                           Names.GunShopName);
  gtk_window_set_title(GTK_WINDOW(window), text);
  my_set_dialog_position(GTK_WINDOW(window));
  g_free(text);
  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(window),
                               GTK_WINDOW(ClientData.window));
  gtk_container_set_border_width(GTK_CONTAINER(window), 7);
  SetShowing(window, &IsShowingGunShop);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  CreateInventory(hbox, Names.Guns, accel_group, TRUE, TRUE,
                  &ClientData.Gun, G_CALLBACK(DealGuns));

  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();
  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(window));
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  UpdateInventory(&ClientData.Gun, ClientData.Play->Guns, NumGun, FALSE);
  gtk_widget_show_all(window);
}

void UpdatePlayerLists(void)
{
  if (IsShowingPlayerList) {
    UpdatePlayerList(ClientData.PlayerList, FALSE);
  }
  if (IsShowingTalkList) {
    UpdatePlayerList(ClientData.TalkList, FALSE);
  }
}

void GetSpyReports(GtkWidget *Widget, gpointer data)
{
  SendClientMessage(ClientData.Play, C_NONE, C_CONTACTSPY, NULL, NULL);
}

static void DestroySpyReports(GtkWidget *widget, gpointer data)
{
  SpyReportsDialog = NULL;
}

static void CreateSpyReports(void)
{
  GtkWidget *window, *button, *vbox, *notebook;
  GtkAccelGroup *accel_group;

  SpyReportsDialog = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  g_object_set_data(G_OBJECT(window), "accel_group", accel_group);
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Title of window to display reports from spies with other players */
  gtk_window_set_title(GTK_WINDOW(window), _("Spy reports"));
  my_set_dialog_position(GTK_WINDOW(window));

  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(window),
                               GTK_WINDOW(ClientData.window));
  gtk_container_set_border_width(GTK_CONTAINER(window), 7);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(DestroySpyReports), NULL);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  notebook = gtk_notebook_new();
  g_object_set_data(G_OBJECT(window), "notebook", notebook);

  gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic(_("_Close"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(window));
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show_all(window);
}

void DisplaySpyReports(Player *Play)
{
  GtkWidget *dialog, *notebook, *vbox, *hbox, *frame, *label, *grid;
  GtkAccelGroup *accel_group;
  struct StatusWidgets Status;
  struct InventoryWidgets SpyDrugs, SpyGuns;

  if (!SpyReportsDialog)
    CreateSpyReports();
  dialog = SpyReportsDialog;
  notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "notebook"));
  accel_group =
      (GtkAccelGroup *)(g_object_get_data(G_OBJECT(dialog), "accel_group"));
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  frame = gtk_frame_new("Stats");
  gtk_container_set_border_width(GTK_CONTAINER(frame), 3);
  grid = CreateStatusWidgets(&Status);
  gtk_container_add(GTK_CONTAINER(frame), grid);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  CreateInventory(hbox, Names.Drugs, accel_group, FALSE, FALSE, &SpyDrugs,
                  NULL);
  CreateInventory(hbox, Names.Guns, accel_group, FALSE, FALSE, &SpyGuns,
                  NULL);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  label = gtk_label_new(GetPlayerName(Play));

  DisplayStats(Play, &Status);
  UpdateInventory(&SpyDrugs, Play->Drugs, NumDrug, TRUE);
  UpdateInventory(&SpyGuns, Play->Guns, NumGun, FALSE);

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

  gtk_widget_show_all(notebook);
}
