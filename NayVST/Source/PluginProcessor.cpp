#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac";

static float dbToGainOrOff (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NayAudioProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<F> (juce::ParameterID { "gain", 1 },     "Gain",         R (0.f, 1.f, 0.001f), 0.8f));
    l.add (std::make_unique<F> (juce::ParameterID { "attack", 1 },   "Attack",       R (0.002f, 1.f, 0.001f, 0.4f), 0.03f));
    l.add (std::make_unique<F> (juce::ParameterID { "release", 1 },  "Release",      R (0.02f, 3.f, 0.001f, 0.4f), 0.25f));
    l.add (std::make_unique<F> (juce::ParameterID { "glide", 1 },    "Legato Glide", R (0.f, 500.f, 1.f, 0.5f), 60.f));
    l.add (std::make_unique<F> (juce::ParameterID { "vibDepth", 1 }, "Vib Depth",    R (0.f, 100.f, 0.1f), 12.f));
    l.add (std::make_unique<F> (juce::ParameterID { "vibRate", 1 },  "Vib Speed",    R (0.5f, 10.f, 0.01f), 5.f));

    l.add (std::make_unique<B> (juce::ParameterID { "mono", 1 },     "Mono (off = Poly)", true));
    l.add (std::make_unique<B> (juce::ParameterID { "legato", 1 },   "Legato (Mono)",     true));

    l.add (std::make_unique<F> (juce::ParameterID { "delayTime", 1 }, "Delay Time",     R (10.f, 1000.f, 1.f, 0.5f), 350.f));
    l.add (std::make_unique<F> (juce::ParameterID { "delayFb", 1 },   "Delay Feedback", R (0.f, 0.9f, 0.001f), 0.35f));
    l.add (std::make_unique<F> (juce::ParameterID { "delayLevel", 1 }, "Delay Level (dB)", R (-60.f, 0.f, 0.1f), -10.f));
    l.add (std::make_unique<F> (juce::ParameterID { "rvSize", 1 },    "Reverb Size",    R (0.f, 1.f, 0.001f), 0.6f));
    l.add (std::make_unique<F> (juce::ParameterID { "rvLevel", 1 },    "Reverb Level (dB)", R (-60.f, 0.f, 0.1f), -20.f));

    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<F> (juce::ParameterID { "off" + juce::String (i), 1 },
                                    juce::String ("Fine tune ") + kNoteNames[i] + " (cents)", R (-100.f, 100.f, 1.f), 0.f));
    return l;
}

//==============================================================================
NayAudioProcessor::NayAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAY_STATE", createLayout())
{
    pGainRaw      = apvts.getRawParameterValue ("gain");
    pAttackRaw    = apvts.getRawParameterValue ("attack");
    pReleaseRaw   = apvts.getRawParameterValue ("release");
    pGlideRaw     = apvts.getRawParameterValue ("glide");
    pVibDepthRaw  = apvts.getRawParameterValue ("vibDepth");
    pVibRateRaw   = apvts.getRawParameterValue ("vibRate");
    pMonoRaw      = apvts.getRawParameterValue ("mono");
    pLegatoRaw    = apvts.getRawParameterValue ("legato");
    pDelayTimeRaw = apvts.getRawParameterValue ("delayTime");
    pDelayFbRaw   = apvts.getRawParameterValue ("delayFb");
    pDelayLvlRaw  = apvts.getRawParameterValue ("delayLevel");
    pRvSizeRaw    = apvts.getRawParameterValue ("rvSize");
    pRvLvlRaw     = apvts.getRawParameterValue ("rvLevel");

    for (int i = 0; i < 12; ++i)
        offRaw[i] = apvts.getRawParameterValue ("off" + juce::String (i));
}

bool NayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void NayAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    for (auto& v : voices)
    {
        v.adsr.setSampleRate (sampleRate);
        v.adsr.reset();
        v.sample.reset();
        v.gate = false;
        v.note = -1;
    }
    held.clear();

    delayBuf.setSize (2, (int) (sampleRate * 1.1) + 8);
    delayBuf.clear();
    delayWrite = 0;

    reverb.setSampleRate (sampleRate);
    reverb.reset();
}

