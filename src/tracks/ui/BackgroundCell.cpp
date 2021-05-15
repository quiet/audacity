/**********************************************************************

Audacity: A Digital Audio Editor

BackgroundCell.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../Audacity.h"
#include "BackgroundCell.h"

#include "../../HitTestResult.h"
#include "../../Project.h"
#include "../../RefreshCode.h"
#include "../../Track.h"
#include "../../TrackPanelMouseEvent.h"
#include "../../UIHandle.h"

#include <wx/cursor.h>
#include <wx/event.h>

// Define this, just so the click to deselect can dispatch here
// This handle class, unlike most, doesn't associate with any particular cell.
class BackgroundHandle : public UIHandle
{
   BackgroundHandle(const BackgroundHandle&) = delete;
   BackgroundHandle &operator=(const BackgroundHandle&) = delete;

public:
   BackgroundHandle() {}

   static HitTestPreview HitPreview()
   {
      static wxCursor arrowCursor{ wxCURSOR_ARROW };
      return { {}, &arrowCursor };
   }

   virtual ~BackgroundHandle()
   {}

   Result Click
      (const TrackPanelMouseEvent &evt, AudacityProject *pProject) override
   {
      using namespace RefreshCode;
      const wxMouseEvent &event = evt.event;
      // Do not start a drag
      Result result = Cancelled;

      // AS: If the user clicked outside all tracks, make nothing
      //  selected.
      if ((event.ButtonDown() || event.ButtonDClick())) {
         pProject->GetSelectionState().SelectNone( *pProject->GetTracks() );
         result |= RefreshAll;
      }

      return result;
   }

   Result Drag
      (const TrackPanelMouseEvent &, AudacityProject *) override
   { return RefreshCode::RefreshNone; }

   HitTestPreview Preview
      (const TrackPanelMouseState &, const AudacityProject *) override
   { return HitPreview(); }

   Result Release
      (const TrackPanelMouseEvent &, AudacityProject *,
       wxWindow *) override
   { return RefreshCode::RefreshNone; }

   Result Cancel(AudacityProject *) override
   { return RefreshCode::RefreshNone; }
};

BackgroundCell::~BackgroundCell()
{
}

std::vector<UIHandlePtr> BackgroundCell::HitTest
(const TrackPanelMouseState &,
 const AudacityProject *)
{
   std::vector<UIHandlePtr> results;
   auto result = mHandle.lock();
   if (!result)
      result = std::make_shared<BackgroundHandle>();
   results.push_back(result);
   return results;
}

std::shared_ptr<Track> BackgroundCell::DoFindTrack()
{
   return {};
}

