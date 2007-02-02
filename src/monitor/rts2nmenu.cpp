#include "rts2nmenu.h"

void
Rts2NAction::draw (WINDOW * master_window, int y)
{
  mvwprintw (master_window, y, 0, "%s", text);
}

Rts2NSubmenu::Rts2NSubmenu (WINDOW * master_window, const char *in_text):
Rts2NSelWindow (master_window, 0, 0, 2, 2)
{
  text = in_text;
  grow (strlen (text) + 2, 0);
}

Rts2NSubmenu::~Rts2NSubmenu (void)
{
}

void
Rts2NSubmenu::draw (WINDOW * master_window)
{
  mvwprintw (master_window, 0, getX () + 1, "%s", text);
}

void
Rts2NSubmenu::drawSubmenu ()
{
  Rts2NSelWindow::draw ();
  werase (scrolpad);
  maxrow = 0;
  for (std::vector < Rts2NAction * >::iterator iter = actions.begin ();
       iter != actions.end (); iter++, maxrow++)
    {
      Rts2NAction *action = *iter;
      action->draw (scrolpad, maxrow);
    }
}

Rts2NMenu::Rts2NMenu (WINDOW * master_window):Rts2NWindow (master_window, 0, 0, COLS, 1,
	     0)
{
  selSubmenuIter = submenus.begin ();
  selSubmenu = NULL;
  top_x = 0;
}

Rts2NMenu::~Rts2NMenu (void)
{
  for (selSubmenuIter = submenus.begin (); selSubmenuIter != submenus.end ();
       selSubmenuIter++)
    {
      selSubmenu = *selSubmenuIter;
      delete selSubmenu;
    }
  submenus.clear ();
}

int
Rts2NMenu::injectKey (int key)
{
  switch (key)
    {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
      if (selSubmenu)
	{
	  return selSubmenu->injectKey (key);
	}
      break;
    case KEY_RIGHT:
      selSubmenuIter++;
      if (selSubmenuIter == submenus.end ())
	selSubmenuIter = submenus.begin ();
      selSubmenu = *selSubmenuIter;
      break;
    case KEY_LEFT:
      if (selSubmenuIter == submenus.begin ())
	selSubmenuIter = submenus.end ();
      selSubmenuIter--;
      selSubmenu = *selSubmenuIter;
      break;
    case '\n':
    case KEY_ENTER:
      return 0;
    case KEY_EXIT:
      return -1;
    }
  return -1;
}

void
Rts2NMenu::draw ()
{
  Rts2NWindow::draw ();
  werase (window);
  for (std::vector < Rts2NSubmenu * >::iterator iter = submenus.begin ();
       iter != submenus.end (); iter++)
    {
      Rts2NSubmenu *submenu = *iter;
      if (submenu == selSubmenu)
	submenu->drawSubmenu ();
      submenu->draw (window);
      submenu->refresh ();
    }
  refresh ();
}

void
Rts2NMenu::addSubmenu (Rts2NSubmenu * in_submenu)
{
  submenus.push_back (in_submenu);
  selSubmenuIter = submenus.begin ();
  if (!selSubmenu)
    {
      selSubmenu = *selSubmenuIter;
    }
  in_submenu->move (top_x, 0);
  top_x += in_submenu->getWidth () + 2;
}
