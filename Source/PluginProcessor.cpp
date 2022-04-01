#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DistortionAudioProcessor::DistortionAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameters()) // returns a ParameterLayout object to use
#endif
{
    apvts.addParameterListener("GAIN", this);
    apvts.addParameterListener("DRIVE", this);
    apvts.addParameterListener("MIX", this);
    apvts.addParameterListener("OUTPUT", this);
}

DistortionAudioProcessor::~DistortionAudioProcessor()
{
    apvts.removeParameterListener("GAIN", this);
    apvts.removeParameterListener("DRIVE", this);
    apvts.removeParameterListener("MIX", this);
    apvts.removeParameterListener("OUTPUT", this);
}

//==============================================================================
/* Implement this function to give us our parameter layout*/
juce::AudioProcessorValueTreeState::ParameterLayout DistortionAudioProcessor::createParameters()
{
    // A vector of all the ranged audio parameters
    // Smart Pointers of the parameters
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    auto inputParam = std::make_unique<juce::AudioParameterFloat>("GAIN", "Gain", -24.0f, 24.0f, 0.0f);
    auto driveParam = std::make_unique<juce::AudioParameterFloat>("DRIVE", "driveDB", 0.0f, 20.0f, 0.0f);
    auto mixParam = std::make_unique<juce::AudioParameterFloat>("MIX", "mix", 0.0f, 100, 0.0f);
    auto outputParam = std::make_unique<juce::AudioParameterFloat>("OUTPUT", "outputDB", -24.0f, 24.0f, 0.0f);
    
    /* Use the push_back method since it's a vector.
    We have to allocate memory for this unique_pointer.
    AudioParameterFloat IS a type of RangedAudioParameter.
    Give it all its initialization arugments (see docs) */
    params.push_back(std::move(inputParam));
    params.push_back(std::move(driveParam));
    params.push_back(std::move(mixParam));
    params.push_back(std::move(outputParam));
    
    // Return iterator of the vector using .begin() and .end() member functions
    return { params.begin(), params.end() };
}

void DistortionAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "GAIN")
    {
        inputDB = newValue;
    }
    
    if (parameterID == "DRIVE")
    {
        driveDB = newValue;
    }

    if (parameterID == "MIX")
    {
        mix = static_cast<float>(newValue) / 100.0;
    }

    if (parameterID == "OUTPUT")
    {
        outputDB = newValue;
    }
}

const juce::String DistortionAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistortionAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DistortionAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DistortionAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DistortionAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DistortionAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DistortionAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistortionAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DistortionAudioProcessor::getProgramName (int index)
{
    return {};
}

void DistortionAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DistortionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void DistortionAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistortionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DistortionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Creating an Audio Block.
    juce::dsp::AudioBlock<float> block (buffer);
    
    for(int channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* channelData = block.getChannelPointer(channel);
        
        for(int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto input = channelData[sample] * juce::Decibels::decibelsToGain(inputDB);
            
            const auto softClip = piDivisor * std::atanf(input * juce::Decibels::decibelsToGain(driveDB));
            
            auto blend = input * (1.0 - mix) + softClip * mix;
            
            blend *= juce::Decibels::decibelsToGain(outputDB);
            
            channelData[sample] = blend;
        }
    }
}

//==============================================================================
bool DistortionAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DistortionAudioProcessor::createEditor()
{
    return new DistortionAudioProcessorEditor (*this);
}

//==============================================================================
void DistortionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void DistortionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionAudioProcessor();
}
