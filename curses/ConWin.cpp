//--------------------------------------------------------------------
//
//   Visual Binary Diff
//   Copyright 1997-2005 by Christopher J. Madsen
//
//   Support class for curses applications
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of
//   the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <https://www.gnu.org/licenses/>.
//--------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include "ConWin.hpp"

void exitMsg(int status, const char* message); // From vbindiff.cpp

//--------------------------------------------------------------------
// Default ("modernized") color scheme:
//
// Each Style gets its own foreground, background, and extra attributes
// (currently just A_BOLD).  This can be overridden at runtime with
// ConWindow::loadColorConfig() -- see that function for the file format.

struct StyleColor { short fg, bg; attr_t attrs; };

static StyleColor styleColor[cNumStyles] = {
  { COLOR_WHITE,  COLOR_BLUE,  0         },  // cBackground
  { COLOR_WHITE,  COLOR_BLUE,  0         },  // cPromptWin
  { COLOR_WHITE,  COLOR_BLUE,  A_BOLD    },  // cPromptKey
  { COLOR_WHITE,  COLOR_BLUE,  A_BOLD    },  // cPromptBdr
  { COLOR_WHITE,  COLOR_BLACK, A_REVERSE },  // cCurrentMode
  { COLOR_WHITE,  COLOR_BLACK, A_REVERSE },  // cFileName
  { COLOR_WHITE,  COLOR_BLUE,  0         },  // cFileWin
  { COLOR_RED,    COLOR_BLUE,  A_BOLD    },  // cFileDiff
  { COLOR_YELLOW, COLOR_BLUE,  A_BOLD    },  // cFileEdit
  { COLOR_WHITE,  COLOR_BLUE,  A_BOLD    },  // cDivider (new in this fork; matches original border style)
};

static const char* const styleName[cNumStyles] = {
  "background",
  "promptWin",
  "promptKey",
  "promptBdr",
  "currentMode",
  "fileName",
  "fileWin",
  "fileDiff",
  "fileEdit",
  "divider",
};

static attr_t attribStyle[cNumStyles];
static short  pairOf[cNumStyles];     // curses color-pair number per style, 0 = none

//--------------------------------------------------------------------
// Parse a color name (case-insensitive) into a curses COLOR_* value.
// Returns true and sets `color` on success.

static bool parseColorName(const string& name, short& color)
{
  static const struct { const char* name; short color; } table[] = {
    { "black",   COLOR_BLACK   },
    { "red",     COLOR_RED     },
    { "green",   COLOR_GREEN   },
    { "yellow",  COLOR_YELLOW  },
    { "blue",    COLOR_BLUE    },
    { "magenta", COLOR_MAGENTA },
    { "cyan",    COLOR_CYAN    },
    { "white",   COLOR_WHITE   },
    { NULL, 0 }
  };

  string lower(name);
  for (string::size_type i = 0; i < lower.size(); ++i)
    lower[i] = tolower(static_cast<unsigned char>(lower[i]));

  for (int i = 0; table[i].name; ++i)
    if (lower == table[i].name) {
      color = table[i].color;
      return true;
    }

  return false;
} // end parseColorName

//--------------------------------------------------------------------
// Load a color scheme from a config file.
//
// File format (one style per line, blank lines and lines starting with
// '#' are ignored):
//
//   styleName = foreground/background[/bold]
//
// Valid styleNames: background, promptWin, promptKey, promptBdr,
//   currentMode, fileName, fileWin, fileDiff, fileEdit, divider
// Valid colors: black, red, green, yellow, blue, magenta, cyan, white
//
// Example:
//   fileDiff = red/black/bold
//   fileEdit = yellow/black/bold
//
// Must be called before ConWindow::startup().

