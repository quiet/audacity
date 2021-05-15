/**********************************************************************

  Audacity: A Digital Audio Editor

  CommandManager.cpp

  Brian Gunlogson
  Dominic Mazzoni

*******************************************************************//**

\class CommandManager
\brief CommandManager implements a system for organizing all user-callable
  commands.

  It creates and manages a menu bar with a command
  associated with each item, and managing other commands callable
  by keyboard shortcuts.

  Commands are implemented by overriding an abstract functor class.
  See Menus.cpp for an example use.

  Menus or submenus containing lists of items can be added at once,
  with a single function (functor) to be called when any of the
  items is selected, with the index number of the selection as the
  parameter.  This is useful for dynamic menus (effects) and
  submenus containing a list of choices (selection formats).

  Menu items can be enabled or disabled individually, groups of
  "multi-items" can be enabled or disabled all at once, or entire
  sets of commands can be enabled or disabled all at once using
  flags.  The flags should be a bitfield stored in a 32-bit
  integer but can be whatever you want.  You specify both the
  desired values of the flags, and the set of flags relevant to
  a particular command, by using a combination of a flags parameter
  and a mask parameter.  Any flag set to 0 in the mask parameter is
  the same as "don't care".  Any command whose mask is set to zero
  will not be affected by enabling/disabling by flags.

*//****************************************************************//**

\class CommandFunctor
\brief CommandFunctor is a very small class that works with
CommandManager.  It holds the callback for one command.

*//****************************************************************//**

\class MenuBarListEntry
\brief MenuBarListEntry is a structure used by CommandManager.

*//****************************************************************//**

\class SubMenuListEntry
\brief SubMenuListEntry is a structure used by CommandManager.

*//****************************************************************//**

\class CommandListEntry
\brief CommandListEntry is a structure used by CommandManager.

*//****************************************************************//**

\class MenuBarList
\brief List of MenuBarListEntry.

*//****************************************************************//**

\class SubMenuList
\brief List of SubMenuListEntry.

*//****************************************************************//**

\class CommandList
\brief List of CommandListEntry.

*//******************************************************************/

#include "../Audacity.h"

#include "../Experimental.h"

#include "../AudacityHeaders.h"
#include "CommandManager.h"

#include "CommandManagerWindowClasses.h"
#include "CommandContext.h"

#include <wx/defs.h>
#include <wx/eventfilter.h>
#include <wx/evtloop.h>
#include <wx/hash.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/tokenzr.h>

#include "../AudacityException.h"
#include "../Menus.h"
#include "../Prefs.h"
#include "../Project.h"

#include "Keyboard.h"
#include "../PluginManager.h"
#include "../effects/EffectManager.h"
#include "../widgets/LinkingHtmlWindow.h"
#include "../widgets/ErrorDialog.h"
#include "../widgets/HelpSystem.h"


// On wxGTK, there may be many many many plugins, but the menus don't automatically
// allow for scrolling, so we build sub-menus.  If the menu gets longer than
// MAX_MENU_LEN, we put things in submenus that have MAX_SUBMENU_LEN items in them.
//
#ifdef __WXGTK__
#define MAX_MENU_LEN 20
#define MAX_SUBMENU_LEN 15
#else
#define MAX_MENU_LEN 1000
#define MAX_SUBMENU_LEN 1000
#endif

#define COMMAND _("Command")

#if defined(__WXMAC__)
#include <AppKit/AppKit.h>
#include <wx/osx/private.h>
#elif defined(__WXGTK__)
#include <gtk/gtk.h>
#endif

NonKeystrokeInterceptingWindow::~NonKeystrokeInterceptingWindow()
{
}

TopLevelKeystrokeHandlingWindow::~TopLevelKeystrokeHandlingWindow()
{
}

MenuBarListEntry::MenuBarListEntry(const wxString &name_, wxMenuBar *menubar_)
   : name(name_), menubar(menubar_)
{
}

MenuBarListEntry::~MenuBarListEntry()
{
}

SubMenuListEntry::SubMenuListEntry(
   const wxString &name_, std::unique_ptr<wxMenu> &&menu_ )
   : name(name_), menu( std::move(menu_) )
{
}

SubMenuListEntry::SubMenuListEntry(SubMenuListEntry &&that)
   : name(std::move(that.name))
   , menu(std::move(that.menu))
{
}

SubMenuListEntry::~SubMenuListEntry()
{
}

// Shared by all projects
static class CommandManagerEventMonitor final : public wxEventFilter
{
public:
   CommandManagerEventMonitor()
   :  wxEventFilter()
   {
#if defined(__WXMAC__)
      // In wx3, the menu accelerators take precendence over key event processing
      // so we won't get wxEVT_CHAR_HOOK events for combinations assigned to menus.
      // Since we only support OS X 10.6 or greater, we can use an event monitor
      // to capture the key event before it gets to the normal wx3 processing.

      // The documentation for addLocalMonitorForEventsMatchingMask implies that
      // NSKeyUpMask can't be used in 10.6, but testing shows that it can.
      NSEventMask mask = NSKeyDownMask | NSKeyUpMask;

      mHandler =
      [
         NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent *(NSEvent *event)
         {
            WXWidget widget = (WXWidget) [ [event window] firstResponder];
            if (widget)
            {
               wxWidgetCocoaImpl *impl = (wxWidgetCocoaImpl *)
                  wxWidgetImpl::FindFromWXWidget(widget);
               if (impl)
               {
                  mEvent = event;

                  wxKeyEvent wxevent([event type] == NSKeyDown ? wxEVT_KEY_DOWN : wxEVT_KEY_UP);
                  impl->SetupKeyEvent(wxevent, event);

                  NSEvent *result;
                  if ([event type] == NSKeyDown)
                  {
                     wxKeyEvent eventHook(wxEVT_CHAR_HOOK, wxevent);
                     result = FilterEvent(eventHook) == Event_Processed ? nil : event;
                  }
                  else
                  {
                     result = FilterEvent(wxevent) == Event_Processed ? nil : event;
                  }

                  mEvent = nullptr;
                  return result;
               }
            }

            return event;
         }
      ];

      // Bug1252: must also install this filter with wxWidgets, else
      // we don't intercept command keys when focus is in a combo box.
      wxEvtHandler::AddFilter(this);
#else

      wxEvtHandler::AddFilter(this);

#endif
   }

   virtual ~CommandManagerEventMonitor()
   {
#if defined(__WXMAC__)
      [NSEvent removeMonitor:mHandler];
#else
      wxEvtHandler::RemoveFilter(this);
#endif
   }

