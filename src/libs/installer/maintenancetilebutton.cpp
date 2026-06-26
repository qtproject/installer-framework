#include "maintenancetilebutton.h"

MaintenanceTileButton::MaintenanceTileButton(const QString& defaultSvgPath, const QString& hoverSvgPath, const QString& selectedSvgPath, QWidget *parent)
    : QRadioButton(parent)
    , m_defaultRenderer()
    , m_hoverRenderer()
    , m_selectedRenderer()
    , m_targetRatio(57.0 / 50.0) // Based on: https://www.figma.com/design/XXFrRUcdJZ1kYtG7axuwJm/QAWeb-UX?node-id=3900-2629&p=f&t=S0go3ENu90GcymBs-0
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);

    m_defaultRenderer.load(defaultSvgPath);
    m_hoverRenderer.load(hoverSvgPath);
    m_selectedRenderer.load(selectedSvgPath);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

MaintenanceTileButton::~MaintenanceTileButton()
{

}

void MaintenanceTileButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int widgetW = width();
    const int widgetH = height();

    if (widgetW <= 0 || widgetH <= 0)
        return;

    int targetW = widgetW;
    int targetH = static_cast<int>(targetW * m_targetRatio);

    if (targetH > widgetH)
    {
        targetH = widgetH;
        targetW = static_cast<int>(targetH / m_targetRatio);
    }

    int targetX = (widgetW - targetW) / 2;
    int targetY = (widgetH - targetH) / 2;
    QRect renderRect(targetX, targetY, targetW, targetH);

    bool isSelected = isChecked();
    bool isHovered = renderRect.contains(mapFromGlobal(QCursor::pos()));

    QSvgRenderer* activeRenderer = &m_defaultRenderer;
    if (isSelected)
    {
        activeRenderer = &m_selectedRenderer;
    }
    else if (isHovered)
    {
        activeRenderer = &m_hoverRenderer;
    }

    activeRenderer->render(&painter, renderRect);
}

void MaintenanceTileButton::mouseMoveEvent(QMouseEvent *event)
{
    update();
    QRadioButton::mouseMoveEvent(event);
}