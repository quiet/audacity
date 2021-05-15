/**********************************************************************

  Audacity: A Digital Audio Editor

  AudacityLogger.cpp

******************************************************************//**

\class AudacityLogger
\brief AudacityLogger is a thread-safe logger class

Provides thread-safe logging based on the wxWidgets log facility.

*//*******************************************************************/


#include "Audacity.h" // This should always be included first
#include "AudacityLogger.h"

#include "Experimental.h"

#include "FileNames.h"
#include "ShuttleGui.h"

#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/icon.h>
#include <wx/settings.h>

#include "../images/AudacityLogoAlpha.xpm"
#include "widgets/ErrorDialog.h"
#include "Internat.h"

//
// AudacityLogger class
//
// Two reasons for this class instead of the wxLogWindow class (or any WX GUI logging class)
//
// 1)  If wxLogWindow is used and initialized before the Mac's "root" window, then
//     Audacity may crash when terminating.  It's not fully understood why this occurs
//     but it probably has to do with the order of deletion.  However, deferring the
//     creation of the log window until it is actually shown circumvents the problem.
// 2)  By providing an Audacity specific logging class, it can be made thread-safe and,
//     as such, can be used by the ever growing threading within Audacity.
//
enum
{
   LoggerID_Save = wxID_HIGHEST + 1,
   LoggerID_Clear,
   LoggerID_Close
};

AudacityLogger::AudacityLogger()
:  wxEvtHandler(),
   wxLog()
{
   mText = NULL;
   mUpdated = false;
}

void AudacityLogger::Flush()
{
   if (mUpdated && mFrame && mFrame->IsShown()) {
      mUpdated = false;
      mText->ChangeValue(mBuffer);
   }
}

void AudacityLogger::DoLogText(const wxString & str)
{
   if (!wxIsMainThread()) {
      wxMutexGuiEnter();
   }

   if (mBuffer.empty()) {
      wxString stamp;

      TimeStamp(&stamp);

      mBuffer << stamp << _TS("Audacity ") << AUDACITY_VERSION_STRING << wxT("\n");
   }

   mBuffer << str << wxT("\n");

   mUpdated = true;

   Flush();

   if (!wxIsMainThread()) {
      wxMutexGuiLeave();
   }
}

void AudacityLogger::Show(bool show)
{
   // Hide the frame if created, otherwise do nothing
   if (!show) {
      if (mFrame) {
         mFrame->Show(false);
      }
      return;
   }

   // If the frame already exists, refresh its contents and show it
   if (mFrame) {
      if (!mFrame->IsShown()) {
         mText->ChangeValue(mBuffer);
         mText->SetInsertionPointEnd();
         mText->ShowPosition(mText->GetLastPosition());
      }
      mFrame->Show();
      mFrame->Raise();
      return;
   }

   // This is the first use, so create the frame
   Destroy_ptr<wxFrame> frame
      { safenew wxFrame(NULL, wxID_ANY, _("Audacity Log")) };
   frame->SetName(frame->GetTitle());
   frame->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

   // loads either the XPM or the windows resource, depending on the platform
   {
#if !defined(__WXMAC__) && !defined(__WXX11__)
#if defined(__WXMSW__)
      wxIcon ic{wxICON(AudacityLogo)};
#elif defined(__WXGTK__)
      wxIcon ic{wxICON(AudacityLogoAlpha)};
#else
      wxIcon ic{};
      ic.CopyFromBitmap(theTheme.Bitmap(bmpAudacityLogo48x48));
#endif
      frame->SetIcon(ic);
#endif
   }

   // Log text
   ShuttleGui S(frame.get(), eIsCreating);

   S.SetStyle(wxNO_BORDER | wxTAB_TRAVERSAL);
   S.Prop(true).StartPanel();
   {
      S.StartVerticalLay(true);
      {
         S.SetStyle(wxTE_MULTILINE | wxHSCROLL | wxTE_READONLY);
         mText = S.AddTextWindow(mBuffer);

         S.AddSpace(0, 5);
         S.StartHorizontalLay(wxALIGN_CENTER, 0);
         {
            S.AddSpace(10, 0);
            S.Id(LoggerID_Save).AddButton(_("&Save..."));
            S.Id(LoggerID_Clear).AddButton(_("Cl&ear"));
            S.Id(LoggerID_Close).AddButton(_("&Close"));
            S.AddSpace(10, 0);
         }
         S.EndHorizontalLay();
         S.AddSpace(0, 3);
      }
      S.EndVerticalLay();
   }
   S.EndPanel();

   // Give a place for the menu help text to go
   // frame->CreateStatusBar();

   frame->Layout();

   // Hook into the frame events
   frame->Bind(wxEVT_CLOSE_WINDOW,
                  wxCloseEventHandler(AudacityLogger::OnCloseWindow),
                  this);

   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &AudacityLogger::OnSave,
                  this, LoggerID_Save);
   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &AudacityLogger::OnClear,
                  this, LoggerID_Clear);
   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &AudacityLogger::OnClose,
                  this, LoggerID_Close);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &AudacityLogger::OnSave,
                  this, LoggerID_Save);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &AudacityLogger::OnClear,
                  this, LoggerID_Clear);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &AudacityLogger::OnClose,
                  this, LoggerID_Close);

   mFrame = std::move( frame );

   mFrame->Show();

   Flush();
}

#if defined(EXPERIMENTAL_CRASH_REPORT)
wxString AudacityLogger::GetLog()
{
   return mBuffer;
}
#endif

void AudacityLogger::OnCloseWindow(wxCloseEvent & WXUNUSED(e))
{
#if defined(__WXMAC__)
   // On the Mac, destroy the window rather than hiding it since the
   // log menu will override the root windows menu if there is no
   // project window open.
   mFrame.reset();
#else
   Show(false);
#endif
}

void AudacityLogger::OnClose(wxCommandEvent & WXUNUSED(e))
{
   wxCloseEvent dummy;
   OnCloseWindow(dummy);
}

void AudacityLogger::OnClear(wxCommandEvent & WXUNUSED(e))
{
   mBuffer = wxEmptyString;
   DoLogText(wxT("Log Cleared."));
}

void AudacityLogger::OnSave(wxCommandEvent & WXUNUSED(e))
{
   wxString fName = _("log.txt");

   fName = FileNames::SelectFile(FileNames::Operation::Export,
                        _("Save log to:"),
                        wxEmptyString,
                        fName,
                        wxT("txt"),
                        wxT("*.txt"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxRESIZE_BORDER,
                        mFrame.get());

   if (fName.empty()) {
      return;
   }

   if (!mText->SaveFile(fName)) {
      AudacityMessageBox(
         wxString::Format( _("Couldn't save log to file: %s"), fName ),
         _("Warning"),
         wxICON_EXCLAMATION,
         mFrame.get());
      return;
   }
}