//==============================================================================
// File names must start with a note: "C4.wav", "F#3.wav", "Bb2.wav" or a MIDI number "60.wav".
int NayAudioProcessor::parseRootNote (const juce::String& fileName)
{
    auto t = fileName.trim().upToFirstOccurrenceOf ("_", false, false)
                            .upToFirstOccurrenceOf (" ", false, false);
    if (t.isEmpty())
        return -1;

    if (t.containsOnly ("0123456789"))
    {
        const int n = t.getIntValue();
        return n <= 127 ? n : -1;
    }

    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto c = juce::CharacterFunctions::toUpperCase (t[0]);
    if (c < 'A' || c > 'G')
        return -1;

    int pc = base[c - 'A'];
    int idx = 1;
    if (t[idx] == '#')      { ++pc; ++idx; }
    else if (t[idx] == 'b') { --pc; ++idx; }

    const auto oct = t.substring (idx);
    if (oct.isEmpty() || ! oct.containsOnly ("-0123456789"))
        return -1;

    const int note = (oct.getIntValue() + 1) * 12 + pc;
    return (note >= 0 && note <= 127) ? note : -1;
}

bool NayAudioProcessor::folderHasSamples (const juce::File& folder)
{
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, kAudioWildcard))
        if (parseRootNote (f.getFileNameWithoutExtension()) >= 0)
            return true;
    return false;
}

std::shared_ptr<const NayBank> NayAudioProcessor::loadBank (const juce::File& folder)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    auto newBank = std::make_shared<NayBank>();

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, kAudioWildcard))
    {
        const int root = parseRootNote (f.getFileNameWithoutExtension());
        if (root < 0)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples < 4)
            continue;

        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
        const int chans = (int) reader->numChannels;

        juce::AudioBuffer<float> tmp (chans, len);
        reader->read (&tmp, 0, len, 0, true, true);

        auto s = std::make_shared<NaySample>();
        s->rootNote = root;
        s->sampleRate = reader->sampleRate;
        s->data.setSize (1, len);
        s->data.clear();
        for (int ch = 0; ch < chans; ++ch)
            s->data.addFrom (0, 0, tmp, ch, 0, len, 1.0f / (float) chans);

        newBank->push_back (s);
    }

    std::sort (newBank->begin(), newBank->end(),
               [] (const auto& a, const auto& b) { return a->rootNote < b->rootNote; });

    return newBank;
}

//==============================================================================
void NayAudioProcessor::addGroup (const juce::String& name, const juce::String& path)
{
    for (const auto& g : groups)
        if (g.path == path)
            return; // already added

    groups.push_back ({ name, path, nullptr });
}

