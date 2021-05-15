/**********************************************************************

  Audacity: A Digital Audio Editor

  SBSMSEffect.h

  ClaytonOtey

  This abstract class contains all of the common code for an
  effect that uses SBSMS to do its processing (TimeScale)

**********************************************************************/

#ifndef __AUDACITY_EFFECT_SBSMS__
#define __AUDACITY_EFFECT_SBSMS__

#include "../Audacity.h" // for USE_* macros

#if USE_SBSMS

#include "Effect.h"
#include "../../../lib-src/header-substitutes/sbsms.h"

using namespace _sbsms_;

class LabelTrack;

class EffectSBSMS /* not final */ : public Effect
{
public:
   bool Process() override;
   void setParameters(double rateStart, double rateEnd, double pitchStart, double pitchEnd,
                      SlideType rateSlideType, SlideType pitchSlideType,
                      bool bLinkRatePitch, bool bRateReferenceInput, bool bPitchReferenceInput);
   void setParameters(double tempoRatio, double pitchRatio);  // Constant ratio (tempoRatio, pitchRatio)
   static double getInvertedStretchedTime(double rateStart, double rateEnd, SlideType slideType, double outputTime);
   static double getRate(double rateStart, double rateEnd, SlideType slideType, double t);

protected:
   wxString mProxyEffectName { XO("SBSMS Time / Pitch Stretch") };
   // This supplies the abstract virtual function, but in fact this symbol
   // does not get used:  this class is either a temporary helper, or else
   // GetSymbol() is overridden further in derived classes.
   ComponentInterfaceSymbol GetSymbol() override { return mProxyEffectName; }

private:
   bool ProcessLabelTrack(LabelTrack *track);
   double rateStart, rateEnd, pitchStart, pitchEnd;
   bool bLinkRatePitch, bRateReferenceInput, bPitchReferenceInput;
   SlideType rateSlideType;
   SlideType pitchSlideType;
   int mCurTrackNum;
   double mCurT0;
   double mCurT1;
   float mTotalStretch;

   friend class EffectChangeTempo;
   friend class EffectChangePitch;
};

#endif

#endif
