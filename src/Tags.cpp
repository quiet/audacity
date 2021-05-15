/**********************************************************************

  Audacity: A Digital Audio Editor

  Tags.cpp

  Dominic Mazzoni

*******************************************************************//**

\class Tags
\brief ID3 Tags (for MP3)

  This class started as an ID3 tag

  This class holds a few informational tags, such as Title, Author,
  etc. that can be associated with a project or other audio file.
  It is modeled after the ID3 format for MP3 files, and it can
  both import and export ID3 tags from/to MP2, MP3, and AIFF files.

  It can present the user with a dialog for editing this information.

  Use of this functionality requires that libid3tag be compiled in
  with Audacity.

*//****************************************************************//**

\class TagsEditor
\brief Derived from ExpandingToolBar, this dialog allows editing of Tags.

*//*******************************************************************/

#include "Audacity.h"
#include "Tags.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#include <wx/setup.h> // for wxUSE_* macros

#ifndef WX_PRECOMP
// Include your minimal set of headers here, or wx.h
#include <wx/window.h>
#endif

#include "FileNames.h"
#include "Internat.h"
#include "Prefs.h"
#include "ShuttleGui.h"
#include "TranslatableStringArray.h"
#include "widgets/Grid.h"
#include "widgets/ErrorDialog.h"
#include "widgets/HelpSystem.h"
#include "xml/XMLFileReader.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/radiobut.h>
#include <wx/scrolbar.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/textfile.h>
#include <wx/combobox.h>
#include <wx/display.h>

static const wxChar *DefaultGenres[] =
{
   wxT("Blues"),
   wxT("Classic Rock"),
   wxT("Country"),
   wxT("Dance"),
   wxT("Disco"),
   wxT("Funk"),
   wxT("Grunge"),
   wxT("Hip-Hop"),
   wxT("Jazz"),
   wxT("Metal"),
   wxT("New Age"),
   wxT("Oldies"),
   wxT("Other"),
   wxT("Pop"),
   wxT("R&B"),
   wxT("Rap"),
   wxT("Reggae"),
   wxT("Rock"),
   wxT("Techno"),
   wxT("Industrial"),
   wxT("Alternative"),
   wxT("Ska"),
   wxT("Death Metal"),
   wxT("Pranks"),
   wxT("Soundtrack"),
   wxT("Euro-Techno"),
   wxT("Ambient"),
   wxT("Trip-Hop"),
   wxT("Vocal"),
   wxT("Jazz+Funk"),
   wxT("Fusion"),
   wxT("Trance"),
   wxT("Classical"),
   wxT("Instrumental"),
   wxT("Acid"),
   wxT("House"),
   wxT("Game"),
   wxT("Sound Clip"),
   wxT("Gospel"),
   wxT("Noise"),
   wxT("Alt. Rock"),
   wxT("Bass"),
   wxT("Soul"),
   wxT("Punk"),
   wxT("Space"),
   wxT("Meditative"),
   wxT("Instrumental Pop"),
   wxT("Instrumental Rock"),
   wxT("Ethnic"),
   wxT("Gothic"),
   wxT("Darkwave"),
   wxT("Techno-Industrial"),
   wxT("Electronic"),
   wxT("Pop-Folk"),
   wxT("Eurodance"),
   wxT("Dream"),
   wxT("Southern Rock"),
   wxT("Comedy"),
   wxT("Cult"),
   wxT("Gangsta Rap"),
   wxT("Top 40"),
   wxT("Christian Rap"),
   wxT("Pop/Funk"),
   wxT("Jungle"),
   wxT("Native American"),
   wxT("Cabaret"),
   wxT("New Wave"),
   wxT("Psychedelic"),
   wxT("Rave"),
   wxT("Showtunes"),
   wxT("Trailer"),
   wxT("Lo-Fi"),
   wxT("Tribal"),
   wxT("Acid Punk"),
   wxT("Acid Jazz"),
   wxT("Polka"),
   wxT("Retro"),
   wxT("Musical"),
   wxT("Rock & Roll"),
   wxT("Hard Rock"),
   wxT("Folk"),
   wxT("Folk/Rock"),
   wxT("National Folk"),
   wxT("Swing"),
   wxT("Fast-Fusion"),
   wxT("Bebob"),
   wxT("Latin"),
   wxT("Revival"),
   wxT("Celtic"),
   wxT("Bluegrass"),
   wxT("Avantgarde"),
   wxT("Gothic Rock"),
   wxT("Progressive Rock"),
   wxT("Psychedelic Rock"),
   wxT("Symphonic Rock"),
   wxT("Slow Rock"),
   wxT("Big Band"),
   wxT("Chorus"),
   wxT("Easy Listening"),
   wxT("Acoustic"),
   wxT("Humour"),
   wxT("Speech"),
   wxT("Chanson"),
   wxT("Opera"),
   wxT("Chamber Music"),
   wxT("Sonata"),
   wxT("Symphony"),
   wxT("Booty Bass"),
   wxT("Primus"),
   wxT("Porn Groove"),
   wxT("Satire"),
   wxT("Slow Jam"),
   wxT("Club"),
   wxT("Tango"),
   wxT("Samba"),
   wxT("Folklore"),
   wxT("Ballad"),
   wxT("Power Ballad"),
   wxT("Rhythmic Soul"),
   wxT("Freestyle"),
   wxT("Duet"),
   wxT("Punk Rock"),
   wxT("Drum Solo"),
   wxT("A Cappella"),
   wxT("Euro-House"),
   wxT("Dance Hall"),
   wxT("Goa"),
   wxT("Drum & Bass"),
   wxT("Club-House"),
   wxT("Hardcore"),
   wxT("Terror"),
   wxT("Indie"),
   wxT("BritPop"),

   // Standard name is offensive (see "http://www.audacityteam.org/forum/viewtopic.php?f=11&t=3924").
   wxT("Offensive"), // wxT("Negerpunk"),

   wxT("Polsk Punk"),
   wxT("Beat"),
   wxT("Christian Gangsta Rap"),
   wxT("Heavy Metal"),
   wxT("Black Metal"),
   wxT("Crossover"),
   wxT("Contemporary Christian"),
   wxT("Christian Rock"),
   wxT("Merengue"),
   wxT("Salsa"),
   wxT("Thrash Metal"),
   wxT("Anime"),
   wxT("JPop"),
   wxT("Synthpop")
};

