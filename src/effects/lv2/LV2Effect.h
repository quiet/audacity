/**********************************************************************

  Audacity: A Digital Audio Editor

  LV2Effect.h

  Audacity(R) is copyright (c) 1999-2013 Audacity Team.
  License: GPL v2.  See License.txt.

*********************************************************************/

#include "../../Audacity.h" // for USE_* macros

#if USE_LV2

class wxArrayString;

#include "../../MemoryX.h"
#include <vector>

#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/data-access/data-access.h"
#include "lv2/lv2plug.in/ns/ext/options/options.h"
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include <lilv/lilv.h>
#include <suil/suil.h>

#include "../../SampleFormat.h"
#include "../../widgets/NumericTextCtrl.h"

#include "LoadLV2.h"

#include <unordered_map>

class wxSlider;
class wxTextCtrl;

#define LV2EFFECTS_VERSION wxT("1.0.0.0")
/* i18n-hint: abbreviates
   "Linux Audio Developer's Simple Plugin API (LADSPA) version 2" */
#define LV2EFFECTS_FAMILY XO("LV2")

/** A structure that contains information about a single LV2 plugin port. */
class LV2Port
{
public:
   LV2Port()
   {
      mInput = false;
      mToggle = false;
      mTrigger = false;
      mInteger = false;
      mSampleRate = false;
      mEnumeration = false;
      mLogarithmic = false;
      mHasLo = false;
      mHasHi = false;
   }
   LV2Port( const LV2Port & ) = default;
   LV2Port& operator = ( const LV2Port & ) = default;
   //LV2Port( LV2Port && ) = default;
   //LV2Port& operator = ( LV2Port && ) = default;

   uint32_t mIndex;
   wxString mSymbol;
   wxString mName;
   wxString mGroup;
   wxString mUnits;
   float mMin;
   float mMax;
   float mDef;
   float mVal;
   float mTmp;
   float mLo;
   float mHi;
   bool mHasLo;
   bool mHasHi;
   bool mInput;
   bool mToggle;
   bool mTrigger;
   bool mInteger;
   bool mSampleRate;
   bool mEnumeration;
   bool mLogarithmic;

   LilvPort *mPort;

   // ScalePoints
   std::vector<double> mScaleValues;
   wxArrayString mScaleLabels;
};

using LV2GroupMap = std::unordered_map<wxString, std::vector<int>>;

class LV2EffectSettingsDialog;

