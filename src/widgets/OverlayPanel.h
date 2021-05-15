//
//  OverlayPanel.h
//  Audacity
//
//  Created by Paul Licameli on 5/1/16.
//
//

#ifndef __AUDACITY_OVERLAY_PANEL__
#define __AUDACITY_OVERLAY_PANEL__

#include <memory>
#include <vector>
#include "BackedPanel.h" // to inherit

class Overlay;

class AUDACITY_DLL_API OverlayPanel /* not final */ : public BackedPanel {
public:
   OverlayPanel(wxWindow * parent, wxWindowID id,
                const wxPoint & pos,
                const wxSize & size,
                // default as for wxPanel:
                long style = wxTAB_TRAVERSAL | wxNO_BORDER);

   // Registers overlay objects.
   // The sequence in which they were registered is the sequence in
   // which they are painted.
   // OverlayPanel is not responsible for their memory management.
   void AddOverlay( const std::weak_ptr<Overlay> &pOverlay );
   void ClearOverlays();

   // Erases and redraws to the client area the overlays that have
   // been previously added with AddOverlay(). If "repaint_all" is
   // true, all overlays will be erased and re-drawn. Otherwise, only
   // the ones that are out-of-date, as well as the intersecting ones,
   // will be erased and re-drawn.
   // pDC can be null, in which case, DrawOverlays() will create a
   // wxClientDC internally when necessary.
   void DrawOverlays(bool repaint_all, wxDC *pDC = nullptr);
   
private:
   void Compress();
   std::vector< std::weak_ptr<Overlay> > mOverlays;
   
   
   DECLARE_EVENT_TABLE()
   friend class GetInfoCommand;
};

#endif