Tags::Tags()
{
   mEditTitle = true;
   mEditTrackNumber = true;

   LoadDefaults();
   LoadGenres();
}

Tags::~Tags()
{
}

std::shared_ptr<Tags> Tags::Duplicate() const
{
   return std::make_shared<Tags>(*this);
}

Tags & Tags::operator=(const Tags & src)
{
   mEditTitle = src.mEditTitle;
   mEditTrackNumber = src.mEditTrackNumber;

   mXref.clear();
   mXref = src.mXref;
   mMap.clear();
   mMap = src.mMap;

   mGenres.clear();
   mGenres = src.mGenres;

   return *this;
}

void Tags::LoadDefaults()
{
   wxString path;
   wxString name;
   wxString value;
   long ndx;
   bool cont;

   // Set the parent group
   path = gPrefs->GetPath();
   gPrefs->SetPath(wxT("/Tags"));

   // Process all entries in the group
   cont = gPrefs->GetFirstEntry(name, ndx);
   while (cont) {
      gPrefs->Read(name, &value, wxT(""));

      if (name == wxT("ID3V2")) {
         // LLL:  This is obsolute, but it must be handled and ignored.
      }
      else {
         SetTag(name, value);
      }

      cont = gPrefs->GetNextEntry(name, ndx);
   }

   // Restore original group
   gPrefs->SetPath(path);
}

bool Tags::IsEmpty()
{
   // At least one of these should be filled in, otherwise
   // it's assumed that the tags have not been set...
   if (HasTag(TAG_TITLE) || HasTag(TAG_ARTIST) || HasTag(TAG_ALBUM)) {
      return false;
   }

   return true;
}

void Tags::Clear()
{
   mXref.clear();
   mMap.clear();
}

namespace {
   bool EqualMaps(const TagMap &map1, const TagMap &map2)
   {
      // Maps are unordered, hash maps; can't just iterate in tandem and
      // compare.
      if (map1.size() != map2.size())
         return false;

      for (const auto &pair : map2) {
         auto iter = map1.find(pair.first);
         if (iter == map1.end() || iter->second != pair.second)
            return false;
      }

      return true;
   }
}

bool operator== (const Tags &lhs, const Tags &rhs)
{
   if (!EqualMaps(lhs.mXref, rhs.mXref))
      return false;

   if (!EqualMaps(lhs.mMap, rhs.mMap))
      return false;

   return
      lhs.mGenres == rhs.mGenres
      &&
      lhs.mEditTitle == rhs.mEditTitle
      &&
      lhs.mEditTrackNumber == rhs.mEditTrackNumber
   ;
}

void Tags::AllowEditTitle(bool editTitle)
{
   mEditTitle = editTitle;
}

void Tags::AllowEditTrackNumber(bool editTrackNumber)
{
   mEditTrackNumber = editTrackNumber;
}

int Tags::GetNumUserGenres()
{
   return mGenres.size();
}

void Tags::LoadDefaultGenres()
{
   mGenres.clear();
   for (size_t i = 0; i < WXSIZEOF(DefaultGenres); i++) {
      mGenres.push_back(DefaultGenres[i]);
   }
}

void Tags::LoadGenres()
{
   wxFileName fn(FileNames::DataDir(), wxT("genres.txt"));
   wxTextFile tf(fn.GetFullPath());

   if (!tf.Exists() || !tf.Open()) {
      LoadDefaultGenres();
      return;
   }

   mGenres.clear();

   int cnt = tf.GetLineCount();
   for (int i = 0; i < cnt; i++) {
      mGenres.push_back(tf.GetLine(i));
   }
}

wxString Tags::GetUserGenre(int i)
{
   if (i >= 0 && i < GetNumUserGenres()) {
      return mGenres[i];
   }

   return wxT("");
}

wxString Tags::GetGenre(int i)
{
   int cnt = WXSIZEOF(DefaultGenres);

   if (i >= 0 && i < cnt) {
      return DefaultGenres[i];
   }

   return wxT("");
}

