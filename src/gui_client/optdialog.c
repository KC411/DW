/************************************************************************
 * optdialog.c    Configuration file editing dialog                     *
 * Copyright (C)  2002-2004  Ben Webb                                   *
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

#include <string.h>             /* For strcmp */
#include <stdlib.h>             /* For atoi */

#include "configfile.h"         /* For UpdateConfigFile etc. */
#include "dopewars.h"           /* For struct GLOBALS etc. */
#include "gtk_client.h"         /* For mainwindow etc. */
#include "nls.h"                /* For _ function */
#include "sound.h"              /* For SoundPlay */
#include "gtkport/gtkport.h"    /* For gtk_ functions */

struct ConfigWidget {
  GtkWidget *widget;
  gint globind;
  int maxindex;
  gchar **data;
};

static GSList *configlist = NULL, *clists = NULL;

struct ConfigMembers {
  gchar *label, *name;
};

static void FreeConfigWidgets(void)
{
  while (configlist) {
    int i;
    struct ConfigWidget *cwid = (struct ConfigWidget *)configlist->data;

    for (i = 0; i < cwid->maxindex; i++) {
      g_free(cwid->data[i]);
    }
    g_free(cwid->data);
    configlist = g_slist_remove(configlist, cwid);
  }
}

static void UpdateAllLists(void)
{
  GSList *listpt;

  for (listpt = clists; listpt; listpt = g_slist_next(listpt)) {
    GtkTreeView *tv = GTK_TREE_VIEW(listpt->data);
    GtkTreeSelection *treesel = gtk_tree_view_get_selection(tv);
    /* Force unselection, which should trigger *_sel_changed function to
       copy widget data into configuration */
    gtk_tree_selection_unselect_all(treesel);
  }
}

static void SaveConfigWidget(struct GLOBALS *gvar, struct ConfigWidget *cwid,
                             int structind)
{
  gboolean changed = FALSE;

  if (gvar->BoolVal) {
    gboolean *boolpt, newset;

    boolpt = GetGlobalBoolean(cwid->globind, structind);
    if (cwid->maxindex) {
      newset = (cwid->data[structind - 1] != NULL);
    } else {
      newset = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cwid->widget));
    }
    changed = (*boolpt != newset);
    *boolpt = newset;
  } else {
    gchar *text;

    if (cwid->maxindex) {
      text = cwid->data[structind - 1];
    } else {
      text = gtk_editable_get_chars(GTK_EDITABLE(cwid->widget), 0, -1);
    }
    if (gvar->IntVal) {
      int *intpt, newset;

      intpt = GetGlobalInt(cwid->globind, structind);
      newset = atoi(text);
      if (newset < gvar->MinVal) {
        newset = gvar->MinVal;
      }
      if (gvar->MaxVal > gvar->MinVal && newset > gvar->MaxVal) {
        newset = gvar->MaxVal;
      }
      changed = (*intpt != newset);
      *intpt = newset;
    } else if (gvar->PriceVal) {
      price_t *pricept, newset;

      pricept = GetGlobalPrice(cwid->globind, structind);
      newset = strtoprice(text);
      changed = (*pricept != newset);
      *pricept = newset;
    } else if (gvar->StringVal) {
      gchar **strpt;

      strpt = GetGlobalString(cwid->globind, structind);
      changed = (strcmp(*strpt, text) != 0);
      AssignName(strpt, text);
    }
    if (!cwid->maxindex) {
      g_free(text);
    }
  }
  gvar->Modified |= changed;
}

static void ResizeList(struct GLOBALS *gvar, int newmax)
{
  int i;

  for (i = 0; i < NUMGLOB; i++) {
    if (Globals[i].StructListPt == gvar->StructListPt
        && Globals[i].ResizeFunc) {
      Globals[i].Modified = TRUE;
      (Globals[i].ResizeFunc) (newmax);
      return;
    }
  }
  g_assert(0);
}

