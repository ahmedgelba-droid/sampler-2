#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>
#include <vector>

// One loaded sample (mono) tagged with the MIDI note it was recorded at.
struct NaySample
{
    int rootNote = 60;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> data;
};

using NayBank = std::vector<std::shared_ptr<const NaySample>>;

// An "instrument": a named folder of samples.
struct NayGroup
{
    juce::String name;
    juce::String path;
    std::shared_ptr<const NayBank> bank; // loaded lazily
};

struct NayVoice
{
    std::shared_ptr<const NaySample> sample;
    juce::ADSR adsr;
    double playPos = 0.0, pitch = 60.0, target = 60.0, vibPhase = 0.0;
    int note = -1;
    bool gate = false;
    juce::uint64 age = 0;
};

class NayAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumVoices = 8;

    NayAudioProcessor();
    ~NayAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Gelbo Sampler"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- instruments (message thread only) ----
    // Folder with samples -> one instrument named after the folder.
    // Folder of sub-folders -> one instrument per sub-folder. Returns how many were added.
    int addFolder (const juce::File& folder);
    void selectGroup (int index);
    void renameGroup (int index, const juce::String& newName);
    int getNumGroups() const { return (int) groups.size(); }
    juce::String getGroupName (int i) const { return groups[(size_t) i].name; }
    int getActiveGroup() const { return activeGroup; }
    juce::String getSampleInfo() const { return sampleInfo; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static int parseRootNote (const juce::String& fileName);
    static bool folderHasSamples (const juce::File& folder);
    static std::shared_ptr<const NayBank> loadBank (const juce::File& folder);

    void addGroup (const juce::String& name, const juce::String& path);
    void handleMidi (const juce::MidiMessage&, const std::shared_ptr<const NayBank>&);
    void startVoice (NayVoice&, int note, const std::shared_ptr<const NayBank>&);
    void allNotesOff();
    void renderVoice (NayVoice&, juce::AudioBuffer<float>&, int start, int num);
    void renderAll (juce::AudioBuffer<float>&, int start, int num);
    void processDelay (juce::AudioBuffer<float>&);
    void processReverb (juce::AudioBuffer<float>&);
    double noteTarget (int note) const;

    // instruments
    std::vector<NayGroup> groups;
    int activeGroup = -1;
    juce::String sampleInfo { "No instruments yet - click Add Folder" };

    // bank used by the audio thread (swapped from the message thread)
    std::shared_ptr<const NayBank> bank;
    juce::SpinLock bankLock;

    // cached parameter pointers
    std::atomic<float>* pGainRaw     = nullptr;
    std::atomic<float>* pAttackRaw   = nullptr;
    std::atomic<float>* pReleaseRaw  = nullptr;
    std::atomic<float>* pGlideRaw    = nullptr;
    std::atomic<float>* pVibDepthRaw = nullptr;
    std::atomic<float>* pVibRateRaw  = nullptr;
    std::atomic<float>* pMonoRaw     = nullptr;
    std::atomic<float>* pLegatoRaw   = nullptr;
    std::atomic<float>* pDelayTimeRaw = nullptr;
    std::atomic<float>* pDelayFbRaw  = nullptr;
    std::atomic<float>* pDelayLvlRaw = nullptr;
    std::atomic<float>* pRvSizeRaw   = nullptr;
    std::atomic<float>* pRvLvlRaw    = nullptr;
    std::atomic<float>* offRaw[12] {};

    // per-block parameter values
    float pGain = 0.8f, pGlide = 60.f, pVibDepth = 0.f, pVibRate = 5.f;
    float pDelayTime = 350.f, pDelayFb = 0.3f, pDelayLvl = -10.f, pRvSize = 0.6f, pRvLvl = -20.f;
    bool pMono = true, pLegato = true, lastMono = true;
    float off[12] {}; // per-note fine tune in cents

    // voices (mono mode uses voices[0] only)
    std::array<NayVoice, kNumVoices> voices;
    std::vector<int> held;
    juce::uint64 ageCounter = 0;
    float modWheel = 0.f, bendSemis = 0.f;
    double sr = 44100.0;

    // effects
    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NayAudioProcessor)
};