int Tags::GetGenre(const wxString & name)
{
   int cnt = WXSIZEOF(DefaultGenres);

   for (int i = 0; i < cnt; i++) {
      if (name.CmpNoCase(DefaultGenres[i])) {
         return i;
      }
   }

   return 255;
}

bool Tags::HasTag(const wxString & name) const
{
   wxString key = name;
   key.UpperCase();

   auto iter = mXref.find(key);
   return (iter != mXref.end());
}

wxString Tags::GetTag(const wxString & name) const
{
   wxString key = name;
   key.UpperCase();

   auto iter = mXref.find(key);

   if (iter == mXref.end()) {
      return wxEmptyString;
   }

   auto iter2 = mMap.find(iter->second);
   if (iter2 == mMap.end()) {
      wxASSERT(false);
      return wxEmptyString;
   }
   else
      return iter2->second;
}

Tags::Iterators Tags::GetRange() const
{
   return { mMap.begin(), mMap.end() };
}

void Tags::SetTag(const wxString & name, const wxString & value)
{
   // We don't like empty names
   if (name.empty()) {
      return;
   }

   // Tag name must be ascii
   if (!name.IsAscii()) {
      wxLogError("Tag rejected (Non-ascii character in name)");
      return;
   }

   // All keys are uppercase
   wxString key = name;
   key.UpperCase();

   // Look it up
   TagMap::iterator iter = mXref.find(key);

   if (value.empty()) {
      // Erase the tag
      if (iter == mXref.end())
         // nothing to do
         ;
      else {
         mMap.erase(iter->second);
         mXref.erase(iter);
      }
   }
   else {
      if (iter == mXref.end()) {
         // Didn't find the tag

         // Add a NEW tag
         mXref[key] = name;
         mMap[name] = value;
      }
      else if (iter->second != name) {
         // Watch out for case differences!
         mMap[name] = value;
         mMap.erase(iter->second);
         iter->second = name;
      }
      else {
         // Update the value
         mMap[iter->second] = value;
      }
   }
}

void Tags::SetTag(const wxString & name, const int & value)
{
   SetTag(name, wxString::Format(wxT("%d"), value));
}

bool Tags::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if (wxStrcmp(tag, wxT("tags")) == 0) {
      return true;
   }

   if (wxStrcmp(tag, wxT("tag")) == 0) {
      wxString n, v;

      while (*attrs) {
         wxString attr = *attrs++;
         if (attr.empty())
            break;
         wxString value = *attrs++;

         if (!XMLValueChecker::IsGoodString(attr) ||
             !XMLValueChecker::IsGoodString(value)) {
            break;
         }

         if (attr == wxT("name")) {
            n = value;
         }
         else if (attr == wxT("value")) {
            v = value;
         }
      }

      if (n == wxT("id3v2")) {
         // LLL:  This is obsolete, but it must be handled and ignored.
      }
      else {
         SetTag(n, v);
      }

      return true;
   }

   return false;
}

XMLTagHandler *Tags::HandleXMLChild(const wxChar *tag)
{
   if (wxStrcmp(tag, wxT("tags")) == 0) {
      return this;
   }

   if (wxStrcmp(tag, wxT("tag")) == 0) {
      return this;
   }

   return NULL;
}

void Tags::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   xmlFile.StartTag(wxT("tags"));

   for (const auto &pair : GetRange()) {
      const auto &n = pair.first;
      const auto &v = pair.second;
      xmlFile.StartTag(wxT("tag"));
      xmlFile.WriteAttr(wxT("name"), n);
      xmlFile.WriteAttr(wxT("value"), v);
      xmlFile.EndTag(wxT("tag"));
   }

   xmlFile.EndTag(wxT("tags"));
}

bool Tags::ShowEditDialog(wxWindow *parent, const wxString &title, bool force)
{
   if (force) {
      TagsEditor dlg(parent, title, this, mEditTitle, mEditTrackNumber);

      return dlg.ShowModal() == wxID_OK;
   }

   return true;
}
//
// ComboEditor - Wrapper to prevent unwanted background erasure
//

class ComboEditor final : public wxGridCellChoiceEditor
{
public:
   ComboEditor(const wxArrayString& choices, bool allowOthers = false)
   :  wxGridCellChoiceEditor(choices, allowOthers)
   ,  m_choices{ choices }
   ,  m_allowOthers{ allowOthers }
   {
   }

   void PaintBackground(wxDC&, const wxRect& WXUNUSED(rectCell), const wxGridCellAttr & WXUNUSED(attr)) override
   {
      // Ignore it (a must on the Mac as the erasure causes problems.)
   }

   void SetParameters(const wxString& params) override
   {
      wxGridCellChoiceEditor::SetParameters(params);

      // Refresh the wxComboBox with NEW values
      if (Combo()) {
         Combo()->Clear();
         Combo()->Append(m_choices);
      }
   }

   void SetSize(const wxRect& rectOrig) override
   {
      wxRect rect(rectOrig);
      wxRect r = Combo()->GetRect();

      // Center the combo box in or over the cell
      rect.y -= (r.GetHeight() - rect.GetHeight()) / 2;
      rect.height = r.GetHeight();

      wxGridCellChoiceEditor::SetSize(rect);
   }