static void SaveConfigWidgets(void)
{
  GSList *listpt;

  UpdateAllLists();

  for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
    struct ConfigWidget *cwid = (struct ConfigWidget *)listpt->data;
    struct GLOBALS *gvar;

    gvar = &Globals[cwid->globind];
    if (gvar->NameStruct && gvar->NameStruct[0]) {
      int i;

      if (cwid->maxindex != *gvar->MaxIndex) {
        ResizeList(gvar, cwid->maxindex);
      }

      for (i = 1; i <= *gvar->MaxIndex; i++) {
        SaveConfigWidget(gvar, cwid, i);
      }
    } else {
      SaveConfigWidget(gvar, cwid, 0);
    }
  }
}

static void AddConfigWidget(GtkWidget *widget, int ind)
{
  struct ConfigWidget *cwid = g_new(struct ConfigWidget, 1);
  struct GLOBALS *gvar;
  
  cwid->widget = widget;
  cwid->globind = ind;
  cwid->data = NULL;
  cwid->maxindex = 0;

  gvar = &Globals[ind];
  if (gvar->MaxIndex) {
    int i;

    cwid->maxindex = *gvar->MaxIndex;
    cwid->data = g_new(gchar *, *gvar->MaxIndex);
    for (i = 1; i <= *gvar->MaxIndex; i++) {
      if (gvar->StringVal) {
        cwid->data[i - 1] = g_strdup(*GetGlobalString(ind, i));
      } else if (gvar->IntVal) {
        cwid->data[i - 1] =
            g_strdup_printf("%d", *GetGlobalInt(ind, i));
      } else if (gvar->PriceVal) {
        cwid->data[i - 1] = pricetostr(*GetGlobalPrice(ind, i));
      } else if (gvar->BoolVal) {
        if (*GetGlobalBoolean(ind, i)) {
          cwid->data[i - 1] = g_strdup("1");
        } else {
          cwid->data[i - 1] = NULL;
        }
      }
    }
  }
  configlist = g_slist_append(configlist, cwid);
}

static GtkWidget *NewConfigCheck(gchar *name, gchar *label)
{
  GtkWidget *check;
  int ind;
  struct GLOBALS *gvar;

  ind = GetGlobalIndex(name, NULL);
  g_assert(ind >= 0);
  gvar = &Globals[ind];
  g_assert(gvar->BoolVal);

  check = gtk_check_button_new_with_label(label);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), *gvar->BoolVal);

  AddConfigWidget(check, ind);

  return check;
}

static GtkWidget *NewConfigEntry(gchar *name)
{
  GtkWidget *entry;
  gchar *tmpstr;
  int ind;
  struct GLOBALS *gvar;

  ind = GetGlobalIndex(name, NULL);
  g_assert(ind >= 0);

  entry = gtk_entry_new();
  gvar = &Globals[ind];

  if (gvar->StringVal) {
    gtk_entry_set_text(GTK_ENTRY(entry), *gvar->StringVal);
  } else if (gvar->IntVal) {
    if (gvar->MaxVal > gvar->MinVal) {
      GtkAdjustment *spin_adj;

      gtk_widget_destroy(entry);
      spin_adj = (GtkAdjustment *)gtk_adjustment_new(*gvar->IntVal,
                                                     gvar->MinVal,
                                                     gvar->MaxVal,
                                                     1.0, 10.0, 0.0);
      entry = gtk_spin_button_new(spin_adj, 1.0, 0);
    } else {
      tmpstr = g_strdup_printf("%d", *gvar->IntVal);
      gtk_entry_set_text(GTK_ENTRY(entry), tmpstr);
      g_free(tmpstr);
    }
  } else if (gvar->PriceVal) {
    tmpstr = pricetostr(*gvar->PriceVal);
    gtk_entry_set_text(GTK_ENTRY(entry), tmpstr);
    g_free(tmpstr);
  }

  AddConfigWidget(entry, ind);

  return entry;
}

static void AddStructConfig(GtkWidget *grid, int row, gchar *structname,
                            struct ConfigMembers *member)
{
  int ind;
  struct GLOBALS *gvar;

  ind = GetGlobalIndex(structname, member->name);
  g_assert(ind >= 0);