   int FilterEvent(wxEvent& event) override
   {
      // Unguarded exception propagation may crash the program, at least
      // on Mac while in the objective-C closure above
      return GuardedCall< int > ( [&] {
         // Quickly bail if this isn't something we want.
         wxEventType type = event.GetEventType();
         if (type != wxEVT_CHAR_HOOK && type != wxEVT_KEY_UP)
         {
            return Event_Skip;
         }

         // We must have a project since we will be working with the Command Manager
         // and capture handler, both of which are (currently) tied to individual projects.
         //
         // Shouldn't they be tied to the application instead???
         AudacityProject *project = GetActiveProject();
         if (!project || !project->IsEnabled())
         {
            return Event_Skip;
         }

         // Make a copy of the event and (possibly) make it look like a key down
         // event.
         wxKeyEvent key = (wxKeyEvent &) event;
         if (type == wxEVT_CHAR_HOOK)
         {
            key.SetEventType(wxEVT_KEY_DOWN);
         }

         // Give the capture handler first dibs at the event.
         wxWindow *handler = project->GetKeyboardCaptureHandler();
         if (handler && HandleCapture(handler, key))
         {
            return Event_Processed;
         }

         // Capture handler didn't want it, so ask the Command Manager.
         CommandManager *manager = project->GetCommandManager();
         if (manager && manager->FilterKeyEvent(project, key))
         {
            return Event_Processed;
         }

         // Give it back to WX for normal processing.
         return Event_Skip;
      }, MakeSimpleGuard( Event_Skip ) );
   }

private:

   // Returns true if the event was captured and processed
   bool HandleCapture(wxWindow *target, const wxKeyEvent & event)
   {
      if (wxGetTopLevelParent(target) != wxGetTopLevelParent(wxWindow::FindFocus()))
      {
         return false;
      }
      wxEvtHandler *handler = target->GetEventHandler();

      // We make a copy of the event because the capture handler may modify it.
      wxKeyEvent temp = event;

#if defined(__WXGTK__)
      // wxGTK uses the control and alt modifiers to represent ALTGR,
      // so remove it as it might confuse the capture handlers.
      if (temp.GetModifiers() == (wxMOD_CONTROL | wxMOD_ALT))
      {
         temp.SetControlDown(false);
         temp.SetAltDown(false);
      }
#endif

      // Ask the capture handler if the key down/up event is something they it
      // might be interested in handling.
      wxCommandEvent e(EVT_CAPTURE_KEY);
      e.SetEventObject(&temp);
      e.StopPropagation();
      if (!handler->ProcessEvent(e))
      {
         return false;
      }

      // Now, let the handler process the normal key event.
      bool keyDown = temp.GetEventType() == wxEVT_KEY_DOWN;
      temp.WasProcessed();
      temp.StopPropagation();
      wxEventProcessInHandlerOnly onlyDown(temp, handler);
      bool processed = handler->ProcessEvent(temp);

      // Don't go any further if the capture handler didn't process
      // the key down event.
      if (!processed && keyDown)
      {
         return false;
      }

      // At this point the capture handler has either processed a key down event
      // or we're dealing with a key up event.
      //
      // So, only generate the char events for key down events.
      if (keyDown)
      {
         wxString chars = GetUnicodeString(temp);
         for (size_t i = 0, cnt = chars.length(); i < cnt; i++)
         {
            temp = event;
            temp.SetEventType(wxEVT_CHAR);
            temp.WasProcessed();
            temp.StopPropagation();
            temp.m_uniChar = chars[i];
            wxEventProcessInHandlerOnly onlyChar(temp, handler);
            handler->ProcessEvent(temp);
         }
      }

      // We get here for processed key down events or for key up events, whether
      // processed or not.
      return true;
   }

   // Convert the key down event to a unicode string.
   wxString GetUnicodeString(const wxKeyEvent & event)
   {
      wxString chars;

#if defined(__WXMSW__)

      BYTE ks[256];
      GetKeyboardState(ks);
      WCHAR ucode[256];
      int res = ToUnicode(event.GetRawKeyCode(), 0, ks, ucode, 256, 0);
      if (res >= 1)
      {
         chars.Append(ucode, res);
      }

#elif defined(__WXGTK__)

      chars.Append((wxChar) gdk_keyval_to_unicode(event.GetRawKeyCode()));

#elif defined(__WXMAC__)

      if (!mEvent) {
         // TODO:  we got here without getting the NSEvent pointer,
         // as in the combo box case of bug 1252.  We can't compute it!
         // This makes a difference only when there is a capture handler.
         // It's never the case yet that there is one.
         wxASSERT(false);
         return chars;
      }

      NSString *c = [mEvent charactersIgnoringModifiers];
      if ([c length] == 1)
      {
         unichar codepoint = [c characterAtIndex:0];
         if ((codepoint >= 0xF700 && codepoint <= 0xF8FF) || codepoint == 0x7F)
         {
            return chars;
         }
      }

      c = [mEvent characters];
      chars = [c UTF8String];

      TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardInputSource();
      CFDataRef uchr = (CFDataRef)TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
      CFRelease(currentKeyboard);
      if (uchr == NULL)
      {
         return chars;
      }

      const UCKeyboardLayout *keyboardLayout = (const UCKeyboardLayout*)CFDataGetBytePtr(uchr);
      if (keyboardLayout == NULL)
      {
         return chars;
      }

      const UniCharCount maxStringLength = 255;
      UniCharCount actualStringLength = 0;
      UniChar unicodeString[maxStringLength];
      UInt32 nsflags = [mEvent modifierFlags];
      UInt16 modifiers = (nsflags & NSAlphaShiftKeyMask ? alphaLock : 0) |
                         (nsflags & NSShiftKeyMask ? shiftKey : 0) |
                         (nsflags & NSControlKeyMask ? controlKey : 0) |
                         (nsflags & NSAlternateKeyMask ? optionKey : 0) |
                         (nsflags & NSCommandKeyMask ? cmdKey : 0);

      OSStatus status = UCKeyTranslate(keyboardLayout,
                                       [mEvent keyCode],
                                       kUCKeyActionDown,
                                       (modifiers >> 8) & 0xff,
                                       LMGetKbdType(),
                                       0,
                                       &mDeadKeyState,
                                       maxStringLength,
                                       &actualStringLength,
                                       unicodeString);

      if (status != noErr)
      {
         return chars;
      }

      chars = [ [NSString stringWithCharacters:unicodeString
                                       length:(NSInteger)actualStringLength] UTF8String];

#endif

      return chars;
   }

private:

#if defined(__WXMAC__)
   id mHandler;
   NSEvent *mEvent {};
   UInt32 mDeadKeyState;
#endif

}monitor;

///
///  Standard Constructor
///
CommandManager::CommandManager():
   mCurrentID(17000),
   mCurrentMenuName(COMMAND),
   bMakingOccultCommands( false )
{
   mbSeparatorAllowed = false;
   SetMaxList();
}

///
///  Class Destructor.  Includes PurgeData, which removes
///  menubars
CommandManager::~CommandManager()
{
   //WARNING: This removes menubars that could still be assigned to windows!
   PurgeData();
}

