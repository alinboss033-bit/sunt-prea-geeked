#include "Common.h"
using namespace geeked;

class DelayProcessor : public Base
{
public:
    DelayProcessor() : Base (layout(), presets()) {}

    static APVTS::ParameterLayout layout()
    {
        APVTS::ParameterLayout l;
        l.add (B ("on", "Delay", true), C ("div", "Diviziune", StringArray { "1/4", U ("1/8 punctat"), "1/8", "1/16" }, 1),
               F ("bpm", "Tempo (fără host)", 60, 200, 120, "BPM"), F ("fb", "Feedback", 0, 95, 35, "%"),
               F ("pp", "Ping-pong", 0, 100, 60, "%"));
        l.add (B ("col_on", "Culoare", true), F ("tone", "Ton", 1000, 12000, 4500, "Hz", 0.4f),
               F ("lc", "Low cut", 20, 800, 250, "Hz", 0.5f), F ("sat", "Saturație", 0, 100, 25, "%"),
               F ("wow", "Wow", 0, 100, 20, "%"));
        l.add (F ("mix", "Mix", 0, 100, 30, "%"), F ("duck", "Duck", 0, 100, 40, "%"), F ("out", "Ieșire", -12, 12, 0, "dB"));
        return l;
    }

    static std::vector<Preset> presets()
    {
        struct DP { const char* n; float div, bpm, fb, pp, tone, lc, sat, wow, mix, duck; };
        static const DP t[] = {
            { "Curat 1/4",          0, 120, 30,   0, 8000, 200,  0,  0, 25, 40 },
            { "Tape 1/8 punctat",   1, 120, 35,  20, 4500, 300, 35, 25, 30, 40 },
            { "Ping-pong",          2, 120, 40, 100, 6000, 250, 10, 10, 28, 50 },
            { "Fantomă",            1, 120, 55,  60, 3500, 350, 30, 40, 35, 70 },
            { "Slapback 1/16",      3, 120, 20,   0, 7000, 200, 15,  0, 20, 30 },
            { "Lo-fi tape",         2, 120, 45,  40, 2800, 400, 70, 60, 30, 30 },
        };
        std::vector<Preset> out;
        for (auto& p : t)
            out.push_back ({ p.n, { { "div", p.div }, { "bpm", p.bpm }, { "fb", p.fb }, { "pp", p.pp }, { "tone", p.tone },
                { "lc", p.lc }, { "sat", p.sat }, { "wow", p.wow }, { "mix", p.mix }, { "duck", p.duck } } });
        return out;
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr;
        for (auto& b : buf) b.assign ((size_t) (2.2 * sr), 0.f);
        wp = 0; dCur = 0.f; ph1 = ph2 = 0.f; env = 0.f; hp.reset(); lp.reset();
    }

    void processBlock (AudioBuffer<float>& io, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        const int n = io.getNumSamples();
        if (! on ("on") || io.getNumChannels() < 2 || n == 0) return;
        float* l = io.getWritePointer (0);
        float* r = io.getWritePointer (1);

        double tempo = P ("bpm");
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) tempo = *b;
        if (tempo < 30.0) tempo = 120.0;

        static const float mult[4] = { 1.f, 0.75f, 0.5f, 0.25f };
        const int size = (int) buf[0].size();
        const float wowA = P ("wow") * 0.01f * 0.003f * (float) sampleRate;
        float target = (float) (60.0 / tempo * mult[jlimit (0, 3, (int) P ("div"))] * sampleRate);
        target = jlimit (16.f, (float) size - 8.f - wowA, target);
        if (dCur < 1.f) dCur = target;

        const bool col = on ("col_on");
        hp.set (Filt::Co::makeHighPass (sampleRate, col ? P ("lc") : 20.f, 0.707f));
        lp.set (Filt::Co::makeLowPass (sampleRate, col ? P ("tone") : 16000.f, 0.707f));
        const float fb = P ("fb") * 0.01f, pp = P ("pp") * 0.01f, mix = P ("mix") * 0.01f;
        const float drive = 1.f + (col ? 4.f * P ("sat") * 0.01f : 0.f);
        const float duckA = P ("duck") * 0.01f, g = Decibels::decibelsToGain (P ("out"));
        const float tw = MathConstants<float>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            dCur += (target - dCur) * 0.0008f;
            ph1 += 0.55f / (float) sampleRate; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += 5.3f / (float) sampleRate;  if (ph2 >= 1.f) ph2 -= 1.f;
            float dl = dCur + (col ? (std::sin (tw * ph1) + 0.25f * std::sin (tw * ph2)) * wowA : 0.f);
            float rp = (float) wp - dl;
            while (rp < 0.f) rp += (float) size;
            int i0 = (int) rp, i1 = (i0 + 1) % size;
            float fr = rp - (float) i0;

            float inL = l[i], inR = r[i], w[2];
            for (int c = 0; c < 2; ++c)
            {
                float raw = buf[c][(size_t) i0] * (1.f - fr) + buf[c][(size_t) i1] * fr;
                float x = lp.f[c].processSample (hp.f[c].processSample (raw));
                w[c] = std::tanh (x * drive) / std::sqrt (drive);
            }
            float a = jmax (std::abs (inL), std::abs (inR));
            env += (a - env) * (a > env ? 0.01f : 0.0004f);
            float duck = 1.f - duckA * jlimit (0.f, 1.f, env * 4.f);

            buf[0][(size_t) wp] = inL + (w[0] * (1.f - pp) + w[1] * pp) * fb;
            buf[1][(size_t) wp] = inR + (w[1] * (1.f - pp) + w[0] * pp) * fb;
            wp = (wp + 1) % size;

            l[i] = (inL + w[0] * mix * duck) * g;
            r[i] = (inR + w[1] * mix * duck) * g;
        }
    }

    AudioProcessorEditor* createEditor() override
    {
        return new Editor (*this, Colour (0xffff4fa3), "Delay", {
            { U ("Timp"), "on", { "div", "bpm", "fb", "pp" } },
            { U ("Culoare tape"), "col_on", { "tone", "lc", "sat", "wow" } },
            { U ("Mix"), "", { "mix", "duck", "out" } } }, 3);
    }

private:
    Filt hp, lp;
    std::vector<float> buf[2];
    int wp = 0;
    float dCur = 0.f, ph1 = 0.f, ph2 = 0.f, env = 0.f;
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelayProcessor(); }