   // Fix for Bug 1389
   // July 2016: ANSWER-ME: Does this need reporting upstream to wxWidgets?
   virtual void StartingKey(wxKeyEvent& event) override
   {
       // Lifted from wxGridCellTextEditor and adapted to combo.

       // [Below is comment from wxWidgets code]
       // Since this is now happening in the EVT_CHAR event EmulateKeyPress is no
       // longer an appropriate way to get the character into the text control.
       // Do it ourselves instead.  We know that if we get this far that we have
       // a valid character, so not a whole lot of testing needs to be done.

       //The only difference to wxGridCellTextEditor.
       //wxTextCtrl* tc = (wxTextCtrl *)m_control;
       wxComboBox * tc = Combo();
       int ch;

       bool isPrintable;

   #if wxUSE_UNICODE
       ch = event.GetUnicodeKey();
       if ( ch != WXK_NONE )
           isPrintable = true;
       else
   #endif // wxUSE_UNICODE
       {
           ch = event.GetKeyCode();
           isPrintable = ch >= WXK_SPACE && ch < WXK_START;
       }

       switch (ch)
       {
           case WXK_DELETE:
               // Delete the initial character when starting to edit with DELETE.
               tc->Remove(0, 1);
               break;

           case WXK_BACK:
               // Delete the last character when starting to edit with BACKSPACE.
               {
                   const long pos = tc->GetLastPosition();
                   tc->Remove(pos - 1, pos);
               }
               break;

           default:
               if ( isPrintable )
                   tc->WriteText(static_cast<wxChar>(ch));
               break;
       }
   }

   // Clone is required by wxwidgets; implemented via copy constructor
   wxGridCellEditor *Clone() const override
   {
      return safenew ComboEditor{ m_choices, m_allowOthers };
   }

private:
   wxArrayString   m_choices;
   bool            m_allowOthers;
};

//
// Editor
//

#define LABEL_ARTIST    XO("Artist Name")
#define LABEL_TITLE     XO("Track Title")
#define LABEL_ALBUM     XO("Album Title")
#define LABEL_TRACK     XO("Track Number")
#define LABEL_YEAR      XO("Year")
#define LABEL_GENRE     XO("Genre")
#define LABEL_COMMENTS  XO("Comments")

static wxArrayString names()
{
   static wxString theNames[] =
   {
      LABEL_ARTIST,
      LABEL_TITLE,
      LABEL_ALBUM,
      LABEL_TRACK,
      LABEL_YEAR,
      LABEL_GENRE,
      LABEL_COMMENTS
   };

   class NamesArray final : public TranslatableStringArray
   {
      void Populate() override
      {
         for (auto &name : theNames)
            mContents.push_back( wxGetTranslation( name ) );
      }
   };

   static NamesArray theArray;

   // Yes, return array by value
   return theArray.Get();
}

static const struct
{
   wxString label;
   wxString name;
}
labelmap[] =
{
   {  LABEL_ARTIST,     TAG_ARTIST     },
   {  LABEL_TITLE,      TAG_TITLE      },
   {  LABEL_ALBUM,      TAG_ALBUM      },
   {  LABEL_TRACK,      TAG_TRACK      },
   {  LABEL_YEAR,       TAG_YEAR       },
   {  LABEL_GENRE,      TAG_GENRE      },
   {  LABEL_COMMENTS,   TAG_COMMENTS   }
};

#define STATICCNT WXSIZEOF(labelmap)

enum {
   ClearID = 10000,
   EditID,
   ResetID,
   LoadID,
   SaveID,
   SaveDefaultsID,
   AddID,
   RemoveID,
   DontShowID
};

BEGIN_EVENT_TABLE(TagsEditor, wxDialogWrapper)
   EVT_GRID_CELL_CHANGED(TagsEditor::OnChange)
   EVT_BUTTON(EditID, TagsEditor::OnEdit)
   EVT_BUTTON(ResetID, TagsEditor::OnReset)
   EVT_BUTTON(ClearID, TagsEditor::OnClear)
   EVT_BUTTON(LoadID, TagsEditor::OnLoad)
   EVT_BUTTON(SaveID, TagsEditor::OnSave)
   EVT_BUTTON(SaveDefaultsID, TagsEditor::OnSaveDefaults)
   EVT_BUTTON(AddID, TagsEditor::OnAdd)
   EVT_BUTTON(RemoveID, TagsEditor::OnRemove)
   EVT_BUTTON(wxID_HELP, TagsEditor::OnHelp)
   EVT_BUTTON(wxID_CANCEL, TagsEditor::OnCancel)
   EVT_BUTTON(wxID_OK, TagsEditor::OnOk)
   EVT_CHECKBOX( DontShowID, TagsEditor::OnDontShow )
   EVT_KEY_DOWN(TagsEditor::OnKeyDown)
END_EVENT_TABLE()

TagsEditor::TagsEditor(wxWindow * parent,
                       const wxString &title,
                       Tags * tags,
                       bool editTitle,
                       bool editTrack)
