/**********************************************************************

  Audacity: A Digital Audio Editor

  ErrorDialog.h

  Jimmy Johnson
  James Crook

**********************************************************************/

#ifndef __AUDACITY_ERRORDIALOG__
#define __AUDACITY_ERRORDIALOG__

#include "../Audacity.h"

#include <wx/defs.h>
#include <wx/msgdlg.h> // to inherit
#include "wxPanelWrapper.h" // to inherit

class AudacityProject;

class ErrorDialog /* not final */ : public wxDialogWrapper
{
public:
   // constructors and destructors
   ErrorDialog(wxWindow *parent,
      const wxString & dlogTitle,
      const wxString & message,
      const wxString & helpPage,
      const bool Close = true, const bool modal = true);

   virtual ~ErrorDialog(){}

private:
   wxString dhelpPage;
   bool dClose;
   bool dModal;

   void OnOk( wxCommandEvent &event );
   void OnHelp( wxCommandEvent &event );
   DECLARE_EVENT_TABLE()
};

/// Displays an error dialog with a button that offers help
void ShowErrorDialog(wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &message,
                     const wxString &helpPage,
                     bool Close = true);

/// Displays a modeless error dialog with a button that offers help
void ShowModelessErrorDialog(wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &message,
                     const wxString &helpPage,
                     bool Close = true);

/// Displays a custom modeless error dialog for aliased file errors
void ShowAliasMissingDialog(AudacityProject *parent,
                     const wxString &dlogTitle,
                     const wxString &message,
                     const wxString &helpPage,
                     const bool Close = true);

extern wxString AudacityMessageBoxCaptionStr();

// Do not use wxMessageBox!!  Its default window title does not translate!
inline int AudacityMessageBox(const wxString& message,
   const wxString& caption = AudacityMessageBoxCaptionStr(),
   long style = wxOK | wxCENTRE,
   wxWindow *parent = NULL,
   int x = wxDefaultCoord, int y = wxDefaultCoord)
{
   return ::wxMessageBox(message, caption, style, parent, x, y);
}


#include <wx/textdlg.h> // to inherit

/**************************************************************************//**
\class AudacityTextEntryDialog
\brief Wrap wxTextEntryDialog so that caption IS translatable.
********************************************************************************/
class AudacityTextEntryDialog : public wxTabTraversalWrapper< wxTextEntryDialog >
{
public:
    AudacityTextEntryDialog(
         wxWindow *parent,
         const wxString& message,
         const wxString& caption, // don't use = wxGetTextFromUserPromptStr,
         const wxString& value = {},
         long style = wxTextEntryDialogStyle,
         const wxPoint& pos = wxDefaultPosition)
   : wxTabTraversalWrapper< wxTextEntryDialog>
      ( parent, message, caption, value, style, pos )
   {}
   
   void SetInsertionPointEnd();
   bool Show(bool show = true) override;

private:
   bool mSetInsertionPointEnd{};
};

/**************************************************************************//**

\brief Wrap wxMessageDialog so that caption IS translatable.
********************************************************************************/
class AudacityMessageDialog : public wxTabTraversalWrapper< wxMessageDialog >
{
public:
    AudacityMessageDialog(
         wxWindow *parent,
         const wxString& message,
         const wxString& caption, // don't use = wxMessageBoxCaptionStr,
         long style = wxOK|wxCENTRE,
         const wxPoint& pos = wxDefaultPosition)
   : wxTabTraversalWrapper< wxMessageDialog>
      ( parent, message, caption, style, pos )
   {}
};

#endif // __AUDACITY_ERRORDIALOG__