const std::vector<NormalizedKeyString> &CommandManager::ExcludedList()
{
   static const auto list = [] {
      // These short cuts are for the max list only....
      const char *const strings[] = {
         // "Ctrl+I",
         "Ctrl+Alt+I",
         "Ctrl+J",
         "Ctrl+Alt+J",
         "Ctrl+Alt+V",
         "Alt+X",
         "Alt+K",
         "Shift+Alt+X",
         "Shift+Alt+K",
         "Alt+L",
         "Shift+Alt+C",
         "Alt+I",
         "Alt+J",
         "Shift+Alt+J",
         "Ctrl+Shift+A",
         "Q",
         //"Shift+J",
         //"Shift+K",
         //"Shift+Home",
         //"Shift+End",
         "Ctrl+[",
         "Ctrl+]",
         "1",
         "Shift+F5",
         "Shift+F6",
         "Shift+F7",
         "Shift+F8",
         "Ctrl+Shift+F5",
         "Ctrl+Shift+F7",
         "Ctrl+Shift+N",
         "Ctrl+Shift+M",
         "Ctrl+Home",
         "Ctrl+End",
         "Shift+C",
         "Alt+Shift+Up",
         "Alt+Shift+Down",
         "Shift+P",
         "Alt+Shift+Left",
         "Alt+Shift+Right",
         "Ctrl+Shift+T",
         //"Command+M",
         //"Option+Command+M",
         "Shift+H",
         "Shift+O",
         "Shift+I",
         "Shift+N",
         "D",
         "A",
         "Alt+Shift+F6",
         "Alt+F6",
      };

      std::vector<NormalizedKeyString> result(
         strings, strings + sizeof(strings)/sizeof(*strings) );
      std::sort( result.begin(), result.end() );
      return result;
   }();
   return list;
}

// CommandManager needs to know which defaults are standard and which are in the
// full (max) list.
void CommandManager::SetMaxList()
{

   // This list is a DUPLICATE of the list in
   // KeyConfigPrefs::OnImportDefaults(wxCommandEvent & event)

   // TODO: At a later date get rid of the maxList entirely and
   // instead use flags in the menu entrys to indicate whether the default
   // shortcut is standard or full.

   mMaxListOnly.clear();

   // if the full list, don't exclude any.
   bool bFull = gPrefs->ReadBool(wxT("/GUI/Shortcuts/FullDefaults"),false);
   if( bFull )
      return;

   mMaxListOnly = ExcludedList();
}


void CommandManager::PurgeData()
{
   // mCommandList contains pointers to CommandListEntrys
   // mMenuBarList contains MenuBarListEntrys.
   // mSubMenuList contains SubMenuListEntrys
   mCommandList.clear();
   mMenuBarList.clear();
   mSubMenuList.clear();

   mCommandNameHash.clear();
   mCommandKeyHash.clear();
   mCommandIDHash.clear();

   mCurrentMenuName = COMMAND;
   mCurrentID = 17000;
}


///
/// Makes a NEW menubar for placement on the top of a project
/// Names it according to the passed-in string argument.
///
/// If the menubar already exists, that's unexpected.
std::unique_ptr<wxMenuBar> CommandManager::AddMenuBar(const wxString & sMenu)
{
   wxMenuBar *menuBar = GetMenuBar(sMenu);
   if (menuBar) {
      wxASSERT(false);
      return {};
   }

   auto result = std::make_unique<wxMenuBar>();
   mMenuBarList.emplace_back(sMenu, result.get());

   return result;
}


///
/// Retrieves the menubar based on the name given in AddMenuBar(name)
///
wxMenuBar * CommandManager::GetMenuBar(const wxString & sMenu) const
{
   for (const auto &entry : mMenuBarList)
   {
      if(entry.name == sMenu)
         return entry.menubar;
   }

   return NULL;
}


///
/// Retrieve the 'current' menubar; either NULL or the
/// last on in the mMenuBarList.
wxMenuBar * CommandManager::CurrentMenuBar() const
{
   if(mMenuBarList.empty())
      return NULL;

   return mMenuBarList.back().menubar;
}

///
/// Typically used to switch back and forth
/// between adding to a hidden menu bar and
/// adding to one that is visible
///
void CommandManager::PopMenuBar()
{
   auto iter = mMenuBarList.end();
   if ( iter != mMenuBarList.begin() )
      mMenuBarList.erase( --iter );
   else
      wxASSERT( false );
}


///
/// This starts a NEW menu
///
wxMenu *CommandManager::BeginMenu(const wxString & tName)
{
   if ( mCurrentMenu )
      return BeginSubMenu( tName );
   else
      return BeginMainMenu( tName );
}


///
/// This attaches a menu, if it's main, to the menubar
//  and in all cases ends the menu
///
void CommandManager::EndMenu()
{
   if ( mSubMenuList.empty() )
      EndMainMenu();
   else
      EndSubMenu();
}


///
/// This starts a NEW menu
///
wxMenu *CommandManager::BeginMainMenu(const wxString & tName)
{
   uCurrentMenu = std::make_unique<wxMenu>();
   mCurrentMenu = uCurrentMenu.get();
   mCurrentMenuName = tName;
   return mCurrentMenu;
}


///
/// This attaches a menu to the menubar and ends the menu
///
void CommandManager::EndMainMenu()
{
   // Add the menu to the menubar after all menu items have been
   // added to the menu to allow OSX to rearrange special menu
   // items like Preferences, About, and Quit.
   wxASSERT(uCurrentMenu);
   CurrentMenuBar()->Append(uCurrentMenu.release(), mCurrentMenuName);
   mCurrentMenu = nullptr;
   mCurrentMenuName = COMMAND;
}


///
/// This starts a NEW submenu, and names it according to
/// the function's argument.
wxMenu* CommandManager::BeginSubMenu(const wxString & tName)
{
   mSubMenuList.push_back
      (std::make_unique< SubMenuListEntry > ( tName, std::make_unique<wxMenu>() ));
   mbSeparatorAllowed = false;
   return mSubMenuList.back()->menu.get();
}


///
/// This function is called after the final item of a SUBmenu is added.
/// Submenu items are added just like regular menu items; they just happen
/// after BeginSubMenu() is called but before EndSubMenu() is called.
void CommandManager::EndSubMenu()
{
   //Save the submenu's information
   SubMenuListEntry tmpSubMenu { std::move( *mSubMenuList.back() ) };

   //Pop off the NEW submenu so CurrentMenu returns the parent of the submenu
   mSubMenuList.pop_back();

   //Add the submenu to the current menu
   CurrentMenu()->Append
      (0, tmpSubMenu.name, tmpSubMenu.menu.release(), tmpSubMenu.name);
   mbSeparatorAllowed = true;
}


///
/// This returns the 'Current' Submenu, which is the one at the
///  end of the mSubMenuList (or NULL, if it doesn't exist).
wxMenu * CommandManager::CurrentSubMenu() const
{
   if(mSubMenuList.empty())
      return NULL;

   return mSubMenuList.back()->menu.get();
}

///
/// This returns the current menu that we're appending to - note that
/// it could be a submenu if BeginSubMenu was called and we haven't
/// reached EndSubMenu yet.
wxMenu * CommandManager::CurrentMenu() const
{
   if(!mCurrentMenu)
      return NULL;

   wxMenu * tmpCurrentSubMenu = CurrentSubMenu();

   if(!tmpCurrentSubMenu)
   {
      return mCurrentMenu;
   }

   return tmpCurrentSubMenu;
}

void CommandManager::SetCurrentMenu(wxMenu * menu)
{
   // uCurrentMenu ought to be null in correct usage
   wxASSERT(!uCurrentMenu);
   // Make sure of it anyway
   uCurrentMenu.reset();

   mCurrentMenu = menu;
}

void CommandManager::ClearCurrentMenu()
{
   // uCurrentMenu ought to be null in correct usage
   wxASSERT(!uCurrentMenu);
   // Make sure of it anyway
   uCurrentMenu.reset();

   mCurrentMenu = nullptr;
}