string ConWindow::loadColorConfig(const char* path, bool optional)
{
  ifstream in(path);
  if (!in) {
    if (optional) return "";
    return string("Unable to open color config file: ") + path;
  }

  map<string,int> nameToIndex;
  for (int i = 0; i < cNumStyles; ++i)
    nameToIndex[styleName[i]] = i;

  string line;
  int lineNum = 0;
  while (getline(in, line)) {
    ++lineNum;

    // Strip comments & whitespace:
    string::size_type hash = line.find('#');
    if (hash != string::npos) line.erase(hash);

    string::size_type start = line.find_first_not_of(" \t\r\n");
    if (start == string::npos) continue;    // blank line
    string::size_type end = line.find_last_not_of(" \t\r\n");
    line = line.substr(start, end - start + 1);
    if (line.empty()) continue;

    string::size_type eq = line.find('=');
    if (eq == string::npos) {
      ostringstream err;
      err << path << ':' << lineNum << ": expected 'name = fg/bg'";
      return err.str();
    }

    string name = line.substr(0, eq);
    string value = line.substr(eq+1);

    string::size_type nEnd = name.find_last_not_of(" \t");
    name = name.substr(0, nEnd+1);
    string::size_type vStart = value.find_first_not_of(" \t");
    if (vStart != string::npos) value = value.substr(vStart);

    map<string,int>::iterator it = nameToIndex.find(name);
    if (it == nameToIndex.end()) {
      ostringstream err;
      err << path << ':' << lineNum << ": unknown style name '" << name
          << "'";
      return err.str();
    }

    // Split value on '/':
    vector<string> parts;
    string::size_type pos = 0;
    while (true) {
      string::size_type slash = value.find('/', pos);
      parts.push_back(value.substr(pos, slash - pos));
      if (slash == string::npos) break;
      pos = slash + 1;
    }

    if (parts.size() < 2) {
      ostringstream err;
      err << path << ':' << lineNum
          << ": expected 'foreground/background[/bold]'";
      return err.str();
    }

    short fg, bg;
    if (!parseColorName(parts[0], fg) || !parseColorName(parts[1], bg)) {
      ostringstream err;
      err << path << ':' << lineNum << ": unrecognized color name";
      return err.str();
    }

    attr_t attrs = 0;
    for (size_t i = 2; i < parts.size(); ++i) {
      string opt(parts[i]);
      for (string::size_type j = 0; j < opt.size(); ++j)
        opt[j] = tolower(static_cast<unsigned char>(opt[j]));
      if (opt == "bold")         attrs |= A_BOLD;
      else if (opt == "reverse") attrs |= A_REVERSE;
      else if (opt == "underline") attrs |= A_UNDERLINE;
      else {
        ostringstream err;
        err << path << ':' << lineNum << ": unrecognized attribute '"
            << parts[i] << "'";
        return err.str();
      }
    }

    int idx = it->second;
    styleColor[idx].fg    = fg;
    styleColor[idx].bg    = bg;
    styleColor[idx].attrs = attrs;
  } // end while getline

  return "";
} // end ConWindow::loadColorConfig

//====================================================================
// Class ConWindow:
//--------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////
// Static Member Functions:
//--------------------------------------------------------------------
// Start up the window system:
//
// Allocates a screen buffer and sets input mode:
//
// Returns:
//   true:   Everything set up properly
//   false:  Unable to initialize

bool ConWindow::startup()
{
  if (!initscr()) return false; // initialize the curses library
  atexit(ConWindow::shutdown);  // just in case

  keypad(stdscr, true);         // enable keyboard mapping
  nonl();           // tell curses not to do NL->CR/NL on output
  cbreak();         // take input chars one at a time, no wait for \n
  noecho();         // do not echo input

  if (has_colors()) {
    start_color();

    for (int i = 0; i < cNumStyles; ++i) {
      short pair = short(i + 1);
      init_pair(pair, styleColor[i].fg, styleColor[i].bg);
      pairOf[i]      = pair;
      attribStyle[i] = styleColor[i].attrs | COLOR_PAIR(pair);
    }
  } else {
    // No color support: fall back to plain attributes only:
    for (int i = 0; i < cNumStyles; ++i) {
      pairOf[i]      = 0;
      attribStyle[i] = styleColor[i].attrs;
    }
  } // end if terminal has color

  return true;
} // end ConWindow::startup

//--------------------------------------------------------------------
// Shut down the window system:
//
// Deallocate the screen buffer and restore the original input mode.

