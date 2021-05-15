/**********************************************************************

  Audacity: A Digital Audio Editor

  ProjectsPrefs.cpp

  Joshua Haberman
  Dominic Mazzoni
  James Crook

*******************************************************************//**

\class ProjectsPrefs
\brief A PrefsPanel used to select options related to Audacity Project
handling.

*//*******************************************************************/

#include "../Audacity.h"
#include "ProjectsPrefs.h"

#include <wx/defs.h>
#include <wx/textctrl.h>

#include "../Prefs.h"
#include "../ShuttleGui.h"

#include "../Internat.h"

////////////////////////////////////////////////////////////////////////////////

ProjectsPrefs::ProjectsPrefs(wxWindow * parent, wxWindowID winid)
:   PrefsPanel(parent, winid,
   /* i18n-hint: (noun) i.e Audacity projects. */
               _("Projects"))
{
   Populate();
}

ProjectsPrefs::~ProjectsPrefs()
{
}

ComponentInterfaceSymbol ProjectsPrefs::GetSymbol()
{
   return PROJECTS_PREFS_PLUGIN_SYMBOL;
}

wxString ProjectsPrefs::GetDescription()
{
   return _("Preferences for Projects");
}

wxString ProjectsPrefs::HelpPageName()
{
   return "Projects_Preferences";
}

/// Creates the dialog and its contents.
void ProjectsPrefs::Populate()
{
   //------------------------- Main section --------------------
   // Now construct the GUI itself.
   // Use 'eIsCreatingFromPrefs' so that the GUI is
   // initialised with values from gPrefs.
   ShuttleGui S(this, eIsCreatingFromPrefs);
   PopulateOrExchange(S);
   // ----------------------- End of main section --------------
}

void ProjectsPrefs::PopulateOrExchange(ShuttleGui & S)
{
   S.SetBorder(2);   
   S.StartScroller();
#ifndef EXPERIMENTAL_DA 
// DA always copies.  Using a reference is dangerous.  
   S.StartStatic(_("When saving a project that depends on other audio files"));
   {
      S.StartRadioButtonGroup(wxT("/FileFormats/SaveProjectWithDependencies"), wxT("ask"));
      {
         S.TieRadioButton(_("&Copy audio into project"),
                          wxT("copy"));
         S.TieRadioButton(_("Do &not copy"),
                          wxT("never"));
         S.TieRadioButton(_("As&k"),
                          wxT("ask"));
      }
      S.EndRadioButtonGroup();
   }
   S.EndStatic();
   S.EndScroller();
#endif   

}

bool ProjectsPrefs::Commit()
{
   ShuttleGui S(this, eIsSavingToPrefs);
   PopulateOrExchange(S);

   return true;
}

PrefsPanel *ProjectsPrefsFactory::operator () (wxWindow *parent, wxWindowID winid)
{
   wxASSERT(parent); // to justify safenew
   return safenew ProjectsPrefs(parent, winid);
}