void CommandManager::AddItem(const CommandID &name,
                             const wxChar *label_in,
                             bool hasDialog,
                             CommandHandlerFinder finder,
                             CommandFunctorPointer callback,
                             CommandFlag flags,
                             const Options &options)
{
   if (options.global) {
      wxASSERT( flags == AlwaysEnabledFlag );
      AddGlobalCommand(
         name, label_in, hasDialog, finder, callback, options.accel );
      return;
   }

   wxASSERT( flags != NoFlagsSpecified );

   auto mask = options.mask;
   if (mask == NoFlagsSpecified)
      mask = flags;

   CommandParameter cookedParameter;
   const auto &parameter = options.parameter;
   if( parameter.empty() )
      cookedParameter = name;
   else
      cookedParameter = parameter;
   CommandListEntry *entry =
      NewIdentifier(name,
         label_in,
         options.longName,
         hasDialog,
         options.accel, CurrentMenu(), finder, callback,
         {}, 0, 0, options.bIsEffect, cookedParameter);
   int ID = entry->id;
   wxString label = GetLabelWithDisabledAccel(entry);

   SetCommandFlags(name, flags, mask);


   auto checkmark = options.check;
   if (checkmark >= 0) {
      CurrentMenu()->AppendCheckItem(ID, label);
      CurrentMenu()->Check(ID, checkmark != 0);
   }
   else {
      CurrentMenu()->Append(ID, label);
   }

   mbSeparatorAllowed = true;
}

///
/// Add a list of menu items to the current menu.  When the user selects any
/// one of these, the given functor will be called
/// with its position in the list as the index number.
/// When you call Enable on this command name, it will enable or disable
/// all of the items at once.
void CommandManager::AddItemList(const CommandID & name,
                                 const ComponentInterfaceSymbol items[],
                                 size_t nItems,
                                 CommandHandlerFinder finder,
                                 CommandFunctorPointer callback,
                                 CommandFlag flags,
                                 bool bIsEffect)
{
   for (size_t i = 0, cnt = nItems; i < cnt; i++) {
      auto translated = items[i].Translation();
      CommandListEntry *entry = NewIdentifier(name,
                                              translated,
                                              translated,
                                              // No means yet to specify !
                                              false,
                                              CurrentMenu(),
                                              finder,
                                              callback,
                                              items[i].Internal(),
                                              i,
                                              cnt,
                                              bIsEffect);
      entry->mask = entry->flags = flags;
      CurrentMenu()->Append(entry->id, GetLabel(entry));
      mbSeparatorAllowed = true;
   }
}

///
/// Add a command that doesn't appear in a menu.  When the key is pressed, the
/// given function pointer will be called (via the CommandManagerListener)
void CommandManager::AddCommand(const CommandID &name,
                                const wxChar *label,
                                CommandHandlerFinder finder,
                                CommandFunctorPointer callback,
                                CommandFlag flags)
{
   AddCommand(name, label, finder, callback, wxT(""), flags);
}

void CommandManager::AddCommand(const CommandID &name,
                                const wxChar *label_in,
                                CommandHandlerFinder finder,
                                CommandFunctorPointer callback,
                                const wxChar *accel,
                                CommandFlag flags)
{
   wxASSERT( flags != NoFlagsSpecified );

   NewIdentifier(name, label_in, label_in, false, accel, NULL, finder, callback, {}, 0, 0, false, {});

   SetCommandFlags(name, flags, flags);
}

void CommandManager::AddGlobalCommand(const CommandID &name,
                                      const wxChar *label_in,
                                      bool hasDialog,
                                      CommandHandlerFinder finder,
                                      CommandFunctorPointer callback,
                                      const wxChar *accel)
{
   CommandListEntry *entry =
      NewIdentifier(name, label_in, label_in, hasDialog, accel, NULL, finder, callback,
                    {}, 0, 0, false, {});

   entry->enabled = false;
   entry->isGlobal = true;
   entry->flags = AlwaysEnabledFlag;
   entry->mask = AlwaysEnabledFlag;
}

void CommandManager::AddSeparator()
{
   if( mbSeparatorAllowed )
      CurrentMenu()->AppendSeparator();
   mbSeparatorAllowed = false; // boolean to prevent too many separators.
}

int CommandManager::NextIdentifier(int ID)
{
   ID++;

   //Skip the reserved identifiers used by wxWidgets
   if((ID >= wxID_LOWEST) && (ID <= wxID_HIGHEST))
      ID = wxID_HIGHEST+1;

   return ID;
}

///Given all of the information for a command, comes up with a NEW unique
///ID, adds it to a list, and returns the ID.
///WARNING: Does this conflict with the identifiers set for controls/windows?
///If it does, a workaround may be to keep controls below wxID_LOWEST
///and keep menus above wxID_HIGHEST
CommandListEntry *CommandManager::NewIdentifier(const CommandID & name,
                                                const wxString & label,
                                                const wxString & longLabel,
                                                bool hasDialog,
                                                wxMenu *menu,
                                                CommandHandlerFinder finder,
                                                CommandFunctorPointer callback,
                                                const CommandID &nameSuffix,
                                                int index,
                                                int count,
                                                bool bIsEffect)
{
   return NewIdentifier(name,
                        label.BeforeFirst(wxT('\t')),
                        longLabel.BeforeFirst(wxT('\t')),
                        hasDialog,
                        label.AfterFirst(wxT('\t')),
                        menu,
                        finder,
                        callback,
                        nameSuffix,
                        index,
                        count,
                        bIsEffect,
                        {});
}

