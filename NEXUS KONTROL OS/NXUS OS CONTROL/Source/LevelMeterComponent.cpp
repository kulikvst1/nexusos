#include "LevelMeterComponent.h"

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz(30);
    scaleMarks = { -60.0f, -20.0f, -12.0f, -6.0f, -3.0f, 0.0f };
}

LevelMeterComponent::~LevelMeterComponent()
{
    stopTimer();
}

void LevelMeterComponent::setLevel(float newLevel) noexcept
{
    levelLinear = juce::jlimit(0.0f, 1.0f, newLevel);
}

void LevelMeterComponent::setScaleMarks(const std::vector<float>& marks) noexcept
{
    scaleMarks = marks;
}

void LevelMeterComponent::timerCallback()
{
    repaint();
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    // ���
    auto fullArea = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::darkgrey.darker(0.2f));

    if (fullArea.isEmpty())
        return;

    // ������� ��������� ������ � �� (������� minDb)
    float levelDb = juce::Decibels::gainToDecibels(levelLinear, minDb);

    // ��������� Y-������� ��� 0 dB � ��� �������� ������
    float yZero = juce::jmap<float>(0.0f, maxDb, minDb, fullArea.getY(), fullArea.getBottom());
    float yLevel = juce::jmap<float>(levelDb, maxDb, minDb, fullArea.getY(), fullArea.getBottom());

    // 1) ������ ������� ������ ��� 0 dB
    if (yZero > fullArea.getY())
    {
        juce::Rectangle<float> redRect(
            fullArea.getX(),
            fullArea.getY(),
            fullArea.getWidth(),
            yZero - fullArea.getY());

        g.setColour(juce::Colours::red);
        g.fillRect(redRect);
    }

    // 2) ������ ������ ���� �� �������� ������ (��� �� 0 dB, ���� level ����) �� ����
    float startY = std::max(yLevel, yZero);  // ���� level > 0dB, ����� ������� �� yZero
    float fillH = fullArea.getBottom() - startY;

    juce::Rectangle<float> greenRect = fullArea.removeFromBottom(fillH);
    // removeFromBottom ���������� ������ ����� ������� fillH

    g.setColour(juce::Colours::green);
    g.fillRect(greenRect);

    // 3) ����� ����� (�� ����� ������)
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (auto markDb : scaleMarks)
    {
        float y = juce::jmap<float>(markDb, maxDb, minDb,
            getLocalBounds().getY(),
            getLocalBounds().getBottom());

        g.drawHorizontalLine(int(y),
            float(getLocalBounds().getX()),
            float(getLocalBounds().getRight()));
    }
}