:  wxDialogWrapper(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
   mTags(tags),
   mEditTitle(editTitle),
   mEditTrack(editTrack)
{
   SetName(GetTitle());

   mGrid = NULL;

   // Make a local copy of the passed in tags
   mLocal = *mTags;

   // Build, size, and position the dialog
   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);

   TransferDataToWindow();

   Layout();
   Fit();
   Center();
   SetSizeHints(GetSize());

   // Restore the original tags because TransferDataToWindow() will be called again
   mLocal.Clear();
   mLocal = *mTags;

   // Override size and position with last saved
   wxRect r = GetRect();
   gPrefs->Read(wxT("/TagsEditor/x"), &r.x, r.x);
   gPrefs->Read(wxT("/TagsEditor/y"), &r.y, r.y);
   gPrefs->Read(wxT("/TagsEditor/width"), &r.width, r.width);
   gPrefs->Read(wxT("/TagsEditor/height"), &r.height, r.height);
   //On multi-monitor systems, there's a chance the last saved window position is
   //on a monitor that has been removed or is unavailable.
   if (IsWindowRectValid(&r))
      Move(r.GetPosition());

   SetSize(r.GetSize());
   Layout();

   // Resize value column width based on width of columns and the vertical scrollbar
   wxScrollBar sb(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
   r = mGrid->GetClientRect();
   r.width -= mGrid->GetColSize(0);
   r.width -= sb.GetSize().GetWidth();
   r.width -= 10;
   r.width -= r.x;
   mGrid->SetColSize(1, r.width);
   //Bug 2038
   mGrid->SetFocus();

   // Load the genres
   PopulateGenres();
}

TagsEditor::~TagsEditor()
{
   // This DELETE is not needed because wxWidgets owns the grid.
// DELETE mGrid;

// TODO:  Need to figure out if these should be deleted.  Looks like the wxGrid
//        code takes ownership and uses reference counting, but there's been
//        cases where they show up as memory leaks.
//  PRL: Fixed the leaks, see commit c87eb0804bc5f40659b133cab6e2ade061959645
//   DELETE mStringRenderer;
//   DELETE mComboEditor;
}

void TagsEditor::PopulateOrExchange(ShuttleGui & S)
{
   bool bShow;
   gPrefs->Read(wxT("/AudioFiles/ShowId3Dialog"), &bShow, true );

   S.StartVerticalLay();
   {
      S.StartHorizontalLay(wxALIGN_LEFT, 0);
      {
         S.AddUnits(_("Use arrow keys (or ENTER key after editing) to navigate fields."));
      }
      S.EndHorizontalLay();

      if (mGrid == NULL) {
         mGrid = safenew Grid(S.GetParent(),
                          wxID_ANY,
                          wxDefaultPosition,
                          wxDefaultSize,
                          wxSUNKEN_BORDER);

         mGrid->RegisterDataType(wxT("Combo"),
            (mStringRenderer = safenew wxGridCellStringRenderer),
            (mComboEditor = safenew ComboEditor(wxArrayString(), true)));

         mGrid->SetColLabelSize(mGrid->GetDefaultRowSize());

         auto  cs = names();

         // Build the initial (empty) grid
         mGrid->CreateGrid(0, 2);
         mGrid->SetRowLabelSize(0);
         mGrid->SetDefaultCellAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
         mGrid->SetColLabelValue(0, _("Tag"));
         mGrid->SetColLabelValue(1, _("Value"));

         // Resize the name column and set default row height.
         wxComboBox tc(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, cs);
         mGrid->SetColSize(0, tc.GetSize().x);
         mGrid->SetColMinimalWidth(0, tc.GetSize().x);
      }
      S.Prop(1);
      S.AddWindow(mGrid, wxEXPAND | wxALL);

      S.StartMultiColumn(4, wxALIGN_CENTER);
      {
         S.Id(AddID).AddButton(_("&Add"));
         S.Id(RemoveID).AddButton(_("&Remove"));
         S.AddTitle( {} );
         S.Id(ClearID).AddButton(_("Cl&ear"));
      }
      S.EndMultiColumn();

      S.StartHorizontalLay(wxALIGN_CENTRE, 0);
      {
         S.StartStatic(_("Genres"));
         {
            S.StartMultiColumn(4, wxALIGN_CENTER);
            {
               S.Id(EditID).AddButton(_("E&dit..."));
               S.Id(ResetID).AddButton(_("Rese&t..."));
            }
            S.EndMultiColumn();
         }
         S.EndStatic();
         S.StartStatic(_("Template"));
         {
            S.StartMultiColumn(4, wxALIGN_CENTER);
            {
               S.Id(LoadID).AddButton(_("&Load..."));
               S.Id(SaveID).AddButton(_("&Save..."));
               S.AddTitle( {} );
               S.Id(SaveDefaultsID).AddButton(_("Set De&fault"));
            }
            S.EndMultiColumn();
         }
         S.EndStatic();
      }
      S.EndHorizontalLay();
      S.StartHorizontalLay(wxALIGN_LEFT, 0);
      {
         S.Id( DontShowID ).AddCheckBox( _("Don't show this when exporting audio"), !bShow );
      }
      S.EndHorizontalLay();
   }
   S.EndVerticalLay();

   S.AddStandardButtons(eOkButton | eCancelButton | eHelpButton);
}

void TagsEditor::OnDontShow( wxCommandEvent & Evt )
{
   bool bShow = !Evt.IsChecked();
   gPrefs->Write(wxT("/AudioFiles/ShowId3Dialog"), bShow );
   gPrefs->Flush();
}

void TagsEditor::OnHelp(wxCommandEvent& WXUNUSED(event))
{
   HelpSystem::ShowHelp(this, wxT("Metadata_Editor"), true);
}

bool TagsEditor::TransferDataFromWindow()
{
   int i, cnt = mGrid->GetNumberRows();

   if (mGrid->IsCellEditControlShown()) {
      mGrid->SaveEditControlValue();
      mGrid->HideCellEditControl();
   }

   mLocal.Clear();
   for (i = 0; i < cnt; i++) {
      // Get tag name from the grid

      wxString n = mGrid->GetCellValue(i, 0);
      wxString v = mGrid->GetCellValue(i, 1);

      if (n.empty()) {
         continue;
      }

      // Map special tag names back to internal keys
      if (n.CmpNoCase(wxGetTranslation(LABEL_ARTIST)) == 0) {
         n = TAG_ARTIST;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_TITLE)) == 0) {
         n = TAG_TITLE;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_ALBUM)) == 0) {
         n = TAG_ALBUM;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_TRACK)) == 0) {
         n = TAG_TRACK;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_YEAR)) == 0) {
         n = TAG_YEAR;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_GENRE)) == 0) {
         n = TAG_GENRE;
      }
      else if (n.CmpNoCase(wxGetTranslation(LABEL_COMMENTS)) == 0) {
         n = TAG_COMMENTS;
      }

      mLocal.SetTag(n, v);
   }

   return true;
}