  gvar = &Globals[ind];
  if (gvar->BoolVal) {
    GtkWidget *check;

    check = gtk_check_button_new_with_label(_(member->label));
    dp_gtk_grid_attach(GTK_GRID(grid), check, 0, row, 2, 1, TRUE);
    AddConfigWidget(check, ind);
  } else {
    GtkWidget *label, *entry;

    label = gtk_label_new(_(member->label));
    dp_gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1, FALSE);
    if (gvar->IntVal && gvar->MaxVal > gvar->MinVal) {
      GtkAdjustment *spin_adj = (GtkAdjustment *)
          gtk_adjustment_new(gvar->MinVal, gvar->MinVal, gvar->MaxVal,
                             1.0, 10.0, 0.0);
      entry = gtk_spin_button_new(spin_adj, 1.0, 0);
    } else {
      entry = gtk_entry_new();
    }
    dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1, TRUE);
    AddConfigWidget(entry, ind);
  }
}

static void swap_rows(GtkTreeView *tv, gint selrow, gint swaprow,
                      gchar *structname)
{
  GSList *listpt;
  GtkTreeIter seliter, swapiter;
  GtkTreeModel *model = gtk_tree_view_get_model(tv);
  GtkTreeSelection *treesel = gtk_tree_view_get_selection(tv);

  g_assert(gtk_tree_model_iter_nth_child(model, &seliter, NULL, selrow));
  g_assert(gtk_tree_model_iter_nth_child(model, &swapiter, NULL, swaprow));

  gtk_tree_selection_unselect_iter(treesel, &seliter);
  gtk_list_store_swap(GTK_LIST_STORE(model), &seliter, &swapiter);

  for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
    struct ConfigWidget *cwid = (struct ConfigWidget *)listpt->data;
    struct GLOBALS *gvar;

    gvar = &Globals[cwid->globind];

    if (gvar->NameStruct && strcmp(structname, gvar->NameStruct) == 0) {
      gchar *tmp = cwid->data[selrow];

      cwid->data[selrow] = cwid->data[swaprow];
      cwid->data[swaprow] = tmp;
    }
  }

  gtk_tree_selection_select_iter(treesel, &seliter);
}

/* Return the index of the currently selected row, or -1 if none is selected.
   This works only for lists (not trees) with GTK_SELECTION_SINGLE mode */
static int get_tree_selection_row_index(GtkTreeSelection *treesel,
                                        GtkTreeModel **model)
{
  int row = -1;
  GList *selrows = gtk_tree_selection_get_selected_rows(treesel, model);
  if (selrows) {
    GtkTreePath *path = selrows->data;
    gint depth;
    gint *inds = gtk_tree_path_get_indices_with_depth(path, &depth);
    g_assert(selrows->next == NULL);
    g_assert(depth == 1);
    row = inds[0];
  }
  g_list_free_full(selrows, (GDestroyNotify)gtk_tree_path_free);
  return row;
}

static void list_delete(GtkWidget *widget, gchar *structname)
{
  GtkTreeView *tv;
  GtkTreeSelection *treesel;
  GtkTreeModel *model;
  int minlistlength, nrows, row;

  tv = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(widget), "treeview"));
  g_assert(tv);
  treesel = gtk_tree_view_get_selection(tv);
  row = get_tree_selection_row_index(treesel, &model);

  minlistlength = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model),
                                                    "minlistlength"));
  nrows = gtk_tree_model_iter_n_children(model, NULL);

  if (nrows > minlistlength && row >= 0) {
    GtkTreeIter iter;
    GSList *listpt;
    gboolean valid;
    /* Prevent selection changed from reading deleted entry */
    g_object_set_data(G_OBJECT(model), "oldsel", GINT_TO_POINTER(-1));
    g_assert(gtk_tree_model_iter_nth_child(model, &iter, NULL, row));
    gtk_tree_selection_unselect_iter(treesel, &iter);
    valid = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
      struct ConfigWidget *cwid = (struct ConfigWidget *)listpt->data;
      struct GLOBALS *gvar;

      gvar = &Globals[cwid->globind];
      if (gvar->NameStruct && strcmp(structname, gvar->NameStruct) == 0) {
        int i;

        cwid->maxindex--;
        g_free(cwid->data[row]);
        for (i = row; i < cwid->maxindex; i++) {
          cwid->data[i] = cwid->data[i + 1];
        }
        cwid->data = g_realloc(cwid->data, sizeof(gchar *) * cwid->maxindex);
      }
    }
    if (valid) {
      gtk_tree_selection_select_iter(treesel, &iter);
    }
  }
}

