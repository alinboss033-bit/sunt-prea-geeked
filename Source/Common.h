#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
using namespace juce;

namespace geeked
{
using APVTS = AudioProcessorValueTreeState;
inline String U (const char* s) { return String::fromUTF8 (s); }

inline std::unique_ptr<AudioParameterFloat> F (const char* id, const char* name, float lo, float hi,
                                               float def, const char* unit = "", float skew = 1.f)
{
    return std::make_unique<AudioParameterFloat> (
        ParameterID { id, 1 }, U (name), NormalisableRange<float> (lo, hi, 0.01f, skew), def,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [u = U (unit)] (float v, int) { return String (v, 1) + (u.isEmpty() ? String() : " " + u); }));
}
inline std::unique_ptr<AudioParameterBool> B (const char* id, const char* name, bool def)
{
    return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, U (name), def);
}
inline std::unique_ptr<AudioParameterChoice> C (const char* id, const char* name, StringArray items, int def)
{
    return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, U (name), items, def);
}

struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };

inline void applyPreset (APVTS& a, const Preset& p)
{
    for (auto* par : a.processor.getParameters())
        par->setValueNotifyingHost (par->getDefaultValue());
    for (auto& kv : p.v)
        if (auto* par = a.getParameter (kv.first))
            par->setValueNotifyingHost (par->convertTo0to1 (kv.second));
}

// Filtru biquad stereo
struct Filt
{
    using Co = dsp::IIR::Coefficients<float>;
    dsp::IIR::Filter<float> f[2];
    void set (Co::Ptr c) { f[0].coefficients = c; f[1].coefficients = c; }
    void reset() { f[0].reset(); f[1].reset(); }
};

// Baza comună: parametri, preseturi (programe), stare
class Base : public AudioProcessor
{
public:
    APVTS apvts;
    std::vector<Preset> presets;
    int cur = 0;
    double sampleRate = 44100.0;

    Base (APVTS::ParameterLayout l, std::vector<Preset> p)
        : AudioProcessor (BusesProperties().withInput ("In", AudioChannelSet::stereo(), true)
                                           .withOutput ("Out", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "STATE", std::move (l)), presets (std::move (p)) {}

    float P (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return P (id) > 0.5f; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }
    bool hasEditor() const override { return true; }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == AudioChannelSet::stereo();
    }
    int getNumPrograms() override { return (int) presets.size(); }
    int getCurrentProgram() override { return cur; }
    void setCurrentProgram (int i) override { cur = jlimit (0, getNumPrograms() - 1, i); applyPreset (apvts, presets[(size_t) cur]); }
    const String getProgramName (int i) override { return U (presets[(size_t) i].name); }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock& d) override
    {
        if (auto x = apvts.copyState().createXml()) copyXmlToBinary (*x, d);
    }
    void setStateInformation (const void* d, int n) override
    {
        if (auto x = getXmlFromBinary (d, n))
            if (x->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*x));
    }
};

// Aspect: butoane rotative luminoase, butoane rotunde on/off
struct Look : LookAndFeel_V4
{
    Colour accent;
    explicit Look (Colour c) : accent (c)
    {
        setColour (Slider::textBoxTextColourId, Colours::white.withAlpha (0.85f));
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Label::textColourId, Colours::white.withAlpha (0.65f));
        setColour (ComboBox::backgroundColourId, Colour (0xff17132a));
        setColour (ComboBox::textColourId, Colours::white);
        setColour (ComboBox::outlineColourId, c.withAlpha (0.5f));
        setColour (ComboBox::arrowColourId, c);
        setColour (PopupMenu::backgroundColourId, Colour (0xff17132a));
        setColour (PopupMenu::textColourId, Colours::white);
        setColour (PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.4f));
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.f);
        float r = jmin (b.getWidth(), b.getHeight()) / 2.f;
        auto c = b.getCentre();
        PathStrokeType thin (4.f, PathStrokeType::curved, PathStrokeType::rounded);
        PathStrokeType glow (10.f, PathStrokeType::curved, PathStrokeType::rounded);

        Path track; track.addCentredArc (c.x, c.y, r, r, 0.f, a0, a1, true);
        g.setColour (Colour (0xff2a2540)); g.strokePath (track, thin);

        float ang = a0 + pos * (a1 - a0);
        Path val; val.addCentredArc (c.x, c.y, r, r, 0.f, a0, ang, true);
        g.setColour (accent.withAlpha (0.25f)); g.strokePath (val, glow);
        g.setColour (accent); g.strokePath (val, thin);

        float rb = r * 0.74f;
        g.setGradientFill (ColourGradient (Colour (0xff35304f), c.x, c.y - rb, Colour (0xff0b0914), c.x, c.y + rb, false));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f, 1.f);

        Path p; p.addRoundedRectangle (-1.8f, -rb + 3.f, 3.6f, rb * 0.42f, 1.8f);
        g.setColour (Colours::white);
        g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (Graphics& g, ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (3.f);
        float d = jmin (r.getWidth(), r.getHeight());
        auto circ = Rectangle<float> (d, d).withCentre (r.getCentre());
        bool isOn = b.getToggleState();
        if (isOn) { g.setColour (accent.withAlpha (0.3f)); g.fillEllipse (circ.expanded (3.f)); }
        g.setColour (isOn ? accent : Colour (0xff3a3555));
        g.fillEllipse (circ);
        g.setColour (Colour (0xff0b0914).withAlpha (hover ? 0.5f : 0.75f));
        g.fillEllipse (circ.reduced (d * 0.3f));
    }
};