class LV2Effect final : public wxEvtHandler,
                  public EffectClientInterface,
                  public EffectUIClientInterface
{
public:
   LV2Effect(const LilvPlugin *plug);
   virtual ~LV2Effect();

   // ComponentInterface implementation

   PluginPath GetPath() override;
   ComponentInterfaceSymbol GetSymbol() override;
   VendorSymbol GetVendor() override;
   wxString GetVersion() override;
   wxString GetDescription() override;

   // EffectDefinitionInterface implementation

   EffectType GetType() override;
   EffectFamilySymbol GetFamily() override;
   bool IsInteractive() override;
   bool IsDefault() override;
   bool IsLegacy() override;
   bool SupportsRealtime() override;
   bool SupportsAutomation() override;

   // EffectClientInterface implementation

   bool SetHost(EffectHostInterface *host) override;

   unsigned GetAudioInCount() override;
   unsigned GetAudioOutCount() override;

   int GetMidiInCount() override;
   int GetMidiOutCount() override;

   void SetSampleRate(double rate) override;
   size_t SetBlockSize(size_t maxBlockSize) override;

   sampleCount GetLatency() override;
   size_t GetTailSize() override;

   bool IsReady() override;
   bool ProcessInitialize(sampleCount totalLen, ChannelNames chanMap = NULL) override;
   bool ProcessFinalize() override;
   size_t ProcessBlock(float **inbuf, float **outbuf, size_t size) override;

   bool RealtimeInitialize() override;
   bool RealtimeAddProcessor(unsigned numChannels, float sampleRate) override;
   bool RealtimeFinalize() override;
   bool RealtimeSuspend() override;
   bool RealtimeResume() override;
   bool RealtimeProcessStart() override;
   size_t RealtimeProcess(int group,
                                       float **inbuf,
                                       float **outbuf,
                                       size_t numSamples) override;
   bool RealtimeProcessEnd() override;

   bool ShowInterface(wxWindow *parent, bool forceModal = false) override;

   bool GetAutomationParameters(CommandParameters & parms) override;
   bool SetAutomationParameters(CommandParameters & parms) override;

   // EffectUIClientInterface implementation

   void SetHostUI(EffectUIHostInterface *host) override;
   bool PopulateUI(wxWindow *parent) override;
   bool IsGraphicalUI() override;
   bool ValidateUI() override;
   bool HideUI() override;
   bool CloseUI() override;

   bool LoadUserPreset(const RegistryPath & name) override;
   bool SaveUserPreset(const RegistryPath & name) override;

   RegistryPaths GetFactoryPresets() override;
   bool LoadFactoryPreset(int id) override;
   bool LoadFactoryDefaults() override;

   bool CanExportPresets() override;
   void ExportPresets() override;
   void ImportPresets() override;

   bool HasOptions() override;
   void ShowOptions() override;

   // LV2Effect implementation

private:
   bool Load();
   void Unload();

   bool LoadParameters(const RegistryPath & group);
   bool SaveParameters(const RegistryPath & group);

   LilvInstance *InitInstance(float sampleRate);
   void FreeInstance(LilvInstance *handle);

   static uint32_t uri_to_id(LV2_URI_Map_Callback_Data callback_data,
                             const char *map,
                             const char *uri);

   static LV2_URID urid_map(LV2_URID_Map_Handle handle, const char *uri);
   LV2_URID URID_Map(const char *uri);

   static const char *urid_unmap(LV2_URID_Unmap_Handle handle, LV2_URID urid);
   const char *URID_Unmap(LV2_URID urid);

   static int ui_resize(LV2UI_Feature_Handle handle, int width, int height);
   int UIResize(int width, int height);

   size_t AddOption(const char *key, uint32_t size, const char *type, void *value);
   LV2_Feature *AddFeature(const char *uri, void *data);

   bool BuildFancy();
   bool BuildPlain();

   bool TransferDataToWindow() /* not override */;
   bool TransferDataFromWindow() /* not override */;
   void SetSlider(wxSlider *slider, const LV2Port & ctrl);

   void OnTrigger(wxCommandEvent & evt);
   void OnToggle(wxCommandEvent & evt);
   void OnChoice(wxCommandEvent & evt);
   void OnText(wxCommandEvent & evt);
   void OnSlider(wxCommandEvent & evt);

   void OnIdle(wxIdleEvent & evt);

   static void suil_write_func(SuilController controller,
                               uint32_t       port_index,
                               uint32_t       buffer_size,
                               uint32_t       protocol,
                               const void     *buffer);

   void UIWrite(uint32_t port_index,
                uint32_t buffer_size,
                uint32_t protocol,
                const void *buffer);

   static void set_value_func(const char *port_symbol,
                              void       *user_data,
                              const void *value,
                              uint32_t   size,
                              uint32_t   type);

   void SetPortValue(const char *port_symbol,
                     const void *value,
                     uint32_t   size,
                     uint32_t   type);

private:
   // Declare the static URI nodes
   #undef URI
   #define URI(n, u) static LilvNode *n;
   URILIST

   const LilvPlugin *mPlug;

   EffectHostInterface *mHost;

   size_t mBlockSize;
   double mSampleRate;

   wxLongToLongHashMap mControlsMap;
   std::vector<LV2Port> mControls;
   std::vector<int> mAudioInputs;
   std::vector<int> mAudioOutputs;

   LV2GroupMap mGroupMap;
   wxArrayString mGroups;

   bool mUseLatency;
   int mLatencyPort;
   bool mLatencyDone;
   float mLatency;

   LilvInstance *mMaster;
   LilvInstance *mProcess;
   std::vector<LilvInstance*> mSlaves;

   FloatBuffers mMasterIn, mMasterOut;
   size_t mNumSamples;

   double mLength;

   wxDialog *mDialog;
   wxWindow *mParent;
   EffectUIHostInterface *mUIHost;

   bool mUseGUI;

   std::vector< std::unique_ptr<char, freer> > mURIMap;

   LV2_URI_Map_Feature mUriMapFeature;
   LV2_URID_Map mURIDMapFeature;
   LV2_URID_Unmap mURIDUnmapFeature;
   LV2UI_Resize mUIResizeFeature;
   LV2_Extension_Data_Feature mExtDataFeature;
   
   size_t mBlockSizeOption;
   size_t mSampleRateOption;

   LV2_Options_Interface *mOptionsInterface;
   std::vector<LV2_Options_Option> mOptions;

   std::vector<std::unique_ptr<LV2_Feature>> mFeatures;

   LV2_Feature *mInstanceAccessFeature;
   LV2_Feature *mParentFeature;

   const LV2UI_Idle_Interface *mIdleFeature;

   SuilHost *mSuilHost;
   SuilInstance *mSuilInstance;

   NumericTextCtrl *mDuration;
   ArrayOf<wxSlider*> mSliders;
   ArrayOf<wxTextCtrl*> mFields;

   bool mFactoryPresetsLoaded;
   RegistryPaths mFactoryPresetNames;
   wxArrayString mFactoryPresetUris;

   DECLARE_EVENT_TABLE()

   friend class LV2EffectSettingsDialog;
   friend class LV2EffectsModule;
};

inline wxString LilvString(const LilvNode *node)
{
   return wxString::FromUTF8(lilv_node_as_string(node));
};

inline wxString LilvString(LilvNode *node, bool free)
{
   wxString str = LilvString(node);
   if (free)
   {
      lilv_node_free(node);
   }

   return str;
};


#endif