bool TagsEditor::TransferDataToWindow()
{
   size_t i;
   TagMap popTagMap;

   // Disable redrawing until we're done
   mGrid->BeginBatch();

   // Delete all rows
   if (mGrid->GetNumberRows()) {
      mGrid->DeleteRows(0, mGrid->GetNumberRows());
   }

   // Populate the static rows
   for (i = 0; i < STATICCNT; i++) {
      mGrid->AppendRows();

      mGrid->SetReadOnly(i, 0);
      // The special tag name that's displayed and translated may not match
      // the key string used for internal lookup.
      mGrid->SetCellValue(i, 0, wxGetTranslation( labelmap[i].label ) );
      mGrid->SetCellValue(i, 1, mLocal.GetTag(labelmap[i].name));

      if (!mEditTitle &&
          mGrid->GetCellValue(i, 0).CmpNoCase(wxGetTranslation(LABEL_TITLE)) == 0) {
         mGrid->SetReadOnly(i, 1);
      }

      if (!mEditTrack &&
          mGrid->GetCellValue(i, 0).CmpNoCase(wxGetTranslation(LABEL_TRACK)) == 0) {
         mGrid->SetReadOnly(i, 1);
      }

      popTagMap[ labelmap[i].name ] = mGrid->GetCellValue(i, 1);
   }

   // Populate the rest
   for (const auto &pair : mLocal.GetRange()) {
      const auto &n = pair.first;
      const auto &v = pair.second;
      if (popTagMap.find(n) == popTagMap.end()) {
         mGrid->AppendRows();
         mGrid->SetCellValue(i, 0, n);
         mGrid->SetCellValue(i, 1, v);
         i++;
      }
   }

   // Add an extra one to help with initial sizing and to show it can be done
   mGrid->AppendRows(1);

   // We're done, so allow the grid to redraw
   mGrid->EndBatch();

   // Set the editors
   SetEditors();
   Layout();
   Fit();

   return true;
}

void TagsEditor::OnChange(wxGridEvent & event)
{
   static bool ischanging = false;

   // Prevent recursion
   if (ischanging) {
      return;
   }

   event.Skip();

   if (event.GetCol() != 0) {
      return;
   }

   // Do not permit duplication of any of the tags.
   // Tags differing only in case are nondistinct.
   auto row = event.GetRow();
   const wxString key0 = mGrid->GetCellValue(row, 0).Upper();
   auto nn = mGrid->GetNumberRows();
   for (decltype(nn) ii = 0; ii < nn; ++ii) {
      if (ii == row)
         continue;
      auto key = mGrid->GetCellValue(ii, 0).Upper();
      if (key0.CmpNoCase(key) == 0) {
         ischanging = true;
         wxBell();
         mGrid->SetGridCursor(ii, 0);
         event.Veto();
         ischanging = false;
         break;
      }
   }

   return;
}

