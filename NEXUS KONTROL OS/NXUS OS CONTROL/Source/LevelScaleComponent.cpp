#include "LevelScaleComponent.h"

LevelScaleComponent::LevelScaleComponent()
{
    // базовый набор меток студийного уровня
    scaleMarks = { -60.0f, -30.0f, -20.0f, -12.0f, -6.0f, -3.0f, 0.0f };
    // на всякий случай убедимся, что −30 dB присутствует и отсортируем
    ensureThirtyDbMark();
}

void LevelScaleComponent::setScaleMarks(const std::vector<float>& marks) noexcept
{
    scaleMarks = marks;
    ensureThirtyDbMark();
    repaint();
}

void LevelScaleComponent::ensureThirtyDbMark() noexcept
{
    // если нет −30, добавляем
    if (std::find(scaleMarks.begin(), scaleMarks.end(), -30.0f) == scaleMarks.end())
        scaleMarks.push_back(-30.0f);

    // сортируем по убыванию, чтобы jmap корректно рассчитывал Y
    std::sort(scaleMarks.begin(), scaleMarks.end(), std::greater<float>());
}
void LevelScaleComponent::paint(juce::Graphics& g)
{
    // 1) Заливаем фон прозрачным чёрным
    auto boundsF = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::transparentBlack);

    // Проверка на валидный размер
    if (boundsF.getWidth() <= 0 || boundsF.getHeight() <= 0)
        return;

    // 2) Настраиваем шрифт (≈6% от высоты компонента)
    float fontHeight = boundsF.getHeight() * 0.06f;
    g.setFont(juce::Font(fontHeight, juce::Font::bold));

    // 3) Рисуем каждую метку из scaleMarks
    for (auto dB : scaleMarks)
    {
        // Преобразуем dB в экранную Y-координату
        float y = juce::jmap<float>(dB, maxDb, minDb,
            boundsF.getY(),
            boundsF.getBottom());

        // 3a) Линия масштаба
        g.setColour(lineColour);
        g.drawHorizontalLine(int(y),
            boundsF.getX(),
            boundsF.getRight());

        // 3b) Надпись "XX dB"
        juce::String txt = juce::String(int(std::round(dB))) + " dB";
        juce::Rectangle<int> textArea{
            int(boundsF.getX()) + textMargin,
            int(y - fontHeight * 0.5f),
            int(boundsF.getWidth()) - textMargin * 2,
            int(fontHeight)
        };

        g.setColour(textColour);
        g.drawFittedText(txt, textArea,
            juce::Justification::centred, 1);
    }
}