CommandListEntry *CommandManager::NewIdentifier(const CommandID & nameIn,
   const wxString & label,
   const wxString & longLabel,
   bool hasDialog,
   const wxString & accel,
   wxMenu *menu,
   CommandHandlerFinder finder,
   CommandFunctorPointer callback,
   const CommandID &nameSuffix,
   int index,
   int count,
   bool bIsEffect,
   const CommandParameter &parameter)
{
   const bool multi = !nameSuffix.empty();
   auto name = nameIn;

   // If we have the identifier already, reuse it.
   CommandListEntry *prev = mCommandNameHash[name];
   if (!prev);
   else if( prev->label != label );
   else if( multi );
   else
      return prev;

   {
      auto entry = std::make_unique<CommandListEntry>();

      wxString labelPrefix;
      if (!mSubMenuList.empty()) {
         labelPrefix = mSubMenuList.back()->name;
      }

      // For key bindings for commands with a list, such as align,
      // the name in prefs is the category name plus the effect name.
      // This feature is not used for built-in effects.
      if (multi)
         name = wxString::Format(wxT("%s_%s"), name, nameSuffix);

      // wxMac 2.5 and higher will do special things with the
      // Preferences, Exit (Quit), and About menu items,
      // if we give them the right IDs.
      // Otherwise we just pick increasing ID numbers for each NEW
      // command.  Note that the name string we are comparing
      // ("About", "Preferences") is the internal command name
      // (untranslated), not the label that actually appears in the
      // menu (which might be translated).

      mCurrentID = NextIdentifier(mCurrentID);
      entry->id = mCurrentID;
      entry->parameter = parameter;

#if defined(__WXMAC__)
      if (name == wxT("Preferences"))
         entry->id = wxID_PREFERENCES;
      else if (name == wxT("Exit"))
         entry->id = wxID_EXIT;
      else if (name == wxT("About"))
         entry->id = wxID_ABOUT;
#endif

      entry->name = name;
      entry->label = label;
      entry->longLabel = longLabel.empty() ? label : longLabel;
      entry->hasDialog = hasDialog;
      entry->key = NormalizedKeyString{ accel.BeforeFirst(wxT('\t')) };
      entry->defaultKey = entry->key;
      entry->labelPrefix = labelPrefix;
      entry->labelTop = wxMenuItem::GetLabelText(mCurrentMenuName);
      entry->menu = menu;
      entry->finder = finder;
      entry->callback = callback;
      entry->isEffect = bIsEffect;
      entry->multi = multi;
      entry->index = index;
      entry->count = count;
      entry->flags = entry->mask = AlwaysEnabledFlag;
      entry->enabled = true;
      entry->skipKeydown = (accel.Find(wxT("\tskipKeydown")) != wxNOT_FOUND);
      entry->wantKeyup = (accel.Find(wxT("\twantKeyup")) != wxNOT_FOUND) || entry->skipKeydown;
      entry->isGlobal = false;
      entry->isOccult = bMakingOccultCommands;

      // Exclude accelerators that are in the MaxList.
      // Note that the default is unaffected, intentionally so.
      // There are effectively two levels of default, the full (max) list
      // and the normal reduced list.
      if( std::binary_search( mMaxListOnly.begin(), mMaxListOnly.end(),
                              entry->key ) )
         entry->key = {};

      // Key from preferences overrides the default key given
      gPrefs->SetPath(wxT("/NewKeys"));
      if (gPrefs->HasEntry(entry->name)) {
         entry->key =
            NormalizedKeyString{ gPrefs->Read(entry->name, entry->key.Raw()) };
      }
      gPrefs->SetPath(wxT("/"));

      mCommandList.push_back(std::move(entry));
      // Don't use the variable entry eny more!
   }

   // New variable
   CommandListEntry *entry = &*mCommandList.back();
   mCommandIDHash[entry->id] = entry;

#if defined(__WXDEBUG__)
   prev = mCommandNameHash[entry->name];
   if (prev) {
      // Under Linux it looks as if we may ask for a newID for the same command
      // more than once.  So it's only an error if two different commands
      // have the exact same name.
      if( prev->label != entry->label )
      {
         wxLogDebug(wxT("Command '%s' defined by '%s' and '%s'"),
                    entry->name,
                    prev->label.BeforeFirst(wxT('\t')),
                    entry->label.BeforeFirst(wxT('\t')));
         wxFAIL_MSG(wxString::Format(wxT("Command '%s' defined by '%s' and '%s'"),
                    entry->name,
                    prev->label.BeforeFirst(wxT('\t')),
                    entry->label.BeforeFirst(wxT('\t'))));
      }
   }
#endif
   mCommandNameHash[entry->name] = entry;

   if (!entry->key.empty()) {
      mCommandKeyHash[entry->key] = entry;
   }

   return entry;
}

wxString CommandManager::GetLabel(const CommandListEntry *entry) const
{
   wxString label = entry->label;
   if (!entry->key.empty())
   {
      label += wxT("\t") + entry->key.Raw();
   }

   return label;
}

// A label that may have its accelerator disabled.
// The problem is that as soon as we show accelerators in the menu, the menu might
// catch them in normal wxWidgets processing, rather than passing the key presses on
// to the controls that had the focus.  We would like all the menu accelerators to be
// disabled, in fact.
wxString CommandManager::GetLabelWithDisabledAccel(const CommandListEntry *entry) const
{
   wxString label = entry->label;
#if 1
   wxString Accel;
   do{
      if (!entry->key.empty())
      {
         // Dummy accelerator that looks Ok in menus but is non functional.
         // Note the space before the key.
#ifdef __WXMSW__
         auto key = entry->key.Raw();
         Accel = wxString("\t ") + key;
         if( key.StartsWith("Left" )) break;
         if( key.StartsWith("Right")) break;
         if( key.StartsWith("Up" )) break;
         if( key.StartsWith("Down")) break;
         if( key.StartsWith("Return")) break;
         if( key.StartsWith("Tab")) break;
         if( key.StartsWith("Shift+Tab")) break;
         if( key.StartsWith("0")) break;
         if( key.StartsWith("1")) break;
         if( key.StartsWith("2")) break;
         if( key.StartsWith("3")) break;
         if( key.StartsWith("4")) break;
         if( key.StartsWith("5")) break;
         if( key.StartsWith("6")) break;
         if( key.StartsWith("7")) break;
         if( key.StartsWith("8")) break;
         if( key.StartsWith("9")) break;
         // Uncomment the below so as not to add the illegal accelerators.
         // Accel = "";
         //if( entry->key.StartsWith("Space" )) break;
         // These ones appear to be illegal already and mess up accelerator processing.
         if( key.StartsWith("NUMPAD_ENTER" )) break;
         if( key.StartsWith("Backspace" )) break;
         if( key.StartsWith("Delete" )) break;
#endif
         //wxLogDebug("Added Accel:[%s][%s]", entry->label, entry->key );
         // Normal accelerator.
         Accel = wxString("\t") + entry->key.Raw();
      }
   } while (false );
   label += Accel;
#endif
   return label;
}
///Enables or disables a menu item based on its name (not the
///label in the menu bar, but the name of the command.)
///If you give it the name of a multi-item (one that was
///added using AddItemList(), it will enable or disable all
///of them at once
void CommandManager::Enable(CommandListEntry *entry, bool enabled)
{
   if (!entry->menu) {
      entry->enabled = enabled;
      return;
   }

   // LL:  Refresh from real state as we can get out of sync on the
   //      Mac due to its reluctance to enable menus when in a modal
   //      state.
   entry->enabled = entry->menu->IsEnabled(entry->id);

   // Only enabled if needed
   if (entry->enabled != enabled) {
      entry->menu->Enable(entry->id, enabled);
      entry->enabled = entry->menu->IsEnabled(entry->id);
   }

   if (entry->multi) {
      int i;
      int ID = entry->id;

      for(i=1; i<entry->count; i++) {
         ID = NextIdentifier(ID);

         // This menu item is not necessarily in the same menu, because
         // multi-items can be spread across multiple sub menus
         CommandListEntry *multiEntry = mCommandIDHash[ID];
         if (multiEntry) {
            wxMenuItem *item = multiEntry->menu->FindItem(ID);

         if (item) {
            item->Enable(enabled);
         } else {
            wxLogDebug(wxT("Warning: Menu entry with id %i in %s not found"),
                ID, (const wxChar*)entry->name);
         }
         } else {
            wxLogDebug(wxT("Warning: Menu entry with id %i not in hash"), ID);
         }
      }
   }
}

void CommandManager::Enable(const wxString &name, bool enabled)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry || !entry->menu) {
      wxLogDebug(wxT("Warning: Unknown command enabled: '%s'"),
                 (const wxChar*)name);
      return;
   }

   Enable(entry, enabled);
}

void CommandManager::EnableUsingFlags(CommandFlag flags, CommandMask mask)
{
   for(const auto &entry : mCommandList) {
      if (entry->multi && entry->index != 0)
         continue;
      if( entry->isOccult )
         continue;

      auto combinedMask = (mask & entry->mask);
      if (combinedMask) {
         bool enable = ((flags & combinedMask) ==
                        (entry->flags & combinedMask));
         Enable(entry.get(), enable);
      }
   }
}