static void list_new(GtkWidget *widget, gchar *structname)
{
  GtkTreeView *tv;
  GtkListStore *store;
  GtkTreeSelection *treesel;
  GtkTreeIter iter;
  int row;
  GSList *listpt;
  gchar *newname;

  tv = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(widget), "treeview"));
  g_assert(tv);
  treesel = gtk_tree_view_get_selection(tv);
  store = GTK_LIST_STORE(gtk_tree_view_get_model(tv));

  newname = g_strdup_printf(_("New %s"), structname);
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter, 0, newname, -1);
  row = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL) - 1;

  for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
    struct ConfigWidget *cwid = (struct ConfigWidget *)listpt->data;
    struct GLOBALS *gvar;

    gvar = &Globals[cwid->globind];
    if (gvar->NameStruct && strcmp(structname, gvar->NameStruct) == 0) {
      cwid->maxindex++;
      g_assert(cwid->maxindex == row + 1);
      cwid->data = g_realloc(cwid->data, sizeof(gchar *) * cwid->maxindex);
      if (strcmp(gvar->Name, "Name") == 0) {
        cwid->data[row] = g_strdup(newname);
      } else {
        cwid->data[row] = g_strdup("");
      }
    }
  }
  g_free(newname);

  gtk_tree_selection_select_iter(treesel, &iter);
}

static void list_up(GtkWidget *widget, gchar *structname)
{
  GtkTreeView *tv;
  GtkTreeSelection *treesel;
  int row;

  tv = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(widget), "treeview"));
  g_assert(tv);
  treesel = gtk_tree_view_get_selection(tv);
  row = get_tree_selection_row_index(treesel, NULL);

  if (row > 0) {
    swap_rows(tv, row, row - 1, structname);
  }
}

static void list_down(GtkWidget *widget, gchar *structname)
{
  GtkTreeView *tv;
  GtkTreeSelection *treesel;
  GtkTreeModel *model;
  int row, nrows;

  tv = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(widget), "treeview"));
  g_assert(tv);
  treesel = gtk_tree_view_get_selection(tv);
  row = get_tree_selection_row_index(treesel, &model);
  nrows = gtk_tree_model_iter_n_children(model, NULL);

  if (row < nrows - 1 && row >= 0) {
    swap_rows(tv, row, row + 1, structname);
  }
}

static void list_sel_changed(GtkTreeSelection *treesel, gpointer data)
{
  GtkTreeModel *model;
  GtkWidget *delbut, *upbut, *downbut;
  int minlistlength, nrows, row, oldsel;
  GSList *listpt;
  gchar *structname = data;

  row = get_tree_selection_row_index(treesel, &model);
  nrows = gtk_tree_model_iter_n_children(model, NULL);

  delbut  = GTK_WIDGET(g_object_get_data(G_OBJECT(model), "delete"));
  upbut   = GTK_WIDGET(g_object_get_data(G_OBJECT(model), "up"));
  downbut = GTK_WIDGET(g_object_get_data(G_OBJECT(model), "down"));
  oldsel = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model), "oldsel"));
  g_assert(delbut && upbut && downbut);
  minlistlength = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model),
                                                    "minlistlength"));

  gtk_widget_set_sensitive(delbut, nrows > minlistlength);
  gtk_widget_set_sensitive(upbut, row > 0);
  gtk_widget_set_sensitive(downbut, row < nrows - 1);

  /* Store any edited data from old-selected row */
  if (oldsel >= 0) {
    for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
      struct ConfigWidget *conf = (struct ConfigWidget *)listpt->data;
      struct GLOBALS *gvar;

      gvar = &Globals[conf->globind];

      if (gvar->NameStruct && strcmp(structname, gvar->NameStruct) == 0) {
        g_free(conf->data[oldsel]);
        conf->data[oldsel] = NULL;

        if (gvar->BoolVal) {
          if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(conf->widget))) {
            conf->data[oldsel] = g_strdup("1");
          }
        } else {
          conf->data[oldsel] = gtk_editable_get_chars(
                                         GTK_EDITABLE(conf->widget), 0, -1);
          gtk_entry_set_text(GTK_ENTRY(conf->widget), "");
        }
        if (strcmp(gvar->Name, "Name") == 0) {
          GtkTreeIter ositer;
	  g_assert(gtk_tree_model_iter_nth_child(model, &ositer, NULL, oldsel));
	  gtk_list_store_set(GTK_LIST_STORE(model), &ositer, 0,
                             conf->data[oldsel], -1);
        }
      }
    }
  }
  g_object_set_data(G_OBJECT(model), "oldsel", GINT_TO_POINTER(row));

  /* Update widgets with selected row */
  if (row >= 0) {
    for (listpt = configlist; listpt; listpt = g_slist_next(listpt)) {
      struct ConfigWidget *conf = (struct ConfigWidget *)listpt->data;
      struct GLOBALS *gvar;

      gvar = &Globals[conf->globind];

      if (gvar->NameStruct && strcmp(structname, gvar->NameStruct) == 0) {
        if (gvar->BoolVal) {
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(conf->widget),
                                       conf->data[row] != NULL);
        } else {
          gtk_entry_set_text(GTK_ENTRY(conf->widget), conf->data[row]);
	}
      }
    }
  }
}

