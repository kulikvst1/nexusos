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
        // Пропускаем –60 дБ
        if (dB <= minDb + 1e-6f)
            continue;

        float y = LevelUtils::mapDbToY(dB, maxDb, minDb, boundsF);

        // линия масштаба
        g.setColour(lineColour);
        g.drawHorizontalLine(int(y),
            boundsF.getX(),
            boundsF.getRight());

        // надпись
        juce::String txt = juce::String(int(std::round(dB)));
        if (showDbSuffix)
            txt << " dB";

        g.setColour(textColour);
        g.drawFittedText(txt,
            { int(boundsF.getX()) + textMargin,
              int(y - fontHeight * 0.5f),
              int(boundsF.getWidth()) - textMargin * 2,
              int(fontHeight) },
            juce::Justification::centred, 1);
    }
}