bool CommandManager::GetEnabled(const CommandID &name)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry || !entry->menu) {
      wxLogDebug(wxT("Warning: command doesn't exist: '%s'"),
                 (const wxChar*)name);
      return false;
   }
   return entry->enabled;
}

void CommandManager::Check(const CommandID &name, bool checked)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry || !entry->menu || entry->isOccult) {
      return;
   }
   entry->menu->Check(entry->id, checked);
}

///Changes the label text of a menu item
void CommandManager::Modify(const wxString &name, const wxString &newLabel)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (entry && entry->menu) {
      entry->label = newLabel;
      entry->menu->SetLabel(entry->id, GetLabel(entry));
   }
}

void CommandManager::SetKeyFromName(const CommandID &name,
                                    const NormalizedKeyString &key)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (entry) {
      entry->key = key;
   }
}

void CommandManager::SetKeyFromIndex(int i, const NormalizedKeyString &key)
{
   const auto &entry = mCommandList[i];
   entry->key = key;
}

void CommandManager::TellUserWhyDisallowed( const wxString & Name, CommandFlag flagsGot, CommandMask flagsRequired )
{
   // The default string for 'reason' is a catch all.  I hope it won't ever be seen
   // and that we will get something more specific.
   wxString reason = _("There was a problem with your last action. If you think\nthis is a bug, please tell us exactly where it occurred.");
   // The default title string is 'Disallowed'.
   wxString title = _("Disallowed");
   wxString helpPage;

   auto missingFlags = flagsRequired & (~flagsGot );
   if( missingFlags & AudioIONotBusyFlag )
      // This reason will not be shown, because options that require it will be greyed our.
      reason = _("You can only do this when playing and recording are\nstopped. (Pausing is not sufficient.)");
   else if( missingFlags & StereoRequiredFlag )
      // This reason will not be shown, because the stereo-to-mono is greyed out if not allowed.
      reason = _("You must first select some stereo audio to perform this\naction. (You cannot use this with mono.)");
   // In reporting the issue with cut or copy, we don't tell the user they could also select some text in a label.
   else if(( missingFlags & TimeSelectedFlag ) || (missingFlags &CutCopyAvailableFlag )){
      title = _("No Audio Selected");
#ifdef __WXMAC__
      // i18n-hint: %s will be replaced by the name of an action, such as Normalize, Cut, Fade.
      reason = wxString::Format( _("Select the audio for %s to use (for example, Cmd + A to Select All) then try again."
      // No need to explain what a help button is for.
      // "\n\nClick the Help button to learn more about selection methods."
      ), Name );

#else
      // i18n-hint: %s will be replaced by the name of an action, such as Normalize, Cut, Fade.
      reason = wxString::Format( _("Select the audio for %s to use (for example, Ctrl + A to Select All) then try again."
      // No need to explain what a help button is for.
      // "\n\nClick the Help button to learn more about selection methods."
      ), Name );
#endif
      helpPage = "Selecting_Audio_-_the_basics";
   }
   else if( missingFlags & WaveTracksSelectedFlag)
      reason = _("You must first select some audio to perform this action.\n(Selecting other kinds of track won't work.)");
   else if ( missingFlags & TracksSelectedFlag )
      // i18n-hint: %s will be replaced by the name of an action, such as "Remove Tracks".
      reason = wxString::Format(_("\"%s\" requires one or more tracks to be selected."), Name);
   // If the only thing wrong was no tracks, we do nothing and don't report a problem
   else if( missingFlags == TracksExistFlag )
      return;
   // Likewise return if it was just no tracks, and track panel did not have focus.  (e.g. up-arrow to move track)
   else if( missingFlags == (TracksExistFlag | TrackPanelHasFocus) )
      return;
   // Likewise as above too...
   else if( missingFlags == TrackPanelHasFocus )
      return;

   // Does not have the warning icon...
   ShowErrorDialog(
      NULL,
      title,
      reason,
      helpPage);
}

wxString CommandManager::DescribeCommandsAndShortcuts
(const TranslatedInternalString commands[], size_t nCommands) const
{
   wxString mark;
   // This depends on the language setting and may change in-session after
   // change of preferences:
   bool rtl = (wxLayout_RightToLeft == wxTheApp->GetLayoutDirection());
   if (rtl)
      mark = wxT("\u200f");

   static const wxString &separatorFormat = wxT("%s / %s");
   wxString result;
   for (size_t ii = 0; ii < nCommands; ++ii) {
      const auto &pair = commands[ii];
      // If RTL, then the control character forces right-to-left sequencing of
      // "/" -separated command names, and puts any "(...)" shortcuts to the
      // left, consistently with accelerators in menus (assuming matching
      // operating system prefernces for language), even if the command name
      // was missing from the translation file and defaulted to the English.
      auto piece = wxString::Format(wxT("%s%s"), mark, pair.Translated());

      auto name = pair.Internal();
      if (!name.empty()) {
         auto keyStr = GetKeyFromName(name);
         if (!keyStr.empty()){
            auto keyString = keyStr.Display(true);
            auto format = wxT("%s %s(%s)");
#ifdef __WXMAC__
            // The unicode controls push and pop left-to-right embedding.
            // This keeps the directionally weak characters, such as uparrow
            // for Shift, left of the key name,
            // consistently with how menu accelerators appear, even when the
            // system language is RTL.
            format = wxT("%s %s(\u202a%s\u202c)");
#endif
            // The mark makes correctly placed parentheses for RTL, even
            // in the case that the piece is untranslated.
            piece = wxString::Format(format, piece, mark, keyString);
         }
      }

      if (result.empty())
         result = piece;
      else
         result = wxString::Format(separatorFormat, result, piece);
   }
   return result;
}