static void sound_sel_changed(GtkTreeSelection *treesel, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *entry;
  int row, oldsel, globind;

  row = get_tree_selection_row_index(treesel, &model);
  oldsel = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(model), "oldsel"));
  entry = GTK_WIDGET(g_object_get_data(G_OBJECT(model), "entry"));

  /* Store any edited data from old-selected row */
  if (oldsel >= 0) {
    gchar *text, **oldtext;
    g_assert(gtk_tree_model_iter_nth_child(model, &iter, NULL, oldsel));
    gtk_tree_model_get(model, &iter, 2, &globind, -1);
    g_assert(globind >=0 && globind < NUMGLOB);

    text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
    oldtext = GetGlobalString(globind, 0);
    g_assert(text && oldtext);
    if (strcmp(text, *oldtext) != 0) {
      AssignName(GetGlobalString(globind, 0), text);
      Globals[globind].Modified = TRUE;
    }
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    g_free(text);
  }

  g_object_set_data(G_OBJECT(model), "oldsel", GINT_TO_POINTER(row));
  /* Update new selection */
  if (row >= 0) {
    gchar **text;
    g_assert(gtk_tree_model_iter_nth_child(model, &iter, NULL, row));
    gtk_tree_model_get(model, &iter, 2, &globind, -1);
    g_assert(globind >=0 && globind < NUMGLOB);
    text = GetGlobalString(globind, 0);
    gtk_entry_set_text(GTK_ENTRY(entry), *text);
  }
}

static void BrowseSound(GtkWidget *entry)
{
  gchar *oldtext, *newtext;
  GtkWidget *dialog = gtk_widget_get_ancestor(entry, GTK_TYPE_WINDOW);

  oldtext = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

  newtext = GtkGetFile(dialog, oldtext, _("Select sound file"));
  g_free(oldtext);
  if (newtext) {
    gtk_entry_set_text(GTK_ENTRY(entry), newtext);
    g_free(newtext);
  }
}

static void TestPlaySound(GtkWidget *entry)
{
  gchar *text;
  gboolean sound_enabled;

  text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  sound_enabled = IsSoundEnabled();
  SoundEnable(TRUE);
  SoundPlay(text);
  SoundEnable(sound_enabled);
  g_free(text);
}

static void OKCallback(GtkWidget *widget, GtkWidget *dialog)
{
  GtkToggleButton *unicode_check;

  SaveConfigWidgets();
  unicode_check = GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog),
                                                      "unicode_check"));
  UpdateConfigFile(NULL, gtk_toggle_button_get_active(unicode_check));
  gtk_widget_destroy(dialog);
}

static gchar *GetHelpPage(const gchar *pagename)
{
  gchar *root, *file;

  root = GetDocRoot();
  file = g_strdup_printf("%shelp/%s.html", root, pagename);
  g_free(root);
  return file;
}

static void HelpCallback(GtkWidget *widget, GtkWidget *notebook)
{
  const static gchar *pagehelp[] = {
    "general", "locations", "drugs", "guns", "cops", "server", "sounds"
  };
  gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
  gchar *help;

  help = GetHelpPage(pagehelp[page]);
  DisplayHTML(widget, OurWebBrowser, help);
  g_free(help);
}

static void FinishOptDialog(GtkWidget *widget, gpointer data)
{
  FreeConfigWidgets();

  g_slist_free(clists);
  clists = NULL;
}

