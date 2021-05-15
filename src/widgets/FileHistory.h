/**********************************************************************

  Audacity: A Digital Audio Editor

  FileHistory.h

  Leland Lucius

**********************************************************************/

#ifndef __AUDACITY_WIDGETS_FILEHISTORY__
#define __AUDACITY_WIDGETS_FILEHISTORY__

#include <vector>
#include <algorithm>
#include <wx/defs.h>
#include <wx/weakref.h> // member variable

#include "audacity/Types.h"
#include "../MemoryX.h"

class wxConfigBase;
class wxMenu;

class AUDACITY_DLL_API FileHistory
{
 public:
   FileHistory(size_t maxfiles = 12, wxWindowID idbase = wxID_FILE);
   virtual ~FileHistory();

   void AddFileToHistory(const FilePath & file, bool update = true);
   void RemoveFileFromHistory(size_t i, bool update = true);
   void Clear();
   void UseMenu(wxMenu *menu);
   void Load(wxConfigBase& config, const wxString & group);
   void Save(wxConfigBase& config, const wxString & group);

   void AddFilesToMenu();
   void AddFilesToMenu(wxMenu *menu);

   size_t GetCount();
   const FilePath &GetHistoryFile(size_t i) const;

 private:
   void Compress();

   size_t mMaxFiles;
   wxWindowID mIDBase;

   std::vector< wxWeakRef< wxMenu > > mMenus;
   FilePaths mHistory;

};

#endif