void ConWindow::shutdown()
{
  if (!isendwin()) {
    showCursor();
    endwin();
  }
} // end ConWindow::shutdown

//////////////////////////////////////////////////////////////////////
// Member Functions:
//--------------------------------------------------------------------
// Constructor:

ConWindow::ConWindow()
: pan(NULL),
  win(NULL)
{
} // end ConWindow::ConWindow

//--------------------------------------------------------------------
// Destructor:

ConWindow::~ConWindow()
{
  close();
} // end ConWindow::~ConWindow

//--------------------------------------------------------------------
// Initialize the window:
//
// Must be called only once, before any other functions are called.
// Allocates the data structures and clears the window buffer, but
// does not display anything.
//
// Input:
//   x,y:           The position of the window in the screen buffer
//   width,height:  The size of the window
//   attrib:        The default attributes for the window

void ConWindow::init(short x, short y, short width, short height, Style attrib)
{
  if ((win = newwin(height, width, y, x)) == 0)
    exitMsg(99, "Internal error: Failed to create window");

  if ((pan = new_panel(win)) == 0)
    exitMsg(99, "Internal error: Failed to create panel");

  wbkgdset(win, attribStyle[attrib] | ' ');

  keypad(win, TRUE);            // enable keyboard mapping

  clear();
} // end ConWindow::init

//--------------------------------------------------------------------
void ConWindow::close()
{
  if (pan) {
    del_panel(pan);
    pan = NULL;
  }

  if (win) {
    delwin(win);
    win = NULL;
  }
} // end ConWindow::close

//--------------------------------------------------------------------
// Write a string using the current attributes:
//
// Input:
//   x,y:  The start of the string in the window
//   s:    The string to write

//void ConWindow::put(short x, short y, const char* s)

///void ConWindow::put(short x, short y, const String& s)
///{
///  PCHAR_INFO  out = data + x + size.X * y;
///  StrConstItr  c = s.begin();
///
///  while (c != s.end()) {
///    out->Char.AsciiChar = *c;
///    out->Attributes = attribs;
///    ++out;
///    ++c;
///  }
///} // end ConWindow::put

//--------------------------------------------------------------------
// Change the attributes of characters in the window:
//
// Input:
//   x,y:    The position in the window to start changing attributes
//   color:  The attribute to set
//   count:  The number of characters to change

void ConWindow::putAttribs(short x, short y, Style color, short count)
{
  mvwchgat(win, y, x, count, attribStyle[color], pairOf[color], NULL);
  touchwin(win);
} // end ConWindow::putAttribs

//--------------------------------------------------------------------
// Write a character using the current attributes:
//
// Input:
//   x,y:    The position in the window to start writing
//   c:      The character to write
//   count:  The number of characters to write

void ConWindow::putChar(short x, short y, char c, short count)
{
  wmove(win, y, x);

  while (count--) {
    waddch(win, c);
  }
} // end ConWindow::putAttribs

//--------------------------------------------------------------------
// Read the next key down event:
//
// Output:
//   event:  Contains a key down event

int ConWindow::readKey()
{
  top_panel(pan);
  update_panels();
  doupdate();

  return wgetch(win);
} // end ConWindow::readKey

//--------------------------------------------------------------------
void ConWindow::resize(short width, short height)
{
  if (wresize(win, height, width) != OK)
    exitMsg(99, "Internal error: Failed to resize window");

  replace_panel(pan, win);

  clear();
} // end ConWindow::resize

//--------------------------------------------------------------------
void ConWindow::setAttribs(Style color)
{
  wattrset(win, attribStyle[color]);
} // end ConWindow::setAttribs

//--------------------------------------------------------------------
// Position the cursor in the window:
//
// There is only one cursor, and each window does not maintain its own
// cursor position.
//
// Input:
//   x,y:    The position in the window for the cursor

void ConWindow::setCursor(short x, short y)
{
//  ASSERT((x>=0)&&(x<size.X)&&(y>=0)&&(y<size.Y));

  wmove(win, y, x);
} // end ConWindow::setCursor

//--------------------------------------------------------------------
// Local Variables:
//     c-file-style: "cjm"
// End:
