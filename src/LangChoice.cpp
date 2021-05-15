/**********************************************************************

  Audacity: A Digital Audio Editor

  LangChoice.cpp

  Dominic Mazzoni

*******************************************************************//*!

\class LangChoiceDialog
\brief A dialog used (at start up) to present the user with a choice
of languages for Audacity.

*//*******************************************************************/


#include "Audacity.h"
#include "LangChoice.h"

#include <wx/defs.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Languages.h"
#include "ShuttleGui.h"
#include "widgets/wxPanelWrapper.h"
#include "widgets/ErrorDialog.h"
#include "Internat.h"

class LangChoiceDialog final : public wxDialogWrapper {
public:
   LangChoiceDialog(wxWindow * parent,
                    wxWindowID id,
                    const wxString & title);

   wxString GetLang() { return mLang; }

private:
   void OnOk(wxCommandEvent & event);

   wxChoice *mChoice;
   wxString mLang;

   int mNumLangs;
   wxArrayString mLangCodes;
   wxArrayStringEx mLangNames;

   DECLARE_EVENT_TABLE()
};

wxString ChooseLanguage(wxWindow *parent)
{
   wxString returnVal;

   /* i18n-hint: Title on a dialog indicating that this is the first
    * time Audacity has been run. */
   LangChoiceDialog dlog(parent, -1, _("Audacity First Run"));
   dlog.CentreOnParent();
   dlog.ShowModal();
   returnVal = dlog.GetLang();

   return returnVal;
}

BEGIN_EVENT_TABLE(LangChoiceDialog, wxDialogWrapper)
    EVT_BUTTON(wxID_OK, LangChoiceDialog::OnOk)
END_EVENT_TABLE()

LangChoiceDialog::LangChoiceDialog(wxWindow * parent,
                                   wxWindowID id,
                                   const wxString & title):
   wxDialogWrapper(parent, id, title)
{
   SetName(GetTitle());
   GetLanguages(mLangCodes, mLangNames);
   int lang =
      make_iterator_range( mLangCodes ).index( GetSystemLanguageCode() );

   ShuttleGui S(this, eIsCreating);

   S.StartVerticalLay(false);
   {
      S.StartHorizontalLay();
      {
         S.SetBorder(15);
         mChoice = S.AddChoice(_("Choose Language for Audacity to use:"),
                              mLangNames,
                              lang);
      }
      S.EndVerticalLay();

      S.SetBorder(0);
      S.AddStandardButtons(eOkButton);
   }
   S.EndVerticalLay();

   Fit();
}

void LangChoiceDialog::OnOk(wxCommandEvent & WXUNUSED(event))
{
   int ndx = mChoice->GetSelection();
   mLang = mLangCodes[ndx];

   wxString slang = GetSystemLanguageCode();
   int sndx = make_iterator_range( mLangCodes ).index( slang );
   wxString sname;

   if (sndx == wxNOT_FOUND) {
      const wxLanguageInfo *sinfo = wxLocale::FindLanguageInfo(slang);
      if (sinfo) {
         sname = sinfo->Description;
      }
   }
   else {
      sname = mLangNames[sndx];
   }

   if (mLang.Left(2) != slang.Left(2)) {
      wxString msg;
      /* i18n-hint: The %s's are replaced by translated and untranslated
       * versions of language names. */
      msg.Printf(_("The language you have chosen, %s (%s), is not the same as the system language, %s (%s)."),
                 mLangNames[ndx],
                 mLang,
                 sname,
                 slang);
      if (AudacityMessageBox(msg, _("Confirm"), wxYES_NO) == wxNO) {
         return;
      }
   }

   EndModal(true);
}