int NayAudioProcessor::addFolder (const juce::File& folder)
{
    const int firstNew = (int) groups.size();

    if (folderHasSamples (folder))
    {
        addGroup (folder.getFileName(), folder.getFullPathName());
    }
    else
    {
        auto subs = folder.findChildFiles (juce::File::findDirectories, false);
        std::sort (subs.begin(), subs.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        for (const auto& s : subs)
            if (folderHasSamples (s))
                addGroup (s.getFileName(), s.getFullPathName());
    }

    const int added = (int) groups.size() - firstNew;

    if (added > 0)
        selectGroup (firstNew);
    else
        sampleInfo = "No new samples found (name files like C4.wav)";

    return added;
}

void NayAudioProcessor::selectGroup (int index)
{
    if (index < 0 || index >= (int) groups.size())
        return;

    auto& g = groups[(size_t) index];
    if (g.bank == nullptr)
        g.bank = loadBank (juce::File (g.path));

    activeGroup = index;

    {
        juce::SpinLock::ScopedLockType lock (bankLock);
        bank = g.bank;
    }

    sampleInfo = g.name + ": " + juce::String ((int) g.bank->size()) + " samples";
}

void NayAudioProcessor::renameGroup (int index, const juce::String& newName)
{
    if (index < 0 || index >= (int) groups.size() || newName.trim().isEmpty())
        return;

    groups[(size_t) index].name = newName.trim();

    if (index == activeGroup && groups[(size_t) index].bank != nullptr)
        sampleInfo = groups[(size_t) index].name + ": " + juce::String ((int) groups[(size_t) index].bank->size()) + " samples";
}

//==============================================================================
double NayAudioProcessor::noteTarget (int note) const
{
    // Each note name (C..B) has its own fine-tune offset in cents (-50 = quarter tone flat).
    return (double) note + (double) off[note % 12] * 0.01;
}

void NayAudioProcessor::startVoice (NayVoice& v, int note, const std::shared_ptr<const NayBank>& b)
{
    if (b == nullptr || b->empty())
        return;

    std::shared_ptr<const NaySample> best;
    int bestDist = 1000;
    for (const auto& s : *b)
    {
        const int d = std::abs (s->rootNote - note);
        if (d < bestDist) { bestDist = d; best = s; }
    }

    v.sample = best;
    v.playPos = 0.0;
    v.note = note;
    v.target = noteTarget (note);
    v.pitch = v.target;
    v.vibPhase = 0.0;
    v.gate = true;
    v.age = ++ageCounter;
    v.adsr.noteOn();
}

void NayAudioProcessor::allNotesOff()
{
    held.clear();
    for (auto& v : voices)
    {
        v.gate = false;
        v.adsr.noteOff();
    }
}

void NayAudioProcessor::handleMidi (const juce::MidiMessage& msg, const std::shared_ptr<const NayBank>& b)
{
    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();

        if (pMono)
        {
            const bool wasHeld = ! held.empty();
            held.erase (std::remove (held.begin(), held.end(), n), held.end());
            held.push_back (n);

            auto& v = voices[0];
            if (pLegato && wasHeld && v.adsr.isActive() && v.sample != nullptr)
            {
                // legato: no retrigger, just glide (quarter-tone offsets are applied by noteTarget)
                v.note = n;
                v.target = noteTarget (n);
                v.gate = true;
            }
            else
            {
                startVoice (v, n, b);
            }
        }
        else
        {
            NayVoice* freeVoice = nullptr;
            NayVoice* oldest = &voices[0];

            for (auto& v : voices)
            {
                if (! v.adsr.isActive()) { freeVoice = &v; break; }
                if (v.age < oldest->age) oldest = &v;
            }

            startVoice (freeVoice != nullptr ? *freeVoice : *oldest, n, b);
        }
    }
    else if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();

        if (pMono)
        {
            held.erase (std::remove (held.begin(), held.end(), n), held.end());
            auto& v = voices[0];

            if (held.empty())
            {
                v.gate = false;
                v.adsr.noteOff();
            }
            else if (v.note == n)
            {
                // the sounding note was released while another is still held
                if (pLegato)
                {
                    v.note = held.back();
                    v.target = noteTarget (v.note);
                }
                else
                {
                    startVoice (v, held.back(), b);
                }
            }
        }
        else
        {
            for (auto& v : voices)
                if (v.gate && v.note == n)
                {
                    v.gate = false;
                    v.adsr.noteOff();
                }
        }
    }
    else if (msg.isController() && msg.getControllerNumber() == 1)
    {
        modWheel = (float) msg.getControllerValue() / 127.0f;
    }
    else if (msg.isPitchWheel())
    {
        bendSemis = ((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        allNotesOff();
    }
}

//==============================================================================
void NayAudioProcessor::renderVoice (NayVoice& v, juce::AudioBuffer<float>& buf, int start, int num)
{
    if (num <= 0 || v.sample == nullptr || ! v.adsr.isActive())
        return;

    const auto& s = *v.sample;
    const int len = s.data.getNumSamples();
    if (len < 4)
        return;

    const float* src = s.data.getReadPointer (0);
    float* L = buf.getWritePointer (0, start);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1, start) : nullptr;

    // Sustain loop: second half of the sample repeats while the key is held.
    const double loopEnd = (double) len - 1.0;
    const double loopStart = juce::jmin (loopEnd - 1.0, (double) len * 0.5);
    const double glideCoef = pGlide < 1.0f ? 1.0 : 1.0 - std::exp (-1.0 / ((double) pGlide * 0.001 * sr));
    const double rateFix = s.sampleRate / sr;
    const double depthCents = juce::jmin (100.0, (double) pVibDepth + (double) modWheel * 50.0);

    for (int i = 0; i < num; ++i)
    {
        v.pitch += (v.target - v.pitch) * glideCoef;

        v.vibPhase += (double) pVibRate / sr;
        if (v.vibPhase >= 1.0)
            v.vibPhase -= 1.0;
        const double vib = std::sin (juce::MathConstants<double>::twoPi * v.vibPhase) * depthCents * 0.01;

        const double semis = v.pitch + (double) bendSemis + vib - (double) s.rootNote;

        while (v.playPos >= loopEnd)
            v.playPos -= (loopEnd - loopStart);

        const int i0 = (int) v.playPos;
        const float frac = (float) (v.playPos - (double) i0);
        const float smp = src[i0] + frac * (src[i0 + 1] - src[i0]);
        const float out = smp * v.adsr.getNextSample() * pGain;

        L[i] += out;
        if (R != nullptr)
            R[i] += out;

        v.playPos += std::pow (2.0, semis / 12.0) * rateFix;

        if (! v.adsr.isActive())
            break;
    }
}