void TagsEditor::OnEdit(wxCommandEvent & WXUNUSED(event))
{
   if (mGrid->IsCellEditControlShown()) {
      mGrid->SaveEditControlValue();
      mGrid->HideCellEditControl();
   }

   wxDialogWrapper dlg(this, wxID_ANY, _("Edit Genres"),
                wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
   dlg.SetName(dlg.GetTitle());
   wxTextCtrl *tc;

   ShuttleGui S(&dlg, eIsCreating);

   S.StartVerticalLay(true);
   {
      tc = S.AddTextWindow(wxT(""));
   }
   S.EndVerticalLay();

   S.AddStandardButtons();

   wxArrayString g;
   int cnt = mLocal.GetNumUserGenres();
   for (int i = 0; i < cnt; i++) {
      g.push_back(mLocal.GetUserGenre(i));
   }
   std::sort( g.begin(), g.end() );

   for (int i = 0; i < cnt; i++) {
      tc->AppendText(g[i] + wxT("\n"));
   }

   dlg.Center();
   if (dlg.ShowModal() == wxID_CANCEL) {
      return;
   }

   wxFileName fn(FileNames::DataDir(), wxT("genres.txt"));
   wxFile f(fn.GetFullPath(), wxFile::write);
   if (!f.IsOpened() || !f.Write(tc->GetValue())) {
      AudacityMessageBox(_("Unable to save genre file."), _("Reset Genres"));
      return;
   }

   mLocal.LoadGenres();

   PopulateGenres();
}

void TagsEditor::OnReset(wxCommandEvent & WXUNUSED(event))
{
   int id = AudacityMessageBox(_("Are you sure you want to reset the genre list to defaults?"),
                         _("Reset Genres"),
                         wxYES_NO);

   if (id == wxNO) {
      return;
   }
   mLocal.LoadDefaultGenres();

   wxFileName fn(FileNames::DataDir(), wxT("genres.txt"));
   wxTextFile tf(fn.GetFullPath());

   bool open = (tf.Exists() && tf.Open()) ||
               (!tf.Exists() && tf.Create());

   if (!open) {
      AudacityMessageBox(_("Unable to open genre file."), _("Reset Genres"));
      mLocal.LoadGenres();
      return;
   }

   tf.Clear();
   int cnt = mLocal.GetNumUserGenres();
   for (int i = 0; i < cnt; i++) {
      tf.AddLine(mLocal.GetUserGenre(i));
   }

   if (!tf.Write()) {
      AudacityMessageBox(_("Unable to save genre file."), _("Reset Genres"));
      mLocal.LoadGenres();
      return;
   }

   mLocal.LoadGenres();

   PopulateGenres();
}

void TagsEditor::OnClear(wxCommandEvent & WXUNUSED(event))
{
   mLocal.Clear();

   TransferDataToWindow();
}

void TagsEditor::OnLoad(wxCommandEvent & WXUNUSED(event))
{
   wxString fn;

   // Ask the user for the real name
   fn = FileNames::SelectFile(FileNames::Operation::_None,
                     _("Load Metadata As:"),
                     FileNames::DataDir(),
                     wxT("Tags.xml"),
                     wxT("xml"),
                     wxT("*.xml"),
                     wxFD_OPEN | wxRESIZE_BORDER,
                     this);

   // User canceled...
   if (fn.empty()) {
      return;
   }

   // Remember title and track in case they're read only
   wxString title = mLocal.GetTag(TAG_TITLE);
   wxString track = mLocal.GetTag(TAG_TRACK);

   // Clear current contents
   mLocal.Clear();

   // Load the metadata
   XMLFileReader reader;
   if (!reader.Parse(&mLocal, fn)) {
      // Inform user of load failure
      AudacityMessageBox(reader.GetErrorStr(),
                   _("Error Loading Metadata"),
                   wxOK | wxCENTRE,
                   this);
   }

   // Restore title
   if (!mEditTitle) {
      mLocal.SetTag(TAG_TITLE, title);
   }

   // Restore track
   if (!mEditTrack) {
      mLocal.SetTag(TAG_TRACK, track);
   }

   // Go fill up the window
   TransferDataToWindow();

   return;
}

void TagsEditor::OnSave(wxCommandEvent & WXUNUSED(event))
{
   wxString fn;

   // Refresh tags
   TransferDataFromWindow();

   // Ask the user for the real name
   fn = FileNames::SelectFile(FileNames::Operation::_None,
                     _("Save Metadata As:"),
                     FileNames::DataDir(),
                     wxT("Tags.xml"),
                     wxT("xml"),
                     wxT("*.xml"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxRESIZE_BORDER,
                     this);

   // User canceled...
   if (fn.empty()) {
      return;
   }

   GuardedCall( [&] {
      // Create/Open the file
      XMLFileWriter writer{ fn, _("Error Saving Tags File") };

      // Remember title and track in case they're read only
      wxString title = mLocal.GetTag(TAG_TITLE);
      wxString track = mLocal.GetTag(TAG_TRACK);

      // Clear title
      if (!mEditTitle) {
         mLocal.SetTag(TAG_TITLE, wxEmptyString);
      }

      // Clear track
      if (!mEditTrack) {
         mLocal.SetTag(TAG_TRACK, wxEmptyString);
      }

      auto cleanup = finally( [&] {
         // Restore title
         if (!mEditTitle) {
            mLocal.SetTag(TAG_TITLE, title);
         }

         // Restore track
         if (!mEditTrack) {
            mLocal.SetTag(TAG_TRACK, track);
         }
      } );

      // Write the metadata
      mLocal.WriteXML(writer);

      writer.Commit();
   } );
}

void TagsEditor::OnSaveDefaults(wxCommandEvent & WXUNUSED(event))
{
   // Refresh tags
   TransferDataFromWindow();

   // Remember title and track in case they're read only
   wxString title = mLocal.GetTag(TAG_TITLE);
   wxString track = mLocal.GetTag(TAG_TRACK);

   // Clear title
   if (!mEditTitle) {
      mLocal.SetTag(TAG_TITLE, wxEmptyString);
   }

   // Clear track
   if (!mEditTrack) {
      mLocal.SetTag(TAG_TRACK, wxEmptyString);
   }

   // Remove any previous defaults
   gPrefs->DeleteGroup(wxT("/Tags"));

   // Write out each tag
   for (const auto &pair : mLocal.GetRange()) {
      const auto &n = pair.first;
      const auto &v = pair.second;
      gPrefs->Write(wxT("/Tags/") + n, v);
   }
   gPrefs->Flush();

   // Restore title
   if (!mEditTitle) {
      mLocal.SetTag(TAG_TITLE, title);
   }

   // Restore track
   if (!mEditTrack) {
      mLocal.SetTag(TAG_TRACK, track);
   }
}

void TagsEditor::OnAdd(wxCommandEvent & WXUNUSED(event))
{
   mGrid->AppendRows();
}

void TagsEditor::OnRemove(wxCommandEvent & WXUNUSED(event))
{
   size_t row = mGrid->GetGridCursorRow();

   if (!mEditTitle &&
       mGrid->GetCellValue(row, 0).CmpNoCase(wxGetTranslation(LABEL_TITLE)) == 0) {
      return;
   }
   else if (!mEditTrack &&
            mGrid->GetCellValue(row, 0)
               .CmpNoCase(wxGetTranslation(LABEL_TRACK)) == 0) {
      return;
   }
   else if (row < STATICCNT) {
      mGrid->SetCellValue(row, 1, wxEmptyString);
   }
   else if (row >= STATICCNT) {
      mGrid->DeleteRows(row, 1);
   }
}

void TagsEditor::OnOk(wxCommandEvent & WXUNUSED(event))
{
   if (mGrid->IsCellEditControlShown()) {
      mGrid->SaveEditControlValue();
      mGrid->HideCellEditControl();
   }

   if (!Validate() || !TransferDataFromWindow()) {
      return;
   }

   *mTags = mLocal;

   wxRect r = GetRect();
   gPrefs->Write(wxT("/TagsEditor/x"), r.x);
   gPrefs->Write(wxT("/TagsEditor/y"), r.y);
   gPrefs->Write(wxT("/TagsEditor/width"), r.width);
   gPrefs->Write(wxT("/TagsEditor/height"), r.height);
   gPrefs->Flush();

   EndModal(wxID_OK);
}

void TagsEditor::OnCancel(wxCommandEvent & WXUNUSED(event))
{
   DoCancel(false);
}

void TagsEditor::DoCancel(bool escKey)
{
   if (mGrid->IsCellEditControlShown()) {
      auto editor = mGrid->GetCellEditor(mGrid->GetGridCursorRow(),
         mGrid->GetGridCursorCol());
      editor->Reset();
      // To avoid memory leak, don't forget DecRef()!
      editor->DecRef();
      mGrid->HideCellEditControl();
#if defined(__WXMSW__)
      return;
#endif
   }

   auto focus = wxWindow::FindFocus();
   if (escKey && focus == mGrid)
      return;

   EndModal(wxID_CANCEL);
}

void TagsEditor::OnKeyDown(wxKeyEvent &event)
{
   if (event.GetKeyCode() == WXK_ESCAPE)
      DoCancel(true);
   else
      event.Skip();
}

void TagsEditor::SetEditors()
{
   int cnt = mGrid->GetNumberRows();

   for (int i = 0; i < cnt; i++) {
      wxString label = mGrid->GetCellValue(i, 0);
      if (label.CmpNoCase(wxGetTranslation(LABEL_GENRE)) == 0) {
         // This use of GetDefaultEditorForType does not require DecRef.
         mGrid->SetCellEditor(i, 1, mGrid->GetDefaultEditorForType(wxT("Combo")));
      }
      else {
         mGrid->SetCellEditor(i, 1, NULL); //mGrid->GetDefaultEditor());
      }
   }
}

void TagsEditor::PopulateGenres()
{
   int cnt = mLocal.GetNumUserGenres();
   int i;
   wxString parm;
   wxArrayString g;

   for (i = 0; i < cnt; i++) {
      g.push_back(mLocal.GetUserGenre(i));
   }
   std::sort( g.begin(), g.end() );

   for (i = 0; i < cnt; i++) {
      parm = parm + (i == 0 ? wxT("") : wxT(",")) + g[i];
   }

   // Here was a memory leak!  wxWidgets docs for wxGrid::GetDefaultEditorForType() say:
   // "The caller must call DecRef() on the returned pointer."
   auto editor = mGrid->GetDefaultEditorForType(wxT("Combo"));
   editor->SetParameters(parm);
   editor->DecRef();
}

bool TagsEditor::IsWindowRectValid(const wxRect *windowRect) const
{
   wxDisplay display;
   wxPoint topLeft(windowRect->GetTopLeft().x, windowRect->GetTopLeft().y);
   wxPoint topRight(windowRect->GetTopRight().x, windowRect->GetTopRight().y);
   wxPoint bottomLeft(windowRect->GetBottomLeft().x, windowRect->GetBottomLeft().y);
   wxPoint bottomRight(windowRect->GetBottomRight().x, windowRect->GetBottomRight().y);
   display.GetFromPoint(topLeft);
   if (display.GetFromPoint(topLeft) == -1 &&
       display.GetFromPoint(topRight) == -1 &&
       display.GetFromPoint(bottomLeft) == -1 &&
       display.GetFromPoint(bottomRight) == -1) {
      return false;
   }

   return true;
}
