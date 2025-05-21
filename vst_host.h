#pragma once

#include <JuceHeader.h>

// ��������� ��� �������������� ���������� �������.
// �� �������� ��������� �� ����������� ������ � ������� ������ ��������� � ����� ��� ���������� �����������.
class PluginParameterEditorComponent : public juce::Component,
    public juce::Slider::Listener
{
public:
    PluginParameterEditorComponent(juce::AudioPluginInstance* plugin)
        : pluginInstance(plugin)
    {
        if (pluginInstance != nullptr)
        {
            auto& params = pluginInstance->getParameters();
            for (int i = 0; i < params.size(); ++i)
            {
                // ������� ������� ��� ���������� ��������� ��������� (�� 0 �� 1)
                auto* slider = new juce::Slider();
                slider->setRange(0.0, 1.0, 0.001);
                slider->setValue(params[i]->getValue(), juce::dontSendNotification);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
                slider->addListener(this);
                addAndMakeVisible(slider);
                parameterSliders.add(slider);

                // ������� ����� ��� ����������� �������� ���������
                auto* label = new juce::Label();
                label->setText(params[i]->getName(100), juce::dontSendNotification);
                addAndMakeVisible(label);
                parameterLabels.add(label);
            }
            // ������ ���� ������� �� ���������� ����������
            setSize(400, params.size() * 40 + 20);
        }
    }

    ~PluginParameterEditorComponent() override {}

    // ��� ��������� �������� ��������� ��������������� �������� �������
    void sliderValueChanged(juce::Slider* slider) override
    {
        auto& params = pluginInstance->getParameters();
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            if (parameterSliders[i] == slider)
            {
                float newValue = static_cast<float>(slider->getValue());
                if (pluginInstance != nullptr)
                    pluginInstance->setParameterNotifyingHost(i, newValue);
                break;
            }
        }
    }

    void resized() override
    {
        int y = 10;
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            auto area = juce::Rectangle<int>(10, y, getWidth() - 20, 30);
            parameterLabels[i]->setBounds(area.removeFromLeft(120));
            parameterSliders[i]->setBounds(area);
            y += 40;
        }
    }

private:
    juce::AudioPluginInstance* pluginInstance;
    juce::OwnedArray<juce::Slider> parameterSliders;
    juce::OwnedArray<juce::Label> parameterLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginParameterEditorComponent)
};