static GtkWidget *CreateList(gchar *structname, struct ConfigMembers *members)
{
  GtkWidget *hbox, *vbox, *hbbox, *tv, *scrollwin, *button, *grid;
  GtkTreeSelection *treesel;
  GtkListStore *store;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;

  int ind, minlistlength = 0;
  gint i, nummembers;
  struct GLOBALS *gvar;
  struct ConfigMembers namemember = { N_("Name"), "Name" };

  ind = GetGlobalIndex(structname, "Name");
  g_assert(ind >= 0);
  gvar = &Globals[ind];
  g_assert(gvar->StringVal && gvar->MaxIndex);

  for (i = 0; i < NUMGLOB; i++) {
    if (Globals[i].StructListPt == gvar->StructListPt
        && Globals[i].ResizeFunc) {
      minlistlength = Globals[i].MinVal;
    }
  }

  nummembers = 0;
  while (members && members[nummembers].label) {
    nummembers++;
  }

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

  tv = gtk_scrolled_tree_view_new(&scrollwin);
  store = gtk_list_store_new(1, G_TYPE_STRING);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tv), GTK_TREE_MODEL(store));
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(
               GTK_TREE_VIEW(tv), -1, structname, renderer, "text", 0, NULL);
  g_object_unref(store);
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tv), FALSE);

  treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
  g_signal_connect(G_OBJECT(treesel), "changed", G_CALLBACK(list_sel_changed),
                   structname);
  gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
	  GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

  clists = g_slist_append(clists, tv);

  for (i = 1; i <= *gvar->MaxIndex; i++) {
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, *GetGlobalString(ind, i), -1);
  }
  gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 0);

  hbbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_set_homogeneous(GTK_BOX(hbbox), TRUE);
  g_object_set_data(G_OBJECT(store), "oldsel", GINT_TO_POINTER(-1));

  button = gtk_button_new_with_label(_("New"));
  g_object_set_data(G_OBJECT(button), "treeview", tv);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(list_new), structname);
  gtk_box_pack_start(GTK_BOX(hbbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(_("Delete"));
  gtk_widget_set_sensitive(button, FALSE);
  g_object_set_data(G_OBJECT(button), "treeview", tv);
  g_object_set_data(G_OBJECT(store), "delete", button);
  g_object_set_data(G_OBJECT(store), "minlistlength",
                    GINT_TO_POINTER(minlistlength));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(list_delete), structname);
  gtk_box_pack_start(GTK_BOX(hbbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(_("Up"));
  gtk_widget_set_sensitive(button, FALSE);
  g_object_set_data(G_OBJECT(button), "treeview", tv);
  g_object_set_data(G_OBJECT(store), "up", button);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(list_up), structname);
  gtk_box_pack_start(GTK_BOX(hbbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(_("Down"));
  gtk_widget_set_sensitive(button, FALSE);
  g_object_set_data(G_OBJECT(button), "treeview", tv);
  g_object_set_data(G_OBJECT(store), "down", button);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(list_down), structname);
  gtk_box_pack_start(GTK_BOX(hbbox), button, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

  grid = dp_gtk_grid_new(nummembers + 1, 2, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);

  AddStructConfig(grid, 0, structname, &namemember);
  for (i = 0; i < nummembers; i++) {
    AddStructConfig(grid, i + 1, structname, &members[i]);
  }

  gtk_box_pack_start(GTK_BOX(hbox), grid, TRUE, TRUE, 0);

  return hbox;
}

static void FillSoundsList(GtkTreeView *tv)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint i;

  /* Don't update the widget until we're done */
  store = GTK_LIST_STORE(gtk_tree_view_get_model(tv));
  g_object_ref(store);
  gtk_tree_view_set_model(tv, NULL);

  gtk_list_store_clear(store);
  for (i = 0; i < NUMGLOB; i++) {
    if (strlen(Globals[i].Name) > 7
	&& strncmp(Globals[i].Name, "Sounds.", 7) == 0) {
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, 0, &Globals[i].Name[7],
                         1, _(Globals[i].Help), 2, i, -1);
    }
  }

  gtk_tree_view_set_model(tv, GTK_TREE_MODEL(store));
}

void OptDialog(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog, *notebook, *label, *check, *entry, *grid;
  GtkWidget *hbox, *vbox, *vbox2, *hsep, *button, *hbbox, *tv;
  GtkWidget *scrollwin;
  GtkAccelGroup *accel_group;
  gchar *sound_titles[2];
  GtkCellRenderer *renderer;
  GtkListStore *store;
  GtkTreeSelection *treesel;

  struct ConfigMembers locmembers[] = {
    { N_("Police presence"), "PolicePresence" },
    { N_("Minimum no. of drugs"), "MinDrug" },
    { N_("Maximum no. of drugs"), "MaxDrug" },
    { NULL, NULL }
  };
  struct ConfigMembers drugmembers[] = {
    { N_("Minimum normal price"), "MinPrice" },
    { N_("Maximum normal price"), "MaxPrice" },
    { N_("Can be specially cheap"), "Cheap" },
    { N_("Cheap string"), "CheapStr" },
    { N_("Can be specially expensive"), "Expensive" },
    { NULL, NULL }
  };
  struct ConfigMembers gunmembers[] = {
    { N_("Price"), "Price" },
    { N_("Inventory space"), "Space" },
    { N_("Damage"), "Damage" },
    { NULL, NULL }
  };
  struct ConfigMembers copmembers[] = {
    { N_("Name of one deputy"), "DeputyName" },
    { N_("Name of several deputies"), "DeputiesName" },
    { N_("Minimum no. of deputies"), "MinDeputies" },
    { N_("Maximum no. of deputies"), "MaxDeputies" },
    { N_("Cop armor"), "Armor" },
    { N_("Deputy armor"), "DeputyArmor" },
    { NULL, NULL }
  };

  dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog), accel_group);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Options"));
  my_set_dialog_position(GTK_WINDOW(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 7);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

  gtk_window_set_transient_for(GTK_WINDOW(dialog),
                               GTK_WINDOW(MainWindow));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);

  notebook = gtk_notebook_new();

  grid = dp_gtk_grid_new(8, 3, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);

  check = NewConfigCheck("Sanitized", _("Remove drug references"));
  dp_gtk_grid_attach(GTK_GRID(grid), check, 0, 0, 1, 1, FALSE);

  check = gtk_check_button_new_with_label(_("Unicode config file"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), IsConfigFileUTF8());
  dp_gtk_grid_attach(GTK_GRID(grid), check, 1, 0, 2, 1, TRUE);
  g_object_set_data(G_OBJECT(dialog), "unicode_check", check);

  label = gtk_label_new(_("Game length (turns)"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1, FALSE);
  entry = NewConfigEntry("NumTurns");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 1, 2, 1, TRUE);

  label = gtk_label_new(_("Starting cash"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1, FALSE);
  entry = NewConfigEntry("StartCash");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 2, 2, 1, TRUE);

  label = gtk_label_new(_("Starting debt"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1, FALSE);
  entry = NewConfigEntry("StartDebt");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 3, 2, 1, TRUE);

  label = gtk_label_new(_("Currency symbol"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1, FALSE);
  entry = NewConfigEntry("Currency.Symbol");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 4, 1, 1, TRUE);
  check = NewConfigCheck("Currency.Prefix", _("Symbol prefixes prices"));
  dp_gtk_grid_attach(GTK_GRID(grid), check, 2, 4, 1, 1, TRUE);

  label = gtk_label_new(_("Name of one bitch"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1, FALSE);
  entry = NewConfigEntry("Names.Bitch");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 5, 2, 1, TRUE);

  label = gtk_label_new(_("Name of several bitches"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 6, 1, 1, FALSE);
  entry = NewConfigEntry("Names.Bitches");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 6, 2, 1, TRUE);

