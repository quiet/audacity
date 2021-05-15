//
//  InconsistencyException.cpp
//  
//
//  Created by Paul Licameli on 11/27/16.
//
//

#include "Audacity.h"
#include "InconsistencyException.h"

#include "Internat.h"

InconsistencyException::~InconsistencyException()
{
}

wxString InconsistencyException::ErrorMessage() const
{
   // Shorten the path
   wxString path { file };
   auto sub = wxString{ wxFILE_SEP_PATH } + "src" + wxFILE_SEP_PATH;
   auto index = path.Find(sub);
   if (index != wxNOT_FOUND)
      path = path.Mid(index + sub.size());

#ifdef __func__
   return wxString::Format(
_("Internal error in %s at %s line %d.\nPlease inform the Audacity team at https://forum.audacityteam.org/."),
      func, path, line
   );
#else
   return wxString::Format(
_("Internal error at %s line %d.\nPlease inform the Audacity team at https://forum.audacityteam.org/."),
      path, line
   );
#endif
}