///
///
///
bool CommandManager::FilterKeyEvent(AudacityProject *project, const wxKeyEvent & evt, bool permit)
{
   CommandListEntry *entry = mCommandKeyHash[KeyEventToKeyString(evt)];
   if (entry == NULL)
   {
      return false;
   }

   int type = evt.GetEventType();

   // Global commands aren't tied to any specific project
   if (entry->isGlobal && type == wxEVT_KEY_DOWN)
   {
      // Global commands are always disabled so they do not interfere with the
      // rest of the command handling.  But, to use the common handler, we
      // enable them temporarily and then disable them again after handling.
      // LL:  Why do they need to be disabled???
      entry->enabled = false;
      auto cleanup = valueRestorer( entry->enabled, true );
      return HandleCommandEntry(entry, NoFlagsSpecified, NoFlagsSpecified, &evt);
   }

   wxWindow * pFocus = wxWindow::FindFocus();
   wxWindow * pParent = wxGetTopLevelParent( pFocus );
   bool validTarget = pParent == project;
   // Bug 1557.  MixerBoard should count as 'destined for project'
   // MixerBoard IS a TopLevelWindow, and its parent is the project.
   if( pParent && pParent->GetParent() == project){
      if( dynamic_cast< TopLevelKeystrokeHandlingWindow* >( pParent ) != NULL )
         validTarget = true;
   }
   validTarget = validTarget && wxEventLoop::GetActive()->IsMain();

   // Any other keypresses must be destined for this project window
   if (!permit && !validTarget )
   {
      return false;
   }

   auto flags = GetMenuManager(*project).GetUpdateFlags(*project);

   wxKeyEvent temp = evt;

   // Possibly let wxWidgets do its normal key handling IF it is one of
   // the standard navigation keys.
   if((type == wxEVT_KEY_DOWN) || (type == wxEVT_KEY_UP ))
   {
      wxWindow * pWnd = wxWindow::FindFocus();
      bool bIntercept =
         pWnd && !dynamic_cast< NonKeystrokeInterceptingWindow * >( pWnd );

      //wxLogDebug("Focus: %p TrackPanel: %p", pWnd, pTrackPanel );
      // We allow the keystrokes below to be handled by wxWidgets controls IF we are
      // in some sub window rather than in the TrackPanel itself.
      // Otherwise they will go to our command handler and if it handles them
      // they will NOT be available to wxWidgets.
      if( bIntercept ){
         switch( evt.GetKeyCode() ){
         case WXK_LEFT:
         case WXK_RIGHT:
         case WXK_UP:
         case WXK_DOWN:
         // Don't trap WXK_SPACE (Bug 1727 - SPACE not starting/stopping playback
         // when cursor is in a time control)
         // case WXK_SPACE:
         case WXK_TAB:
         case WXK_BACK:
         case WXK_HOME:
         case WXK_END:
         case WXK_RETURN:
         case WXK_NUMPAD_ENTER:
         case WXK_DELETE:
         case '0':
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
            return false;
         }
      }
   }

   if (type == wxEVT_KEY_DOWN)
   {
      if (entry->skipKeydown)
      {
         return true;
      }
      return HandleCommandEntry(entry, flags, NoFlagsSpecified, &temp);
   }

   if (type == wxEVT_KEY_UP && entry->wantKeyup)
   {
      return HandleCommandEntry(entry, flags, NoFlagsSpecified, &temp);
   }

   return false;
}

/// HandleCommandEntry() takes a CommandListEntry and executes it
/// returning true iff successful.  If you pass any flags,
///the command won't be executed unless the flags are compatible
///with the command's flags.
bool CommandManager::HandleCommandEntry(const CommandListEntry * entry,
                                        CommandFlag flags, CommandMask mask, const wxEvent * evt)
{
   if (!entry )
      return false;

   if (flags != AlwaysEnabledFlag && !entry->enabled)
      return false;

   auto proj = GetActiveProject();

   auto combinedMask = (mask & entry->mask);
   if (combinedMask) {

      wxASSERT( proj );
      if( !proj )
         return false;

      wxString NiceName = entry->label;
      NiceName.Replace("&", "");// remove &
      NiceName.Replace(".","");// remove ...
      // NB: The call may have the side effect of changing flags.
      bool allowed =
         GetMenuManager(*proj).ReportIfActionNotAllowed( *proj,
            NiceName, flags, entry->flags, combinedMask );
      // If the function was disallowed, it STILL should count as having been
      // handled (by doing nothing or by telling the user of the problem).
      // Otherwise we may get other handlers having a go at obeying the command.
      if (!allowed)
         return true;
   }

   const CommandContext context{ *proj, evt, entry->index, entry->parameter };
   auto &handler = entry->finder(*proj);
   (handler.*(entry->callback))(context);

   return true;
}

///Call this when a menu event is received.
///If it matches a command, it will call the appropriate
///CommandManagerListener function.  If you pass any flags,
///the command won't be executed unless the flags are compatible
///with the command's flags.
#include "../prefs/PrefsDialog.h"
#include "../prefs/KeyConfigPrefs.h"
bool CommandManager::HandleMenuID(int id, CommandFlag flags, CommandMask mask)
{
   CommandListEntry *entry = mCommandIDHash[id];

#ifdef EXPERIMENTAL_EASY_CHANGE_KEY_BINDINGS
   if (::wxGetMouseState().ShiftDown()) {
      // Only want one page of the preferences
      KeyConfigPrefsFactory keyConfigPrefsFactory{ entry->name };
      PrefsDialog::Factories factories;
      factories.push_back(&keyConfigPrefsFactory);
      GlobalPrefsDialog dialog(GetActiveProject(), factories);
      dialog.ShowModal();
      MenuCreator::RebuildAllMenuBars();
      return true;
   }
#endif

   return HandleCommandEntry( entry, flags, mask );
}

/// HandleTextualCommand() allows us a limitted version of script/batch
/// behavior, since we can get from a string command name to the actual
/// code to run.
bool CommandManager::HandleTextualCommand(const CommandID & Str, const CommandContext & context, CommandFlag flags, CommandMask mask)
{
   if( Str.empty() )
      return false;
   // Linear search for now...
   for (const auto &entry : mCommandList)
   {
      if (!entry->multi)
      {
         // Testing against labelPrefix too allows us to call Nyquist functions by name.
         if( Str.IsSameAs( entry->name, false ) || Str.IsSameAs( entry->labelPrefix, false ))
         {
            return HandleCommandEntry( entry.get(), flags, mask);
         }
      }
      else
      {
         // Handle multis too...
         if( Str.IsSameAs( entry->name, false ) )
         {
            return HandleCommandEntry( entry.get(), flags, mask);
         }
      }
   }
   // Not one of the singleton commands.
   // We could/should try all the list-style commands.
   // instead we only try the effects.
   AudacityProject * proj = GetActiveProject();
   if( !proj )
   {
      return false;
   }

   PluginManager & pm = PluginManager::Get();
   EffectManager & em = EffectManager::Get();
   const PluginDescriptor *plug = pm.GetFirstPlugin(PluginTypeEffect);
   while (plug)
   {
      if (em.GetCommandIdentifier(plug->GetID()).IsSameAs(Str, false))
      {
         return PluginActions::DoEffect(
            plug->GetID(), context,
            PluginActions::kConfigured);
      }
      plug = pm.GetNextPlugin(PluginTypeEffect);
   }

   return false;
}

void CommandManager::GetCategories(wxArrayString &cats)
{
   cats.clear();

   for (const auto &entry : mCommandList) {
      wxString cat = entry->labelTop;
      if ( ! make_iterator_range( cats ).contains(cat) ) {
         cats.push_back(cat);
      }
   }
#if 0
   mCommandList.size(); i++) {
      if (includeMultis || !mCommandList[i]->multi)
         names.push_back(mCommandList[i]->name);
   }

   AudacityProject *p = GetActiveProject();
   if (p == NULL) {
      return;
   }

   wxMenuBar *bar = p->GetMenuBar();
   size_t cnt = bar->GetMenuCount();
   for (size_t i = 0; i < cnt; i++) {
      cats.push_back(bar->GetMenuLabelText(i));
   }

   cats.push_back(COMMAND);
#endif
}

void CommandManager::GetAllCommandNames(CommandIDs &names,
                                        bool includeMultis) const
{
   for(const auto &entry : mCommandList) {
      if ( entry->isEffect )
         continue;
      if (!entry->multi)
         names.push_back(entry->name);
      else if( includeMultis )
         names.push_back(entry->name );// + wxT(":")/*+ mCommandList[i]->label*/);
   }
}

