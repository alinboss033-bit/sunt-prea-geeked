#include "Common.h"
using namespace geeked;

class ReverbProcessor : public Base
{
public:
    ReverbProcessor() : Base (layout(), presets()) {}

    static APVTS::ParameterLayout layout()
    {
        APVTS::ParameterLayout l;
        l.add (B ("on", "Reverb", true), C ("type", "Tip", StringArray { "Plate", "Hall", U ("Cameră"), U ("Catedrală") }, 0),
               F ("size", "Mărime", 0, 100, 60, "%"), F ("decay", "Durată", 0, 100, 55, "%"), F ("pre", "Predelay", 0, 120, 25, "ms"));
        l.add (B ("filt_on", "Filtre", true), F ("damp", "Damping", 0, 100, 50, "%"),
               F ("lc", "Low cut", 20, 600, 300, "Hz", 0.5f), F ("hc", "High cut", 2000, 16000, 8000, "Hz", 0.5f));
        l.add (F ("mix", "Mix", 0, 100, 30, "%"), F ("width", "Lățime", 0, 100, 100, "%"), F ("out", "Ieșire", -12, 12, 0, "dB"));
        return l;
    }

    static std::vector<Preset> presets()
    {
        struct RP { const char* n; float type, size, decay, pre, damp, lc, hc, width, mix; };
        static const RP t[] = {
            { "Plate vocal",      0, 50, 55, 25, 40, 300, 9000, 100, 30 },
            { "Hall",             1, 65, 60, 35, 50, 250, 8000, 100, 28 },
            { "Cameră mică",      2, 35, 35, 10, 55, 200, 10000, 80, 22 },
            { "Catedrală",        3, 80, 75, 50, 40, 300, 6500, 100, 35 },
            { "Spațiu fantomă",   3, 90, 90, 60, 55, 400, 5500, 110, 45 },
        };
        std::vector<Preset> out;
        for (auto& p : t)
            out.push_back ({ p.n, { { "type", p.type }, { "size", p.size }, { "decay", p.decay }, { "pre", p.pre },
                { "damp", p.damp }, { "lc", p.lc }, { "hc", p.hc }, { "width", p.width }, { "mix", p.mix } } });
        return out;
    }

    void prepareToPlay (double sr, int bs) override
    {
        sampleRate = sr;
        for (auto& b : pd) b.assign ((size_t) (0.13 * sr) + 8, 0.f);
        pw = 0; hpF.reset(); lpF.reset();
        wet.setSize (2, bs);
        rv.setSampleRate (sr); rv.reset();
    }

    void processBlock (AudioBuffer<float>& buf, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        const int n = buf.getNumSamples();
        if (! on ("on") || buf.getNumChannels() < 2 || n == 0) return;
        float* l = buf.getWritePointer (0);
        float* r = buf.getWritePointer (1);

        static const float mul[4] = { 0.9f, 1.f, 0.5f, 1.12f };
        static const float dampOff[4] = { -0.15f, 0.f, 0.1f, -0.05f };
        const int type = jlimit (0, 3, (int) P ("type"));
        const float size = P ("size") * 0.01f, dec = P ("decay") * 0.01f;
        Reverb::Parameters p;
        p.roomSize = jlimit (0.05f, 0.98f, (0.2f + 0.75f * dec) * (0.7f + 0.3f * size) * mul[type]);
        p.damping = jlimit (0.f, 1.f, P ("damp") * 0.01f + dampOff[type]);
        p.width = P ("width") * 0.01f; p.wetLevel = 0.33f; p.dryLevel = 0.f; p.freezeMode = 0.f;
        rv.setParameters (p);

        wet.setSize (2, n, false, false, true);
        float* wl = wet.getWritePointer (0);
        float* wr = wet.getWritePointer (1);
        const int psize = (int) pd[0].size();
        const int d = jlimit (0, psize - 2, (int) (P ("pre") * 0.001f * (float) sampleRate));
        for (int i = 0; i < n; ++i)
        {
            pd[0][(size_t) pw] = l[i]; pd[1][(size_t) pw] = r[i];
            int rp = (pw - d + psize) % psize;
            wl[i] = pd[0][(size_t) rp]; wr[i] = pd[1][(size_t) rp];
            pw = (pw + 1) % psize;
        }
        rv.processStereo (wl, wr, n);

        if (on ("filt_on"))
        {
            hpF.set (Filt::Co::makeHighPass (sampleRate, P ("lc"), 0.707f));
            lpF.set (Filt::Co::makeLowPass (sampleRate, P ("hc"), 0.707f));
            for (int i = 0; i < n; ++i)
            {
                wl[i] = lpF.f[0].processSample (hpF.f[0].processSample (wl[i]));
                wr[i] = lpF.f[1].processSample (hpF.f[1].processSample (wr[i]));
            }
        }
        const float m = P ("mix") * 0.016f, g = Decibels::decibelsToGain (P ("out"));
        for (int i = 0; i < n; ++i) { l[i] = (l[i] + wl[i] * m) * g; r[i] = (r[i] + wr[i] * m) * g; }
    }

    AudioProcessorEditor* createEditor() override
    {
        return new Editor (*this, Colour (0xff2dd4ff), "Reverb", {
            { U ("Cameră"), "on", { "type", "size", "decay", "pre" } },
            { U ("Culoare"), "filt_on", { "damp", "lc", "hc" } },
            { U ("Mix"), "", { "mix", "width", "out" } } }, 3);
    }

private:
    Reverb rv;
    Filt hpF, lpF;
    std::vector<float> pd[2];
    AudioBuffer<float> wet;
    int pw = 0;
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverbProcessor(); }