#ifndef CYGWIN
  label = gtk_label_new(_("Web browser"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 7, 1, 1, FALSE);
  entry = NewConfigEntry("WebBrowser");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 7, 2, 1, TRUE);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(grid), 7);
  label = gtk_label_new(_("General"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);

  hbox = CreateList("Location", locmembers);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 7);

  label = gtk_label_new(_("Locations"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);

  vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(vbox2), 7);

  hbox = CreateList("Drug", drugmembers);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox2), hsep, FALSE, FALSE, 0);

  grid = dp_gtk_grid_new(2, 2, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
  label = gtk_label_new(_("Expensive string 1"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1, FALSE);
  entry = NewConfigEntry("Drugs.ExpensiveStr1");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 1, 1, TRUE);

  label = gtk_label_new(_("Expensive string 2"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1, FALSE);
  entry = NewConfigEntry("Drugs.ExpensiveStr2");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 1, 1, 1, TRUE);
  gtk_box_pack_start(GTK_BOX(vbox2), grid, FALSE, FALSE, 0);

  label = gtk_label_new(_("Drugs"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, label);

  hbox = CreateList("Gun", gunmembers);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 7);
  label = gtk_label_new(_("Guns"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);

  hbox = CreateList("Cop", copmembers);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 7);
  label = gtk_label_new(_("Cops"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);

#ifdef NETWORKING
  grid = dp_gtk_grid_new(6, 4, FALSE);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);

  check = NewConfigCheck("MetaServer.Active",
                         _("Server reports to metaserver"));
  dp_gtk_grid_attach(GTK_GRID(grid), check, 0, 0, 2, 1, TRUE);

#ifdef CYGWIN
  check = NewConfigCheck("MinToSysTray", _("Minimize to System Tray"));
  dp_gtk_grid_attach(GTK_GRID(grid), check, 2, 0, 2, 1, TRUE);
#endif

  label = gtk_label_new(_("Metaserver URL"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1, FALSE);
  entry = NewConfigEntry("MetaServer.URL");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 1, 3, 1, TRUE);

  label = gtk_label_new(_("Comment"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1, FALSE);
  entry = NewConfigEntry("MetaServer.Comment");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 4, 3, 1, TRUE);

  label = gtk_label_new(_("MOTD (welcome message)"));
  dp_gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1, FALSE);
  entry = NewConfigEntry("ServerMOTD");
  dp_gtk_grid_attach(GTK_GRID(grid), entry, 1, 5, 3, 1, TRUE);

  gtk_container_set_border_width(GTK_CONTAINER(grid), 7);
  label = gtk_label_new(_("Server"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);
#endif

  vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox2), 7);

  sound_titles[0] = _("Sound name");
  sound_titles[1] = _("Description");
  tv = gtk_scrolled_tree_view_new(&scrollwin);
  store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
  g_object_set_data(G_OBJECT(store), "oldsel", GINT_TO_POINTER(-1));
  gtk_tree_view_set_model(GTK_TREE_VIEW(tv), GTK_TREE_MODEL(store));
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(
               GTK_TREE_VIEW(tv), -1, sound_titles[0], renderer,
	       "text", 0, NULL);
  gtk_tree_view_insert_column_with_attributes(
               GTK_TREE_VIEW(tv), -1, sound_titles[1], renderer,
	       "text", 1, NULL);
  g_object_unref(store);
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tv), FALSE);
  treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
  gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);

  FillSoundsList(GTK_TREE_VIEW(tv));
  g_signal_connect(G_OBJECT(treesel), "changed",
                   G_CALLBACK(sound_sel_changed), NULL);

  clists = g_slist_append(clists, tv);

  gtk_box_pack_start(GTK_BOX(vbox2), scrollwin, TRUE, TRUE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  label = gtk_label_new(_("Sound file"));
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new();
  g_object_set_data(G_OBJECT(store), "entry", entry);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(_("Browse..."));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(BrowseSound), G_OBJECT(entry));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(_("Play"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(TestPlaySound), G_OBJECT(entry));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(_("Sounds"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, label);

  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

  gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

  hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

  hbbox = my_hbbox_new();

  button = gtk_button_new_with_mnemonic(_("_OK"));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(OKCallback), (gpointer)dialog);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  button = gtk_button_new_with_mnemonic(_("_Help"));
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(HelpCallback), (gpointer)notebook);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  button = gtk_button_new_with_mnemonic(_("_Cancel"));
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           G_OBJECT(dialog));
  g_signal_connect(G_OBJECT(dialog), "destroy",
                   G_CALLBACK(FinishOptDialog), NULL);
  my_gtk_box_pack_start_defaults(GTK_BOX(hbbox), button);

  gtk_box_pack_start(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(dialog), vbox);

  gtk_widget_show_all(dialog);
}