void CommandManager::GetAllCommandLabels(wxArrayString &names,
                                         std::vector<bool> &vHasDialog,
                                        bool includeMultis) const
{
   vHasDialog.clear();
   for(const auto &entry : mCommandList) {
      // This is fetching commands from the menus, for use as batch commands.
      // Until we have properly merged EffectManager and CommandManager
      // we explicitly exclude effects, as they are already handled by the
      // effects Manager.
      if ( entry->isEffect )
         continue;
      if (!entry->multi)
         names.push_back(entry->longLabel), vHasDialog.push_back(entry->hasDialog);
      else if( includeMultis )
         names.push_back(entry->longLabel), vHasDialog.push_back(entry->hasDialog);
   }
}

void CommandManager::GetAllCommandData(
   CommandIDs &names,
   std::vector<NormalizedKeyString> &keys,
   std::vector<NormalizedKeyString> &default_keys,
   wxArrayString &labels,
   wxArrayString &categories,
#if defined(EXPERIMENTAL_KEY_VIEW)
   wxArrayString &prefixes,
#endif
   bool includeMultis)
{
   for(const auto &entry : mCommandList) {
      // GetAllCommandData is used by KeyConfigPrefs.
      // It does need the effects.
      //if ( entry->isEffect )
      //   continue;
      if (!entry->multi)
      {
         names.push_back(entry->name);
         keys.push_back(entry->key);
         default_keys.push_back(entry->defaultKey);
         labels.push_back(entry->label);
         categories.push_back(entry->labelTop);
#if defined(EXPERIMENTAL_KEY_VIEW)
         prefixes.push_back(entry->labelPrefix);
#endif
      }
      else if( includeMultis )
      {
         names.push_back(entry->name);
         keys.push_back(entry->key);
         default_keys.push_back(entry->defaultKey);
         labels.push_back(entry->label);
         categories.push_back(entry->labelTop);
#if defined(EXPERIMENTAL_KEY_VIEW)
         prefixes.push_back(entry->labelPrefix);
#endif
      }
   }
}

CommandID CommandManager::GetNameFromID(int id)
{
   CommandListEntry *entry = mCommandIDHash[id];
   if (!entry)
      return {};
   return entry->name;
}

wxString CommandManager::GetLabelFromName(const CommandID &name)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry)
      return wxT("");

   return entry->longLabel;
}

wxString CommandManager::GetPrefixedLabelFromName(const CommandID &name)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry)
      return wxT("");

#if defined(EXPERIMENTAL_KEY_VIEW)
   wxString prefix;
   if (!entry->labelPrefix.empty()) {
      prefix = entry->labelPrefix + wxT(" - ");
   }
   return wxMenuItem::GetLabelText(prefix + entry->label);
#else
   return wxString(entry->labelPrefix + wxT(" ") + entry->label).Trim(false).Trim(true);
#endif
}

wxString CommandManager::GetCategoryFromName(const CommandID &name)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry)
      return wxT("");

   return entry->labelTop;
}

NormalizedKeyString CommandManager::GetKeyFromName(const CommandID &name) const
{
   CommandListEntry *entry =
      // May create a NULL entry
      const_cast<CommandManager*>(this)->mCommandNameHash[name];
   if (!entry)
      return {};

   return entry->key;
}

NormalizedKeyString CommandManager::GetDefaultKeyFromName(const CommandID &name)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (!entry)
      return {};

   return entry->defaultKey;
}

bool CommandManager::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if (!wxStrcmp(tag, wxT("audacitykeyboard"))) {
      mXMLKeysRead = 0;
   }

   if (!wxStrcmp(tag, wxT("command"))) {
      wxString name;
      NormalizedKeyString key;

      while(*attrs) {
         const wxChar *attr = *attrs++;
         const wxChar *value = *attrs++;

         if (!value)
            break;

         if (!wxStrcmp(attr, wxT("name")) && XMLValueChecker::IsGoodString(value))
            name = value;
         if (!wxStrcmp(attr, wxT("key")) && XMLValueChecker::IsGoodString(value))
            key = NormalizedKeyString{ value };
      }

      if (mCommandNameHash[name]) {
         mCommandNameHash[name]->key = key;
         mXMLKeysRead++;
      }
   }

   return true;
}

void CommandManager::HandleXMLEndTag(const wxChar *tag)
{
   if (!wxStrcmp(tag, wxT("audacitykeyboard"))) {
      AudacityMessageBox(wxString::Format(_("Loaded %d keyboard shortcuts\n"),
                                    mXMLKeysRead),
                   _("Loading Keyboard Shortcuts"),
                   wxOK | wxCENTRE);
   }
}

XMLTagHandler *CommandManager::HandleXMLChild(const wxChar * WXUNUSED(tag))
{
   return this;
}

void CommandManager::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   xmlFile.StartTag(wxT("audacitykeyboard"));
   xmlFile.WriteAttr(wxT("audacityversion"), AUDACITY_VERSION_STRING);

   for(const auto &entry : mCommandList) {
      wxString label = entry->label;
      label = wxMenuItem::GetLabelText(label.BeforeFirst(wxT('\t')));

      xmlFile.StartTag(wxT("command"));
      xmlFile.WriteAttr(wxT("name"), entry->name);
      xmlFile.WriteAttr(wxT("label"), label);
      xmlFile.WriteAttr(wxT("key"), entry->key.Raw());
      xmlFile.EndTag(wxT("command"));
   }

   xmlFile.EndTag(wxT("audacitykeyboard"));
}

void CommandManager::BeginOccultCommands()
{
   // To do:  perhaps allow occult item switching at lower levels of the
   // menu tree.
   wxASSERT( !CurrentMenu() );

   // Make a temporary menu bar collecting items added after.
   // This bar will be discarded but other side effects on the command
   // manager persist.
   mTempMenuBar = AddMenuBar(wxT("ext-menu"));
   bMakingOccultCommands = true;
}

void CommandManager::EndOccultCommands()
{
   PopMenuBar();
   bMakingOccultCommands = false;
   mTempMenuBar.reset();
}

void CommandManager::SetCommandFlags(const CommandID &name,
                                     CommandFlag flags, CommandMask mask)
{
   CommandListEntry *entry = mCommandNameHash[name];
   if (entry) {
      entry->flags = flags;
      entry->mask = mask;
   }
}

#if defined(__WXDEBUG__)
void CommandManager::CheckDups()
{
   int cnt = mCommandList.size();
   for (size_t j = 0;  (int)j < cnt; j++) {
      if (mCommandList[j]->key.empty()) {
         continue;
      }

      if (mCommandList[j]->label.AfterLast(wxT('\t')) == wxT("allowDup")) {
         continue;
      }

      for (size_t i = 0; (int)i < cnt; i++) {
         if (i == j) {
            continue;
         }

         if (mCommandList[i]->key == mCommandList[j]->key) {
            wxString msg;
            msg.Printf(wxT("key combo '%s' assigned to '%s' and '%s'"),
                       mCommandList[i]->key.Raw(),
                       mCommandList[i]->label.BeforeFirst(wxT('\t')),
                       mCommandList[j]->label.BeforeFirst(wxT('\t')));
            wxASSERT_MSG(mCommandList[i]->key != mCommandList[j]->key, msg);
         }
      }
   }
}

#endif