class VSTHostComponent : public juce::Component,
    public juce::ListBoxModel,
    public juce::Button::Listener
{
public:
    VSTHostComponent()
    {
        // ������������ ��������� ������������ �������� (VST, AU, AAX � �.�.)
        formatManager.addDefaultFormats();

#ifdef JUCE_WINDOWS
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#else
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#endif

        if (pluginDirectory.exists())
        {
            // �� Windows ���� .vst3 ����� (�� macOS ����� ���� .vst � .component)
            pluginDirectory.findChildFiles(pluginFiles, juce::File::findFiles, true, "*.vst3");
        }

        // ����������� ListBox ��� ����������� ��������� ��������
        addAndMakeVisible(pluginListBox);
        pluginListBox.setModel(this);

        // ������ �������� �������
        loadButton.setButtonText("Load Plugin");
        loadButton.addListener(this);
        addAndMakeVisible(loadButton);

        // ������ �������� �������
        closeButton.setButtonText("Close Plugin");
        closeButton.addListener(this);
        addAndMakeVisible(closeButton);
        closeButton.setVisible(false);

        // ������ ��� �������� ���� �������������� ���������� �������
        parametersButton.setButtonText("Edit Parameters");
        parametersButton.addListener(this);
        addAndMakeVisible(parametersButton);
        parametersButton.setVisible(false);
    }

    ~VSTHostComponent() override
    {
        unloadPlugin();
    }

    // --- ����� ����� ��� ���������� ����������� ������� �������� ����� �������� CC --- 
    // ��� ������ � �������� ccNumber ������������ ������ ��������� (��������, 0 ��� ������� ���������),
    // � ccValue (� ��������� 0..127) ������������� �� ��������� 0.0..1.0.
    void setPluginParameterFromCC(int ccNumber, int ccValue)
    {
        if (pluginInstance != nullptr)
        {
            auto& params = pluginInstance->getParameters();
            if (ccNumber >= 0 && ccNumber < params.size())
            {
                float normalizedValue = static_cast<float>(ccValue) / 127.f;
                pluginInstance->setParameterNotifyingHost(ccNumber, normalizedValue);
            }
        }
    }
    // ListBoxModel ���������
    int getNumRows() override { return pluginFiles.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g,
        int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < pluginFiles.size())
        {
            if (rowIsSelected)
                g.fillAll(juce::Colours::lightblue);

            g.setColour(juce::Colours::black);
            g.drawText(pluginFiles[rowNumber].getFileName(), 2, 0, width - 4, height,
                juce::Justification::centredLeft);
        }
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        selectedIndex = row;
    }

    // ��������� ������� ������
    void buttonClicked(juce::Button* button) override
    {
        if (button == &loadButton)
        {
            if (selectedIndex >= 0 && selectedIndex < pluginFiles.size())
                loadPlugin(pluginFiles[selectedIndex], currentSampleRate, currentBlockSize);
        }
        else if (button == &closeButton)
        {
            unloadPlugin();
            repaint();
        }
        else if (button == &parametersButton)
        {
            // ���� ������ ��������, ��������� ��������� ���� � �����������
            if (pluginInstance != nullptr)
            {
                auto* paramEditor = new PluginParameterEditorComponent(pluginInstance.get());
                // ���������� ����� ����� showDialog ������ ����������� launchAsModalDialog.
                juce::DialogWindow::showDialog("Plugin Parameters",
                    paramEditor,
                    nullptr,
                    juce::Colours::lightgrey,
                    true);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Info",
                    "Plugin is not loaded!");
            }
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();

        if (pluginInstance != nullptr && pluginEditor != nullptr)
        {
            auto topBar = r.removeFromTop(30);
            closeButton.setBounds(topBar.removeFromLeft(100).reduced(4));
            parametersButton.setBounds(topBar.removeFromLeft(150).reduced(4)); // ������ ����������
            pluginEditor->setBounds(r.reduced(4));
        }
        else
        {
            auto listArea = r.removeFromTop(r.getHeight() - 40);
            pluginListBox.setBounds(listArea.reduced(4));
            auto buttonArea = r;
            loadButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(4));
            parametersButton.setVisible(false);
        }
    }

    // ������ �������� sample rate � block size, ������� ����� �������������� ��� �������� �������.
    void setAudioSettings(double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
    }

    // �������� ������� �� �����
    void loadPlugin(const juce::File& pluginFile, double sampleRate, int blockSize)
    {
        unloadPlugin();

        juce::AudioPluginFormat* chosenFormat = nullptr;
        juce::OwnedArray<juce::PluginDescription> pluginDescs;

        for (auto* format : formatManager.getFormats())
        {
            format->findAllTypesForFile(pluginDescs, pluginFile.getFullPathName());
            if (pluginDescs.size() > 0)
            {
                chosenFormat = format;
                break;
            }
        }

        if (chosenFormat == nullptr || pluginDescs.size() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error",
                "No plugin description found in the file.");
            return;
        }

        juce::PluginDescription desc = *pluginDescs[0];
        juce::String error;

        pluginInstance = chosenFormat->createInstanceFromDescription(desc, sampleRate, blockSize, error);
        if (pluginInstance == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error",
                "Failed to create plugin instance:\n" + error);
            return;
        }

        // ����������� �����/������ ������� (� ������ ������� ���������� 2 ������)
        int numInputs = 2;
        int numOutputs = 2;
        pluginInstance->setPlayConfigDetails(numInputs, numOutputs, sampleRate, blockSize);
        DBG(juce::String::formatted("Plugin setPlayConfigDetails called with numInputs = %d, numOutputs = %d, sampleRate = %f, blockSize = %d",
            numInputs, numOutputs, sampleRate, blockSize));

        // �������� prepareToPlay
        pluginInstance->prepareToPlay(currentSampleRate, currentBlockSize);
        DBG(juce::String::formatted("Plugin prepareToPlay called with sampleRate = %f, blockSize = %d",
            currentSampleRate, currentBlockSize));

        // ������� �������� �������, ���� �� ������������
        pluginEditor = pluginInstance->createEditor();
        if (pluginEditor != nullptr)
        {
            addAndMakeVisible(pluginEditor);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Info",
                "Plugin does not provide an editor.");
        }

        pluginListBox.setVisible(false);
        loadButton.setVisible(false);
        closeButton.setVisible(true);
        // ����� �������� ������� ������ ������ �������������� ���������� �������
        parametersButton.setVisible(true);
        resized();
    }

    // ���������� ��������� �� ����������� ������
    juce::AudioPluginInstance* getPluginInstance() { return pluginInstance.get(); }

private:
    // �������� �������
    void unloadPlugin()
    {
        if (pluginEditor != nullptr)
        {
            removeChildComponent(pluginEditor);
            delete pluginEditor;
            pluginEditor = nullptr;
        }
        pluginInstance.reset();
        pluginListBox.setVisible(true);
        loadButton.setVisible(true);
        closeButton.setVisible(false);
        parametersButton.setVisible(false);
    }

    juce::AudioPluginFormatManager formatManager;
    juce::ListBox pluginListBox;
    juce::TextButton loadButton;
    juce::TextButton closeButton;
    juce::TextButton parametersButton; // ������ ��� �������� ���� ����������
    juce::Array<juce::File> pluginFiles;
    int selectedIndex = -1;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::Component* pluginEditor = nullptr;

    double currentSampleRate = 44100;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
