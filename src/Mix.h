/**********************************************************************

  Audacity: A Digital Audio Editor

  Mix.h

  Dominic Mazzoni
  Markus Meyer

********************************************************************//**

\class ArrayOf
\brief Memory.h template class for making an array of float, bool, etc.

\class ArraysOf
\brief memory.h template class for making an array of arrays.

*//********************************************************************/

#ifndef __AUDACITY_MIX__
#define __AUDACITY_MIX__

#include "MemoryX.h"
#include "SampleFormat.h"
#include <vector>

class Resample;
class DirManager;
class TimeTrack;
class TrackFactory;
class TrackList;
class WaveTrack;
using WaveTrackConstArray = std::vector < std::shared_ptr < const WaveTrack > >;
class WaveTrackCache;

/** @brief Mixes together all input tracks, applying any envelopes, amplitude
 * gain, panning, and real-time effects in the process.
 *
 * Takes one or more tracks as input; of all the WaveTrack s that are selected,
 * it mixes them together, applying any envelopes, amplitude gain, panning, and
 * real-time effects in the process.  The resulting pair of tracks (stereo) are
 * "rendered" and have no effects, gain, panning, or envelopes. Other sorts of
 * tracks are ignored.
 * If the start and end times passed are the same this is taken as meaning
 * no explicit time range to process, and the whole occupied length of the
 * input tracks is processed.
 */
void MixAndRender(TrackList * tracks, TrackFactory *factory,
                  double rate, sampleFormat format,
                  double startTime, double endTime,
                  std::shared_ptr<WaveTrack> &uLeft,
                  std::shared_ptr<WaveTrack> &uRight);

void MixBuffers(unsigned numChannels, int *channelFlags, float *gains,
                samplePtr src,
                samplePtr *dests, int len, bool interleaved);

class AUDACITY_DLL_API MixerSpec
{
   unsigned mNumTracks, mNumChannels, mMaxNumChannels;

   void Alloc();

public:
   ArraysOf<bool> mMap;

   MixerSpec( unsigned numTracks, unsigned maxNumChannels );
   MixerSpec( const MixerSpec &mixerSpec );
   virtual ~MixerSpec();

   bool SetNumChannels( unsigned numChannels );
   unsigned GetNumChannels() { return mNumChannels; }

   unsigned GetMaxNumChannels() { return mMaxNumChannels; }
   unsigned GetNumTracks() { return mNumTracks; }

   MixerSpec& operator=( const MixerSpec &mixerSpec );
};

class AUDACITY_DLL_API Mixer {
 public:

    // An argument to Mixer's constructor
    class WarpOptions
    {
    public:
       explicit WarpOptions(const TimeTrack *t)
          : timeTrack(t), minSpeed(0.0), maxSpeed(0.0)
       {}

       WarpOptions(double min, double max);

    private:
       friend class Mixer;
       const TimeTrack *timeTrack;
       double minSpeed, maxSpeed;
    };

    //
   // Constructor / Destructor
   //

   Mixer(const WaveTrackConstArray &inputTracks, bool mayThrow,
         const WarpOptions &warpOptions,
         double startTime, double stopTime,
         unsigned numOutChannels, size_t outBufferSize, bool outInterleaved,
         double outRate, sampleFormat outFormat,
         bool highQuality = true, MixerSpec *mixerSpec = NULL);

   virtual ~ Mixer();

   //
   // Setup
   //

   void ApplyTrackGains(bool apply = true); // True by default

   //
   // Processing
   //

   /// Process a maximum of 'maxSamples' samples and put them into
   /// a buffer which can be retrieved by calling GetBuffer().
   /// Returns number of output samples, or 0, if there are no
   /// more samples that must be processed.
   size_t Process(size_t maxSamples);

   /// Restart processing at beginning of buffer next time
   /// Process() is called.
   void Restart();

   /// Reposition processing to absolute time next time
   /// Process() is called.
   void Reposition(double t, bool bSkipping = false);

   // Used in scrubbing.
   void SetTimesAndSpeed(double t0, double t1, double speed);

   /// Current time in seconds (unwarped, i.e. always between startTime and stopTime)
   /// This value is not accurate, it's useful for progress bars and indicators, but nothing else.
   double MixGetCurrentTime();

   /// Retrieve the main buffer or the interleaved buffer
   samplePtr GetBuffer();

   /// Retrieve one of the non-interleaved buffers
   samplePtr GetBuffer(int channel);

 private:

   void Clear();
   size_t MixSameRate(int *channelFlags, WaveTrackCache &cache,
                           sampleCount *pos);

   size_t MixVariableRates(int *channelFlags, WaveTrackCache &cache,
                                sampleCount *pos, float *queue,
                                int *queueStart, int *queueLen,
                                Resample * pResample);

   void MakeResamplers();

 private:

    // Input
   size_t           mNumInputTracks;
   ArrayOf<WaveTrackCache> mInputTrack;
   bool             mbVariableRates;
   const TimeTrack *mTimeTrack;
   ArrayOf<sampleCount> mSamplePos;
   bool             mApplyTrackGains;
   Doubles          mEnvValues;
   double           mT0; // Start time
   double           mT1; // Stop time (none if mT0==mT1)
   double           mTime;  // Current time (renamed from mT to mTime for consistency with AudioIO - mT represented warped time there)
   ArrayOf<std::unique_ptr<Resample>> mResample;
   size_t           mQueueMaxLen;
   FloatBuffers     mSampleQueue;
   ArrayOf<int>     mQueueStart;
   ArrayOf<int>     mQueueLen;
   size_t           mProcessLen;
   MixerSpec        *mMixerSpec;

   // Output
   size_t              mMaxOut;
   unsigned         mNumChannels;
   Floats           mGains;
   unsigned         mNumBuffers;
   size_t              mBufferSize;
   size_t              mInterleavedBufferSize;
   sampleFormat     mFormat;
   bool             mInterleaved;
   ArrayOf<SampleBuffer> mBuffer, mTemp;
   Floats           mFloatBuffer;
   double           mRate;
   double           mSpeed;
   bool             mHighQuality;
   std::vector<double> mMinFactor, mMaxFactor;

   bool             mMayThrow;
};

#endif

