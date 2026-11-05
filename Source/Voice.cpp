#include "Common.h"
using namespace geeked;

class VoiceProcessor : public Base
{
public:
    VoiceProcessor() : Base (layout(), presets()) {}

    static APVTS::ParameterLayout layout()
    {
        APVTS::ParameterLayout l;
        l.add (B ("eq_on", "EQ", true), F ("hpf", "Joase", 40, 300, 90, "Hz", 0.5f), F ("mud", "Noroi", -8, 0, -2, "dB"),
               F ("pres", "Prezență", 0, 6, 2, "dB"), F ("air", "Aer", 0, 8, 3, "dB"));
        l.add (B ("comp_on", "Compresor", true), F ("thr", "Prag", -40, 0, -20, "dB"), F ("ratio", "Raport", 1, 12, 4, ":1", 0.6f),
               F ("atk", "Attack", 1, 50, 8, "ms", 0.5f), F ("makeup", "Makeup", 0, 12, 3, "dB"));
        l.add (B ("ghost_on", "Fantomă", false), F ("g_mix", "Nivel", 0, 100, 25, "%"), F ("g_time", "Timp", 100, 1500, 450, "ms", 0.6f),
               F ("g_fb", "Repetări", 0, 90, 45, "%"), F ("g_space", "Spațiu", 0, 100, 60, "%"));
        l.add (B ("chor_on", "Chorus", false), F ("c_rate", "Viteză", 0.1f, 5, 0.8f, "Hz", 0.5f), F ("c_depth", "Adâncime", 0, 100, 40, "%"),
               F ("c_mix", "Mix", 0, 100, 35, "%"));
        l.add (B ("fl_on", "Flanger", false), F ("f_rate", "Viteză", 0.05f, 2, 0.3f, "Hz", 0.5f), F ("f_depth", "Adâncime", 0, 100, 60, "%"),
               F ("f_fb", "Feedback", -90, 90, 40, "%"), F ("f_mix", "Mix", 0, 100, 40, "%"));
        l.add (B ("st_on", "Stereo", true), F ("width", "Lățime", 0, 200, 110, "%"), F ("out", "Ieșire", -12, 12, 0, "dB"),
               F ("ceil", "Plafon", -6, 0, -1, "dB"));
        return l;
    }

    static std::vector<Preset> presets()
    {
        struct VP { const char* n; float hpf, mud, pres, air, thr, ratio, atk, mk, gm, gt, gf, gs, cm, cr, cd, fm, fr, fd, ff, w; };
        static const VP t[] = {
            { "Voce naturală",   80, -2, 1.5f, 2,  -18, 3, 12, 2,   0, 380, 35, 50,   0, .8f, 30,   0, .3f, 50, 30,  100 },
            { "Trap",            90, -3, 3, 4,     -24, 4, 6, 4,    18, 380, 35, 50,  0, .8f, 30,   0, .3f, 50, 30,  115 },
            { "Rap",            100, -3, 2.5f, 2.5f, -22, 4.5f, 8, 3.5f, 0, 380, 35, 50, 15, .6f, 25, 0, .3f, 50, 30, 105 },
            { "Rock alternativ",110, -2, 3.5f, 1.5f, -20, 5, 3, 4,  15, 300, 30, 60,  25, .5f, 35,  0, .3f, 50, 30,  125 },
            { "Pop",             90, -2, 2, 5,     -20, 3, 10, 3,   0, 380, 35, 50,  20, .7f, 30,   0, .3f, 50, 30,  130 },
            { "Osamason",       120, -4, 4, 5,     -28, 8, 2, 7,    30, 250, 50, 70,  40, 1.2f, 55, 25, .25f, 50, 30, 160 },
            { "Nettspend",      100, -3, 3, 6,     -26, 6, 4, 6,    25, 420, 45, 75,  45, .9f, 50,   0, .3f, 50, 30,  150 },
            { "Che",            110, -3, 3.5f, 4,  -26, 6, 3, 6,    22, 300, 40, 60,  25, .7f, 40,  20, .35f, 45, 20, 140 },
            { "Ken Carson",     130, -5, 4, 5,     -30, 10, 1.5f, 8, 28, 220, 55, 65, 40, 1.5f, 60, 35, .5f, 70, 45, 170 },
            { "Playboi Carti",  120, -4, 3, 6,     -28, 8, 2, 7,    35, 330, 55, 80,  35, 1.f, 45,   0, .3f, 50, 30,  150 },
        };
        std::vector<Preset> out;
        for (auto& p : t)
            out.push_back ({ p.n, { { "hpf", p.hpf }, { "mud", p.mud }, { "pres", p.pres }, { "air", p.air },
                { "thr", p.thr }, { "ratio", p.ratio }, { "atk", p.atk }, { "makeup", p.mk },
                { "ghost_on", p.gm > 0 ? 1.f : 0.f }, { "g_mix", p.gm }, { "g_time", p.gt }, { "g_fb", p.gf }, { "g_space", p.gs },
                { "chor_on", p.cm > 0 ? 1.f : 0.f }, { "c_rate", p.cr }, { "c_depth", p.cd }, { "c_mix", p.cm },
                { "fl_on", p.fm > 0 ? 1.f : 0.f }, { "f_rate", p.fr }, { "f_depth", p.fd }, { "f_fb", p.ff }, { "f_mix", p.fm },
                { "width", p.w } } });
        return out;
    }