struct Group { String title; String power; std::vector<String> knobs; };

class Editor : public AudioProcessorEditor
{
public:
    Editor (Base& p, Colour acc, const char* sub, std::vector<Group> groups, int columns)
        : AudioProcessorEditor (p), proc (p), look (acc), accent (acc), subtitle (U (sub)), cols (columns)
    {
        setLookAndFeel (&look);
        addAndMakeVisible (presetBox);
        for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
        presetBox.setSelectedItemIndex (p.getCurrentProgram(), dontSendNotification);
        presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };

        for (auto& gr : groups)
        {
            auto pn = std::make_unique<Panel>();
            pn->grp = gr;
            if (gr.power.isNotEmpty())
            {
                pn->t = std::make_unique<ToggleButton>();
                addAndMakeVisible (*pn->t);
                pn->ta = std::make_unique<APVTS::ButtonAttachment> (p.apvts, gr.power, *pn->t);
            }
            for (auto& id : gr.knobs)
            {
                auto c = std::make_unique<Cell>();
                auto* par = p.apvts.getParameter (id);
                c->l.setText (par->getName (24), dontSendNotification);
                c->l.setJustificationType (Justification::centred);
                c->l.setFont (Font (13.f));
                addAndMakeVisible (c->l);
                if (auto* ch = dynamic_cast<AudioParameterChoice*> (par))
                {
                    c->c = std::make_unique<ComboBox>();
                    c->c->addItemList (ch->choices, 1);
                    addAndMakeVisible (*c->c);
                    c->ca = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, id, *c->c);
                }
                else
                {
                    c->s = std::make_unique<Slider>();
                    c->s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
                    c->s->setTextBoxStyle (Slider::TextBoxBelow, false, 76, 18);
                    c->s->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
                    addAndMakeVisible (*c->s);
                    c->sa = std::make_unique<APVTS::SliderAttachment> (p.apvts, id, *c->s);
                }
                pn->cells.push_back (std::move (c));
            }
            panels.push_back (std::move (pn));
        }
        int rows = ((int) panels.size() + cols - 1) / cols;
        setSize (cols * 380 + 20, 90 + rows * 190 + 10);
    }
    ~Editor() override { setLookAndFeel (nullptr); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff06050d), 0.f, 0.f, Colour (0xff1a1233), (float) getWidth(), (float) getHeight(), false));
        g.fillAll();
        g.setColour (accent.withAlpha (0.07f));
        g.fillEllipse (-140.f, -180.f, 480.f, 360.f);
        g.setColour (Colours::white);
        g.setFont (Font (32.f, Font::bold));
        g.drawText ("Sunt prea geeked", 22, 16, 420, 40, Justification::centredLeft);
        g.setColour (accent);
        g.setFont (Font (15.f));
        g.drawText (subtitle, 24, 56, 300, 20, Justification::centredLeft);
        for (int i = 0; i < (int) panels.size(); ++i)
        {
            auto r = panelRect (i).toFloat();
            g.setColour (Colour (0xff110e1c)); g.fillRoundedRectangle (r, 14.f);
            g.setColour (accent.withAlpha (0.25f)); g.drawRoundedRectangle (r, 14.f, 1.f);
            g.setColour (Colours::white.withAlpha (0.9f)); g.setFont (Font (16.f, Font::bold));
            g.drawText (panels[(size_t) i]->grp.title, (int) r.getX() + 16, (int) r.getY() + 8, 220, 26, Justification::centredLeft);
        }
    }

    void resized() override
    {
        presetBox.setBounds (getWidth() - 260, 28, 240, 30);
        for (int i = 0; i < (int) panels.size(); ++i)
        {
            auto r = panelRect (i);
            auto& pn = *panels[(size_t) i];
            if (pn.t) pn.t->setBounds (r.getRight() - 38, r.getY() + 8, 28, 28);
            auto area = r.withTrimmedTop (44).reduced (6, 0);
            int n = (int) pn.cells.size();
            int cw = jmin (96, area.getWidth() / jmax (n, 1));
            int x0 = area.getX() + (area.getWidth() - cw * n) / 2;
            for (int k = 0; k < n; ++k)
            {
                auto& c = *pn.cells[(size_t) k];
                auto cell = Rectangle<int> (x0 + k * cw, area.getY(), cw, area.getHeight());
                c.l.setBounds (cell.removeFromTop (18));
                if (c.s) c.s->setBounds (cell);
                else c.c->setBounds (cell.withSizeKeepingCentre (cw - 8, 28).translated (0, -14));
            }
        }
    }

private:
    struct Cell
    {
        std::unique_ptr<Slider> s;
        std::unique_ptr<ComboBox> c;
        Label l;
        std::unique_ptr<APVTS::SliderAttachment> sa;
        std::unique_ptr<APVTS::ComboBoxAttachment> ca;
    };
    struct Panel
    {
        Group grp;
        std::unique_ptr<ToggleButton> t;
        std::unique_ptr<APVTS::ButtonAttachment> ta;
        std::vector<std::unique_ptr<Cell>> cells;
    };
    Rectangle<int> panelRect (int i) const
    {
        return Rectangle<int> (10 + (i % cols) * 380, 90 + (i / cols) * 190, 380, 190).reduced (8);
    }

    Base& proc;
    Look look;
    Colour accent;
    String subtitle;
    int cols;
    ComboBox presetBox;
    std::vector<std::unique_ptr<Panel>> panels;
};
} // namespace geeked
