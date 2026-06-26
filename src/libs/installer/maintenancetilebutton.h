#pragma once
#include <QPainter>
#include <QStyleFactory>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QtSvg/QSvgRenderer>
#include <QDebug>
#include <QMouseEvent>

/**
 * @brief Selectable tile button unit that handles additional resize events to ensure smooth dynamic button resizing
 */
class MaintenanceTileButton final : public QRadioButton
{
    Q_OBJECT
public:
    explicit MaintenanceTileButton(const QString& defaultSvgPath, const QString& hoverSvgPath, const QString& selectedSvgPath, QWidget *parent = nullptr);
    ~MaintenanceTileButton();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QSvgRenderer m_defaultRenderer;
    QSvgRenderer m_hoverRenderer;
    QSvgRenderer m_selectedRenderer;
    const qreal m_targetRatio;
    bool m_isHovered = false;
};