    void prepareToPlay (double sr, int bs) override
    {
        sampleRate = sr;
        dsp::ProcessSpec s { sr, (uint32) bs, 2 };
        comp.prepare (s); chorus.prepare (s); flanger.prepare (s); limiter.prepare (s);
        chorus.reset(); flanger.reset(); comp.reset(); limiter.reset();
        for (auto* f : { &hpA, &hpB, &mud, &pres, &air, &ghHP, &ghLP }) f->reset();
        ghHP.set (Filt::Co::makeHighPass (sr, 300.f, 0.707f));
        ghLP.set (Filt::Co::makeLowPass (sr, 4500.f, 0.707f));
        int size = (int) (1.7 * sr) + 8;
        for (auto& b : gb) b.assign ((size_t) size, 0.f);
        wp = 0; env = 0.f;
        ghostBuf.setSize (2, bs);
        rv.setSampleRate (sr); rv.reset();
    }

    void processBlock (AudioBuffer<float>& buf, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        const int n = buf.getNumSamples();
        if (buf.getNumChannels() < 2 || n == 0) return;
        float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };

        if (on ("eq_on"))
        {
            auto hp = Filt::Co::makeHighPass (sampleRate, P ("hpf"), 0.707f);
            hpA.set (hp); hpB.set (hp);
            mud.set (Filt::Co::makePeakFilter (sampleRate, 300.f, 1.f, Decibels::decibelsToGain (P ("mud"))));
            pres.set (Filt::Co::makePeakFilter (sampleRate, 3000.f, 0.9f, Decibels::decibelsToGain (P ("pres"))));
            air.set (Filt::Co::makeHighShelf (sampleRate, 10000.f, 0.7f, Decibels::decibelsToGain (P ("air"))));
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float s = ch[c][i];
                    s = hpA.f[c].processSample (s); s = hpB.f[c].processSample (s);
                    s = mud.f[c].processSample (s); s = pres.f[c].processSample (s); s = air.f[c].processSample (s);
                    ch[c][i] = s;
                }
        }

        if (on ("comp_on"))
        {
            comp.setThreshold (P ("thr")); comp.setRatio (P ("ratio"));
            comp.setAttack (P ("atk")); comp.setRelease (120.f);
            dsp::AudioBlock<float> blk (buf);
            comp.process (dsp::ProcessContextReplacing<float> (blk));
            buf.applyGain (Decibels::decibelsToGain (P ("makeup")));
        }

        if (on ("ghost_on"))
        {
            const int size = (int) gb[0].size();
            const int d = jlimit (1, size - 2, (int) (P ("g_time") * 0.001f * (float) sampleRate));
            const float fb = P ("g_fb") * 0.01f, space = P ("g_space") * 0.01f, mixAmt = P ("g_mix") * 0.012f;
            const float atkC = 1.f - std::exp (-1.f / (0.005f * (float) sampleRate));
            const float relC = 1.f - std::exp (-1.f / (0.35f * (float) sampleRate));
            ghostBuf.setSize (2, n, false, false, true);
            float* gl = ghostBuf.getWritePointer (0);
            float* gr = ghostBuf.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                float inL = ch[0][i], inR = ch[1][i];
                float a = jmax (std::abs (inL), std::abs (inR));
                env += (a - env) * (a > env ? atkC : relC);
                float duck = 1.f - jlimit (0.f, 1.f, env * 6.f); // ecoul apare după cuvinte
                int rp = (wp - d + size) % size;
                float wl = ghLP.f[0].processSample (ghHP.f[0].processSample (gb[0][(size_t) rp]));
                float wr = ghLP.f[1].processSample (ghHP.f[1].processSample (gb[1][(size_t) rp]));
                gb[0][(size_t) wp] = inL + wr * fb;
                gb[1][(size_t) wp] = inR + wl * fb;
                gl[i] = wl * duck; gr[i] = wr * duck;
                wp = (wp + 1) % size;
            }
            Reverb::Parameters rp;
            rp.roomSize = 0.5f + 0.45f * space; rp.damping = 0.6f; rp.width = 1.f;
            rp.dryLevel = 0.5f * (1.f - 0.5f * space); rp.wetLevel = 0.3f * space; rp.freezeMode = 0.f;
            rv.setParameters (rp);
            rv.processStereo (gl, gr, n);
            for (int i = 0; i < n; ++i) { ch[0][i] += gl[i] * mixAmt; ch[1][i] += gr[i] * mixAmt; }
        }

        if (on ("chor_on"))
        {
            chorus.setRate (P ("c_rate")); chorus.setDepth (P ("c_depth") * 0.01f); chorus.setCentreDelay (7.f);
            chorus.setFeedback (0.f); chorus.setMix (P ("c_mix") * 0.01f);
            dsp::AudioBlock<float> blk (buf);
            chorus.process (dsp::ProcessContextReplacing<float> (blk));
        }
        if (on ("fl_on"))
        {
            flanger.setRate (P ("f_rate")); flanger.setDepth (P ("f_depth") * 0.01f); flanger.setCentreDelay (2.f);
            flanger.setFeedback (P ("f_fb") * 0.01f); flanger.setMix (P ("f_mix") * 0.01f);
            dsp::AudioBlock<float> blk (buf);
            flanger.process (dsp::ProcessContextReplacing<float> (blk));
        }

        if (on ("st_on"))
        {
            const float w = P ("width") * 0.01f;
            for (int i = 0; i < n; ++i)
            {
                float m = (ch[0][i] + ch[1][i]) * 0.5f, s = (ch[0][i] - ch[1][i]) * 0.5f * w;
                ch[0][i] = m + s; ch[1][i] = m - s;
            }
        }

        buf.applyGain (Decibels::decibelsToGain (P ("out")));
        limiter.setThreshold (P ("ceil")); limiter.setRelease (80.f);
        dsp::AudioBlock<float> blk (buf);
        limiter.process (dsp::ProcessContextReplacing<float> (blk));
    }

    AudioProcessorEditor* createEditor() override
    {
        return new Editor (*this, Colour (0xff9b6bff), "Voce", {
            { U ("EQ"), "eq_on", { "hpf", "mud", "pres", "air" } },
            { U ("Compresor"), "comp_on", { "thr", "ratio", "atk", "makeup" } },
            { U ("Ecou fantomă"), "ghost_on", { "g_mix", "g_time", "g_fb", "g_space" } },
            { U ("Chorus"), "chor_on", { "c_rate", "c_depth", "c_mix" } },
            { U ("Flanger"), "fl_on", { "f_rate", "f_depth", "f_fb", "f_mix" } },
            { U ("Stereo și ieșire"), "st_on", { "width", "out", "ceil" } } }, 3);
    }

private:
    Filt hpA, hpB, mud, pres, air, ghHP, ghLP;
    dsp::Compressor<float> comp;
    dsp::Chorus<float> chorus, flanger;
    dsp::Limiter<float> limiter;
    Reverb rv;
    std::vector<float> gb[2];
    AudioBuffer<float> ghostBuf;
    int wp = 0;
    float env = 0.f;
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoiceProcessor(); }