void NayAudioProcessor::renderAll (juce::AudioBuffer<float>& buf, int start, int num)
{
    if (num <= 0)
        return;

    if (pMono)
    {
        renderVoice (voices[0], buf, start, num);
    }
    else
    {
        for (auto& v : voices)
            renderVoice (v, buf, start, num);
    }
}

//==============================================================================
void NayAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int size = delayBuf.getNumSamples();
    if (size < 16)
        return;

    const int d = juce::jlimit (1, size - 1, (int) ((double) pDelayTime * 0.001 * sr));
    const float fb = pDelayFb, mix = dbToGainOrOff (pDelayLvl);
    const int chans = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < chans; ++ch)
    {
        float* x = buffer.getWritePointer (ch);
        float* db = delayBuf.getWritePointer (ch);
        int w = delayWrite;

        for (int i = 0; i < n; ++i)
        {
            int r = w - d;
            if (r < 0)
                r += size;

            const float delayed = db[r];
            db[w] = x[i] + delayed * fb;
            x[i] += delayed * mix;

            if (++w >= size)
                w = 0;
        }
    }

    delayWrite = (delayWrite + n) % size;
}

void NayAudioProcessor::processReverb (juce::AudioBuffer<float>& buffer)
{
    juce::Reverb::Parameters p;
    p.roomSize = pRvSize;
    p.damping = 0.5f;
    const float wet = dbToGainOrOff (pRvLvl);
    p.wetLevel = wet;
    p.dryLevel = 1.0f - 0.4f * wet;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    if (buffer.getNumChannels() >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else
        reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());
}

//==============================================================================
void NayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    std::shared_ptr<const NayBank> localBank;
    {
        juce::SpinLock::ScopedTryLockType lock (bankLock);
        if (lock.isLocked())
            localBank = bank;
    }

    pGain      = pGainRaw->load();
    pGlide     = pGlideRaw->load();
    pVibDepth  = pVibDepthRaw->load();
    pVibRate   = pVibRateRaw->load();
    pMono      = pMonoRaw->load() > 0.5f;
    pLegato    = pLegatoRaw->load() > 0.5f;
    pDelayTime = pDelayTimeRaw->load();
    pDelayFb   = pDelayFbRaw->load();
    pDelayLvl  = pDelayLvlRaw->load();
    pRvSize    = pRvSizeRaw->load();
    pRvLvl     = pRvLvlRaw->load();
    for (int i = 0; i < 12; ++i)
        off[i] = offRaw[i]->load();

    if (pMono != lastMono)
    {
        allNotesOff(); // switching mono <-> poly releases everything
        lastMono = pMono;
    }

    const juce::ADSR::Parameters ap { pAttackRaw->load(), 0.05f, 1.0f, pReleaseRaw->load() };
    for (auto& v : voices)
    {
        v.adsr.setParameters (ap);
        if (v.adsr.isActive())
            v.target = noteTarget (v.note); // live update when quarter-tone buttons change
    }

    int pos = 0;
    const int total = buffer.getNumSamples();

    for (const auto metadata : midi)
    {
        const int p = juce::jlimit (0, total, metadata.samplePosition);
        renderAll (buffer, pos, p - pos);
        pos = p;
        handleMidi (metadata.getMessage(), localBank);
    }

    renderAll (buffer, pos, total - pos);

    processDelay (buffer);
    processReverb (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* NayAudioProcessor::createEditor()
{
    return new NayAudioProcessorEditor (*this);
}

void NayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    auto old = state.getChildWithName ("GROUPS");
    if (old.isValid())
        state.removeChild (old, nullptr);

    juce::ValueTree gs ("GROUPS");
    gs.setProperty ("active", activeGroup, nullptr);
    for (const auto& g : groups)
    {
        juce::ValueTree c ("GROUP");
        c.setProperty ("name", g.name, nullptr);
        c.setProperty ("path", g.path, nullptr);
        gs.appendChild (c, nullptr);
    }
    state.appendChild (gs, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NayAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (st);

            auto gs = st.getChildWithName ("GROUPS");
            if (gs.isValid())
            {
                groups.clear();
                activeGroup = -1;

                for (int i = 0; i < gs.getNumChildren(); ++i)
                {
                    auto c = gs.getChild (i);
                    groups.push_back ({ c.getProperty ("name").toString(), c.getProperty ("path").toString(), nullptr });
                }

                const int a = (int) gs.getProperty ("active", -1);
                if (a >= 0 && a < (int) groups.size())
                    selectGroup (a);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NayAudioProcessor();
}
