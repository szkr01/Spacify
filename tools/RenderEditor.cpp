#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <iostream>

#include "PluginEditor.h"
#include "PluginProcessor.h"

/**
    Development-only tool: renders the editor off-screen and writes it to a PNG
    so the layout can be reviewed without loading the plug-in into a host.

    usage: SpacifyRenderEditor [output.png] [scale]
*/
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File outputFile = juce::File::getCurrentWorkingDirectory()
                                      .getChildFile (argc > 1 ? argv[1] : "spacify-ui.png");

    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 2.0f;

    spacify::SpacifyAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "createEditor() returned nullptr" << std::endl;
        return 1;
    }

    editor->setSize (spacify::SpacifyAudioProcessorEditor::editorWidth,
                     spacify::SpacifyAudioProcessorEditor::editorHeight);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

    juce::FileOutputStream stream (outputFile);

    if (! stream.openedOk())
    {
        std::cerr << "could not open " << outputFile.getFullPathName() << std::endl;
        return 1;
    }

    juce::PNGImageFormat png;
    const bool written = png.writeImageToStream (image, stream);

    std::cout << (written ? "wrote " : "FAILED to write ")
              << outputFile.getFullPathName() << "  (" << image.getWidth() << "x" << image.getHeight() << ")"
              << std::endl;

    return written ? 0 : 1;
}
