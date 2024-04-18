// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "QAWizard.h"
// #include <QtWidgets/private/qtwidgetsglobal_p.h>

// #if QT_CONFIG(spinbox)
// #include "qabstractspinbox.h"
// #endif
// #include "qalgorithms.h"
// #include "qapplication.h"
// #include "qboxlayout.h"
// #include "qlayoutitem.h"
// #include "qevent.h"
// #include "qframe.h"
// #include "qlabel.h"
// #if QT_CONFIG(lineedit)
// #include "qlineedit.h"
// #endif
// #include <qpointer.h>
// #include "qstylepainter.h"
// #include "qwindow.h"
// #include "qpushbutton.h"
// #include "qset.h"
// #if QT_CONFIG(shortcut)
// #  include "qshortcut.h"
// #endif
// #include "qstyle.h"
// #include "qstyleoption.h"
// #include "qvarlengtharray.h"
// #if defined(Q_OS_MACOS)
// #include <AppKit/AppKit.h>
// #include <QtGui/private/qcoregraphics_p.h>
// #elif QT_CONFIG(style_windowsvista)
// #include "QAWizard_win_p.h"
// #include "qtimer.h"
// #endif

// #include "private/qdialog_p.h"
// #include <qdebug.h>

// #include <string.h>     // for memset()
// #include <algorithm>

// QT_BEGIN_NAMESPACE

// using namespace Qt::StringLiterals;

// // These fudge terms were needed a few places to obtain pixel-perfect results
// const int GapBetweenLogoAndRightEdge = 5;
// const int ModernHeaderTopMargin = 2;
// const int ClassicHMargin = 4;
// const int MacButtonTopMargin = 13;
// const int MacLayoutLeftMargin = 20;
// //const int MacLayoutTopMargin = 14; // Unused. Save some space and avoid warning.
// const int MacLayoutRightMargin = 20;
// const int MacLayoutBottomMargin = 17;

// static void changeSpacerSize(QLayout *layout, int index, int width, int height)
// {
//     QSpacerItem *spacer = layout->itemAt(index)->spacerItem();
//     if (!spacer)
//         return;
//     spacer->changeSize(width, height);
// }

// static QWidget *iWantTheFocus(QWidget *ancestor)
// {
//     const int MaxIterations = 100;

//     QWidget *candidate = ancestor;
//     for (int i = 0; i < MaxIterations; ++i) {
//         candidate = candidate->nextInFocusChain();
//         if (!candidate)
//             break;

//         if (candidate->focusPolicy() & Qt::TabFocus) {
//             if (candidate != ancestor && ancestor->isAncestorOf(candidate))
//                 return candidate;
//         }
//     }
//     return nullptr;
// }

// static bool objectInheritsXAndXIsCloserThanY(const QObject *object, const QByteArray &classX,
//                                              const QByteArray &classY)
// {
//     const QMetaObject *metaObject = object->metaObject();
//     while (metaObject) {
//         if (metaObject->className() == classX)
//             return true;
//         if (metaObject->className() == classY)
//             return false;
//         metaObject = metaObject->superClass();
//     }
//     return false;
// }

// const struct {
//     const char className[16];
//     const char property[13];
// } fallbackProperties[] = {
//     // If you modify this list, make sure to update the documentation (and the auto test)
//     { "QAbstractButton", "checked" },
//     { "QAbstractSlider", "value" },
//     { "QComboBox", "currentIndex" },
//     { "QDateTimeEdit", "dateTime" },
//     { "QLineEdit", "text" },
//     { "QListWidget", "currentRow" },
//     { "QSpinBox", "value" },
// };
// const size_t NFallbackDefaultProperties = sizeof fallbackProperties / sizeof *fallbackProperties;

// static const char *changed_signal(int which)
// {
//     // since it might expand to a runtime function call (to
//     // qFlagLocations()), we cannot store the result of SIGNAL() in a
//     // character array and expect it to be statically initialized. To
//     // avoid the relocations caused by a char pointer table, use a
//     // switch statement:
//     switch (which) {
//     case 0: return SIGNAL(toggled(bool));
//     case 1: return SIGNAL(valueChanged(int));
//     case 2: return SIGNAL(currentIndexChanged(int));
//     case 3: return SIGNAL(dateTimeChanged(QDateTime));
//     case 4: return SIGNAL(textChanged(QString));
//     case 5: return SIGNAL(currentRowChanged(int));
//     case 6: return SIGNAL(valueChanged(int));
//     };
//     static_assert(7 == NFallbackDefaultProperties);
//     Q_UNREACHABLE_RETURN(nullptr);
// }

// class QAWizardDefaultProperty
// {
// public:
//     QByteArray className;
//     QByteArray property;
//     QByteArray changedSignal;

//     inline QAWizardDefaultProperty() {}
//     inline QAWizardDefaultProperty(const char *className, const char *property,
//                                    const char *changedSignal)
//         : className(className), property(property), changedSignal(changedSignal) {}
// };
// Q_DECLARE_TYPEINFO(QAWizardDefaultProperty, Q_RELOCATABLE_TYPE);

// class QAWizardField
// {
// public:
//     inline QAWizardField() {}
//     QAWizardField(QAWizardPage *page, const QString &spec, QObject *object, const char *property,
//                   const char *changedSignal);

//     void resolve(const QList<QAWizardDefaultProperty> &defaultPropertyTable);
//     void findProperty(const QAWizardDefaultProperty *properties, int propertyCount);

//     QAWizardPage *page;
//     QString name;
//     bool mandatory;
//     QObject *object;
//     QByteArray property;
//     QByteArray changedSignal;
//     QVariant initialValue;
// };
// Q_DECLARE_TYPEINFO(QAWizardField, Q_RELOCATABLE_TYPE);

// QAWizardField::QAWizardField(QAWizardPage *page, const QString &spec, QObject *object,
//                            const char *property, const char *changedSignal)
//     : page(page), name(spec), mandatory(false), object(object), property(property),
//       changedSignal(changedSignal)
// {
//     if (name.endsWith(u'*')) {
//         name.chop(1);
//         mandatory = true;
//     }
// }

// void QAWizardField::resolve(const QList<QAWizardDefaultProperty> &defaultPropertyTable)
// {
//     if (property.isEmpty())
//         findProperty(defaultPropertyTable.constData(), defaultPropertyTable.size());
//     initialValue = object->property(property);
// }

// void QAWizardField::findProperty(const QAWizardDefaultProperty *properties, int propertyCount)
// {
//     QByteArray className;

//     for (int i = 0; i < propertyCount; ++i) {
//         if (objectInheritsXAndXIsCloserThanY(object, properties[i].className, className)) {
//             className = properties[i].className;
//             property = properties[i].property;
//             changedSignal = properties[i].changedSignal;
//         }
//     }
// }

// class QAWizardLayoutInfo
// {
// public:
//     int topLevelMarginLeft = -1;
//     int topLevelMarginRight = -1;
//     int topLevelMarginTop = -1;
//     int topLevelMarginBottom = -1;
//     int childMarginLeft = -1;
//     int childMarginRight = -1;
//     int childMarginTop = -1;
//     int childMarginBottom = -1;
//     int hspacing = -1;
//     int vspacing = -1;
//     int buttonSpacing = -1;
//     QAWizard::WizardStyle wizStyle = QAWizard::ClassicStyle;
//     bool header = false;
//     bool watermark = false;
//     bool title = false;
//     bool subTitle = false;
//     bool extension = false;
//     bool sideWidget = false;

//     bool operator==(const QAWizardLayoutInfo &other) const;
//     inline bool operator!=(const QAWizardLayoutInfo &other) const { return !operator==(other); }
// };

// bool QAWizardLayoutInfo::operator==(const QAWizardLayoutInfo &other) const
// {
//     return topLevelMarginLeft == other.topLevelMarginLeft
//            && topLevelMarginRight == other.topLevelMarginRight
//            && topLevelMarginTop == other.topLevelMarginTop
//            && topLevelMarginBottom == other.topLevelMarginBottom
//            && childMarginLeft == other.childMarginLeft
//            && childMarginRight == other.childMarginRight
//            && childMarginTop == other.childMarginTop
//            && childMarginBottom == other.childMarginBottom
//            && hspacing == other.hspacing
//            && vspacing == other.vspacing
//            && buttonSpacing == other.buttonSpacing
//            && wizStyle == other.wizStyle
//            && header == other.header
//            && watermark == other.watermark
//            && title == other.title
//            && subTitle == other.subTitle
//            && extension == other.extension
//            && sideWidget == other.sideWidget;
// }

// class QAWizardHeader : public QWidget
// {
// public:
//     enum RulerType { Ruler };

//     inline QAWizardHeader(RulerType /* ruler */, QWidget *parent = nullptr)
//         : QWidget(parent) { setFixedHeight(2); }
//     QAWizardHeader(QWidget *parent = nullptr);

//     void setup(const QAWizardLayoutInfo &info, const QString &title,
//                const QString &subTitle, const QPixmap &logo, const QPixmap &banner,
//                Qt::TextFormat titleFormat, Qt::TextFormat subTitleFormat);

// protected:
//     void paintEvent(QPaintEvent *event) override;
// #if QT_CONFIG(style_windowsvista)
// private:
//     bool vistaDisabled() const;
// #endif
// private:
//     QLabel *titleLabel;
//     QLabel *subTitleLabel;
//     QLabel *logoLabel;
//     QGridLayout *layout;
//     QPixmap bannerPixmap;
// };

// QAWizardHeader::QAWizardHeader(QWidget *parent)
//     : QWidget(parent)
// {
//     setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
//     setBackgroundRole(QPalette::Base);

//     titleLabel = new QLabel(this);
//     titleLabel->setBackgroundRole(QPalette::Base);

//     subTitleLabel = new QLabel(this);
//     subTitleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
//     subTitleLabel->setWordWrap(true);

//     logoLabel = new QLabel(this);

//     QFont font = titleLabel->font();
//     font.setBold(true);
//     titleLabel->setFont(font);

//     layout = new QGridLayout(this);
//     layout->setContentsMargins(QMargins());
//     layout->setSpacing(0);

//     layout->setRowMinimumHeight(3, 1);
//     layout->setRowStretch(4, 1);

//     layout->setColumnStretch(2, 1);
//     layout->setColumnMinimumWidth(4, 2 * GapBetweenLogoAndRightEdge);
//     layout->setColumnMinimumWidth(6, GapBetweenLogoAndRightEdge);

//     layout->addWidget(titleLabel, 2, 1, 1, 2);
//     layout->addWidget(subTitleLabel, 4, 2);
//     layout->addWidget(logoLabel, 1, 5, 5, 1);
// }

// #if QT_CONFIG(style_windowsvista)
// bool QAWizardHeader::vistaDisabled() const
// {
//     bool styleDisabled = false;
//     QAWizard *wiz = parentWidget() ? qobject_cast <QAWizard *>(parentWidget()->parentWidget()) : 0;
//     if (wiz) {
//         // Designer doesn't support the Vista style for Wizards. This property is used to turn
//         // off the Vista style.
//         const QVariant v = wiz->property("_q_wizard_vista_off");
//         styleDisabled = v.isValid() && v.toBool();
//     }
//     return styleDisabled;
// }
// #endif

// void QAWizardHeader::setup(const QAWizardLayoutInfo &info, const QString &title,
//                           const QString &subTitle, const QPixmap &logo, const QPixmap &banner,
//                           Qt::TextFormat titleFormat, Qt::TextFormat subTitleFormat)
// {
//     bool modern = ((info.wizStyle == QAWizard::ModernStyle)
// #if QT_CONFIG(style_windowsvista)
//         || vistaDisabled()
// #endif
//     );

//     layout->setRowMinimumHeight(0, modern ? ModernHeaderTopMargin : 0);
//     layout->setRowMinimumHeight(1, modern ? info.topLevelMarginTop - ModernHeaderTopMargin - 1 : 0);
//     layout->setRowMinimumHeight(6, (modern ? 3 : GapBetweenLogoAndRightEdge) + 2);

//     int minColumnWidth0 = modern ? info.topLevelMarginLeft + info.topLevelMarginRight : 0;
//     int minColumnWidth1 = modern ? info.topLevelMarginLeft + info.topLevelMarginRight + 1
//                                  : info.topLevelMarginLeft + ClassicHMargin;
//     layout->setColumnMinimumWidth(0, minColumnWidth0);
//     layout->setColumnMinimumWidth(1, minColumnWidth1);

//     titleLabel->setTextFormat(titleFormat);
//     titleLabel->setText(title);
//     logoLabel->setPixmap(logo);

//     subTitleLabel->setTextFormat(subTitleFormat);
//     subTitleLabel->setText("Pq\nPq"_L1);
//     int desiredSubTitleHeight = subTitleLabel->sizeHint().height();
//     subTitleLabel->setText(subTitle);

//     if (modern) {
//         bannerPixmap = banner;
//     } else {
//         bannerPixmap = QPixmap();
//     }

//     if (bannerPixmap.isNull()) {
//         /*
//             There is no widthForHeight() function, so we simulate it with a loop.
//         */
//         int candidateSubTitleWidth = qMin(512, 2 * QGuiApplication::primaryScreen()->virtualGeometry().width() / 3);
//         int delta = candidateSubTitleWidth >> 1;
//         while (delta > 0) {
//             if (subTitleLabel->heightForWidth(candidateSubTitleWidth - delta)
//                         <= desiredSubTitleHeight)
//                 candidateSubTitleWidth -= delta;
//             delta >>= 1;
//         }

//         subTitleLabel->setMinimumSize(candidateSubTitleWidth, desiredSubTitleHeight);

//         QSize size = layout->totalMinimumSize();
//         setMinimumSize(size);
//         setMaximumSize(QWIDGETSIZE_MAX, size.height());
//     } else {
//         subTitleLabel->setMinimumSize(0, 0);
//         setFixedSize(banner.size() + QSize(0, 2));
//     }
//     updateGeometry();
// }

// void QAWizardHeader::paintEvent(QPaintEvent * /* event */)
// {
//     QStylePainter painter(this);
//     painter.drawPixmap(0, 0, bannerPixmap);

//     int x = width() - 2;
//     int y = height() - 2;
//     const QPalette &pal = palette();
//     painter.setPen(pal.mid().color());
//     painter.drawLine(0, y, x, y);
//     painter.setPen(pal.base().color());
//     painter.drawPoint(x + 1, y);
//     painter.drawLine(0, y + 1, x + 1, y + 1);
// }

// // We save one vtable by basing QAWizardRuler on QAWizardHeader
// class QAWizardRuler : public QAWizardHeader
// {
// public:
//     inline QAWizardRuler(QWidget *parent = nullptr)
//         : QAWizardHeader(Ruler, parent) {}
// };

// class QWatermarkLabel : public QLabel
// {
// public:
//     QWatermarkLabel(QWidget *parent, QWidget *sideWidget) : QLabel(parent), m_sideWidget(sideWidget) {
//         m_layout = new QVBoxLayout(this);
//         if (m_sideWidget)
//             m_layout->addWidget(m_sideWidget);
//     }

//     QSize minimumSizeHint() const override {
//         if (!pixmap().isNull())
//             return pixmap().deviceIndependentSize().toSize();
//         return QFrame::minimumSizeHint();
//     }

//     void setSideWidget(QWidget *widget) {
//         if (m_sideWidget == widget)
//             return;
//         if (m_sideWidget) {
//             m_layout->removeWidget(m_sideWidget);
//             m_sideWidget->hide();
//         }
//         m_sideWidget = widget;
//         if (m_sideWidget)
//             m_layout->addWidget(m_sideWidget);
//     }
//     QWidget *sideWidget() const {
//         return m_sideWidget;
//     }
// private:
//     QVBoxLayout *m_layout;
//     QWidget *m_sideWidget;
// };

// class QAWizardPagePrivate : public QWidgetPrivate
// {
//     Q_DECLARE_PUBLIC(QAWizardPage)

// public:
//     enum TriState { Tri_Unknown = -1, Tri_False, Tri_True };

//     bool cachedIsComplete() const;
//     void _q_maybeEmitCompleteChanged();
//     void _q_updateCachedCompleteState();

//     QAWizard *wizard = nullptr;
//     QString title;
//     QString subTitle;
//     QPixmap pixmaps[QAWizard::NPixmaps];
//     QList<QAWizardField> pendingFields;
//     mutable TriState completeState = Tri_Unknown;
//     bool explicitlyFinal = false;
//     bool commit = false;
//     bool initialized = false;
//     QMap<int, QString> buttonCustomTexts;
// };

// bool QAWizardPagePrivate::cachedIsComplete() const
// {
//     Q_Q(const QAWizardPage);
//     if (completeState == Tri_Unknown)
//         completeState = q->isComplete() ? Tri_True : Tri_False;
//     return completeState == Tri_True;
// }

// void QAWizardPagePrivate::_q_maybeEmitCompleteChanged()
// {
//     Q_Q(QAWizardPage);
//     TriState newState = q->isComplete() ? Tri_True : Tri_False;
//     if (newState != completeState)
//         emit q->completeChanged();
// }

// void QAWizardPagePrivate::_q_updateCachedCompleteState()
// {
//     Q_Q(QAWizardPage);
//     completeState = q->isComplete() ? Tri_True : Tri_False;
// }

// class QAWizardAntiFlickerWidget : public QWidget
// {
// public:
// #if QT_CONFIG(style_windowsvista)
//     QAWizardPrivate *wizardPrivate;
//     QAWizardAntiFlickerWidget(QAWizard *wizard, QAWizardPrivate *wizardPrivate)
//         : QWidget(wizard)
//         , wizardPrivate(wizardPrivate) {}
// protected:
//     void paintEvent(QPaintEvent *) override;
// #else
//     QAWizardAntiFlickerWidget(QAWizard *wizard, QAWizardPrivate *)
//         : QWidget(wizard)
//     {}
// #endif
// };

// class QAWizardPrivate : public QDialogPrivate
// {
//     Q_DECLARE_PUBLIC(QAWizard)

// public:
//     typedef QMap<int, QAWizardPage *> PageMap;

//     enum Direction {
//         Backward,
//         Forward
//     };

//     void init();
//     void reset();
//     void cleanupPagesNotInHistory();
//     void addField(const QAWizardField &field);
//     void removeFieldAt(int index);
//     void switchToPage(int newId, Direction direction);
//     QAWizardLayoutInfo layoutInfoForCurrentPage();
//     void recreateLayout(const QAWizardLayoutInfo &info);
//     void updateLayout();
//     void updatePalette();
//     void updateMinMaxSizes(const QAWizardLayoutInfo &info);
//     void updateCurrentPage();
//     bool ensureButton(QAWizard::WizardButton which) const;
//     void connectButton(QAWizard::WizardButton which) const;
//     void updateButtonTexts();
//     void updateButtonLayout();
//     void setButtonLayout(const QAWizard::WizardButton *array, int size);
//     bool buttonLayoutContains(QAWizard::WizardButton which);
//     void updatePixmap(QAWizard::WizardPixmap which);
// #if QT_CONFIG(style_windowsvista)
//     bool vistaDisabled() const;
//     bool handleAeroStyleChange();
// #endif
//     bool isVistaThemeEnabled() const;
//     void disableUpdates();
//     void enableUpdates();
//     void _q_emitCustomButtonClicked();
//     void _q_updateButtonStates();
//     void _q_handleFieldObjectDestroyed(QObject *);
//     void setStyle(QStyle *style);
// #ifdef Q_OS_MACOS
//     static QPixmap findDefaultBackgroundPixmap();
// #endif

//     PageMap pageMap;
//     QList<QAWizardField> fields;
//     QMap<QString, int> fieldIndexMap;
//     QList<QAWizardDefaultProperty> defaultPropertyTable;
//     QList<int> history;
//     int start = -1;
//     bool startSetByUser = false;
//     int current = -1;
//     bool canContinue = false;
//     bool canFinish = false;
//     QAWizardLayoutInfo layoutInfo;
//     int disableUpdatesCount = 0;

//     QAWizard::WizardStyle wizStyle = QAWizard::ClassicStyle;
//     QAWizard::WizardOptions opts;
//     QMap<int, QString> buttonCustomTexts;
//     bool buttonsHaveCustomLayout = false;
//     QList<QAWizard::WizardButton> buttonsCustomLayout;
//     Qt::TextFormat titleFmt = Qt::AutoText;
//     Qt::TextFormat subTitleFmt = Qt::AutoText;
//     mutable QPixmap defaultPixmaps[QAWizard::NPixmaps];

//     union {
//         // keep in sync with QAWizard::WizardButton
//         mutable struct {
//             QAbstractButton *back;
//             QAbstractButton *next;
//             QAbstractButton *commit;
//             QAbstractButton *finish;
//             QAbstractButton *cancel;
//             QAbstractButton *help;
//         } btn;
//         mutable QAbstractButton *btns[QAWizard::NButtons];
//     };
//     QAWizardAntiFlickerWidget *antiFlickerWidget = nullptr;
//     QWidget *placeholderWidget1 = nullptr;
//     QWidget *placeholderWidget2 = nullptr;
//     QAWizardHeader *headerWidget = nullptr;
//     QWatermarkLabel *watermarkLabel = nullptr;
//     QWidget *sideWidget = nullptr;
//     QFrame *pageFrame = nullptr;
//     QLabel *titleLabel = nullptr;
//     QLabel *subTitleLabel = nullptr;
//     QAWizardRuler *bottomRuler = nullptr;

//     QVBoxLayout *pageVBoxLayout = nullptr;
//     QHBoxLayout *buttonLayout = nullptr;
//     QGridLayout *mainLayout = nullptr;

// #if QT_CONFIG(style_windowsvista)
//     QVistaHelper *vistaHelper = nullptr;
// #  if QT_CONFIG(shortcut)
//     QPointer<QShortcut> vistaNextShortcut;
// #  endif
//     bool vistaInitPending = true;
//     bool vistaDirty = true;
//     bool vistaStateChanged = false;
//     bool inHandleAeroStyleChange = false;
// #endif
//     int minimumWidth = 0;
//     int minimumHeight = 0;
//     int maximumWidth = QWIDGETSIZE_MAX;
//     int maximumHeight = QWIDGETSIZE_MAX;
// };

// static QString buttonDefaultText(int wstyle, int which, const QAWizardPrivate *wizardPrivate)
// {
// #if !QT_CONFIG(style_windowsvista)
//     Q_UNUSED(wizardPrivate);
// #endif
//     const bool macStyle = (wstyle == QAWizard::MacStyle);
//     switch (which) {
//     case QAWizard::BackButton:
//         return macStyle ? QAWizard::tr("Go Back") : QAWizard::tr("< &Back");
//     case QAWizard::NextButton:
//         if (macStyle)
//             return QAWizard::tr("Continue");
//         else
//             return wizardPrivate->isVistaThemeEnabled()
//                 ? QAWizard::tr("&Next") : QAWizard::tr("&Next >");
//     case QAWizard::CommitButton:
//         return QAWizard::tr("Commit");
//     case QAWizard::FinishButton:
//         return macStyle ? QAWizard::tr("Done") : QAWizard::tr("&Finish");
//     case QAWizard::CancelButton:
//         return QAWizard::tr("Cancel");
//     case QAWizard::HelpButton:
//         return macStyle ? QAWizard::tr("Help") : QAWizard::tr("&Help");
//     default:
//         return QString();
//     }
// }

// void QAWizardPrivate::init()
// {
//     Q_Q(QAWizard);

//     std::fill(btns, btns + QAWizard::NButtons, nullptr);

//     antiFlickerWidget = new QAWizardAntiFlickerWidget(q, this);
//     wizStyle = QAWizard::WizardStyle(q->style()->styleHint(QStyle::SH_WizardStyle, nullptr, q));
//     if (wizStyle == QAWizard::MacStyle) {
//         opts = (QAWizard::NoDefaultButton | QAWizard::NoCancelButton);
//     } else if (wizStyle == QAWizard::ModernStyle) {
//         opts = QAWizard::HelpButtonOnRight;
//     }

// #if QT_CONFIG(style_windowsvista)
//     vistaHelper = new QVistaHelper(q);
// #endif

//     // create these buttons right away; create the other buttons as necessary
//     ensureButton(QAWizard::BackButton);
//     ensureButton(QAWizard::NextButton);
//     ensureButton(QAWizard::CommitButton);
//     ensureButton(QAWizard::FinishButton);

//     pageFrame = new QFrame(antiFlickerWidget);
//     pageFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

//     pageVBoxLayout = new QVBoxLayout(pageFrame);
//     pageVBoxLayout->setSpacing(0);
//     pageVBoxLayout->addSpacing(0);
//     QSpacerItem *spacerItem = new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
//     pageVBoxLayout->addItem(spacerItem);

//     buttonLayout = new QHBoxLayout;
//     mainLayout = new QGridLayout(antiFlickerWidget);
//     mainLayout->setSizeConstraint(QLayout::SetNoConstraint);

//     updateButtonLayout();

//     defaultPropertyTable.reserve(NFallbackDefaultProperties);
//     for (uint i = 0; i < NFallbackDefaultProperties; ++i)
//         defaultPropertyTable.append(QAWizardDefaultProperty(fallbackProperties[i].className,
//                                                            fallbackProperties[i].property,
//                                                            changed_signal(i)));
// }

// void QAWizardPrivate::reset()
// {
//     Q_Q(QAWizard);
//     if (current != -1) {
//         q->currentPage()->hide();
//         cleanupPagesNotInHistory();
//         const auto end = history.crend();
//         for (auto it = history.crbegin(); it != end; ++it)
//             q->cleanupPage(*it);
//         history.clear();
//         for (QAWizardPage *page : std::as_const(pageMap))
//             page->d_func()->initialized = false;

//         current = -1;
//         emit q->currentIdChanged(-1);
//     }
// }

// void QAWizardPrivate::cleanupPagesNotInHistory()
// {
//     Q_Q(QAWizard);

//     for (auto it = pageMap.begin(), end = pageMap.end(); it != end; ++it) {
//         const auto idx = it.key();
//         const auto page = it.value()->d_func();
//         if (page->initialized && !history.contains(idx)) {
//             q->cleanupPage(idx);
//             page->initialized = false;
//         }
//     }
// }

// void QAWizardPrivate::addField(const QAWizardField &field)
// {
//     Q_Q(QAWizard);

//     QAWizardField myField = field;
//     myField.resolve(defaultPropertyTable);

//     if (Q_UNLIKELY(fieldIndexMap.contains(myField.name))) {
//         qWarning("QAWizardPage::addField: Duplicate field '%ls'", qUtf16Printable(myField.name));
//         return;
//     }

//     fieldIndexMap.insert(myField.name, fields.size());
//     fields += myField;
//     if (myField.mandatory && !myField.changedSignal.isEmpty())
//         QObject::connect(myField.object, myField.changedSignal,
//                          myField.page, SLOT(_q_maybeEmitCompleteChanged()));
//     QObject::connect(
//         myField.object, SIGNAL(destroyed(QObject*)), q,
//         SLOT(_q_handleFieldObjectDestroyed(QObject*)));
// }

// void QAWizardPrivate::removeFieldAt(int index)
// {
//     Q_Q(QAWizard);

//     const QAWizardField &field = fields.at(index);
//     fieldIndexMap.remove(field.name);
//     if (field.mandatory && !field.changedSignal.isEmpty())
//         QObject::disconnect(field.object, field.changedSignal,
//                             field.page, SLOT(_q_maybeEmitCompleteChanged()));
//     QObject::disconnect(
//         field.object, SIGNAL(destroyed(QObject*)), q,
//         SLOT(_q_handleFieldObjectDestroyed(QObject*)));
//     fields.remove(index);
// }

// void QAWizardPrivate::switchToPage(int newId, Direction direction)
// {
//     Q_Q(QAWizard);

//     disableUpdates();

//     int oldId = current;
//     if (QAWizardPage *oldPage = q->currentPage()) {
//         oldPage->hide();

//         if (direction == Backward) {
//             if (!(opts & QAWizard::IndependentPages)) {
//                 q->cleanupPage(oldId);
//                 oldPage->d_func()->initialized = false;
//             }
//             Q_ASSERT(history.constLast() == oldId);
//             history.removeLast();
//             Q_ASSERT(history.constLast() == newId);
//         }
//     }

//     current = newId;

//     QAWizardPage *newPage = q->currentPage();
//     if (newPage) {
//         if (direction == Forward) {
//             if (!newPage->d_func()->initialized) {
//                 newPage->d_func()->initialized = true;
//                 q->initializePage(current);
//             }
//             history.append(current);
//         }
//         newPage->show();
//     }

//     canContinue = (q->nextId() != -1);
//     canFinish = (newPage && newPage->isFinalPage());

//     _q_updateButtonStates();
//     updateButtonTexts();

//     const QAWizard::WizardButton nextOrCommit =
//         newPage && newPage->isCommitPage() ? QAWizard::CommitButton : QAWizard::NextButton;
//     QAbstractButton *nextOrFinishButton =
//         btns[canContinue ? nextOrCommit : QAWizard::FinishButton];
//     QWidget *candidate = nullptr;

//     /*
//         If there is no default button and the Next or Finish button
//         is enabled, give focus directly to it as a convenience to the
//         user. This is the normal case on OS X.

//         Otherwise, give the focus to the new page's first child that
//         can handle it. If there is no such child, give the focus to
//         Next or Finish.
//     */
//     if ((opts & QAWizard::NoDefaultButton) && nextOrFinishButton->isEnabled()) {
//         candidate = nextOrFinishButton;
//     } else if (newPage) {
//         candidate = iWantTheFocus(newPage);
//     }
//     if (!candidate)
//         candidate = nextOrFinishButton;
//     candidate->setFocus();

//     if (wizStyle == QAWizard::MacStyle)
//         q->updateGeometry();

//     enableUpdates();
//     updateLayout();
//     updatePalette();

//     emit q->currentIdChanged(current);
// }

// // keep in sync with QAWizard::WizardButton
// static const char * buttonSlots(QAWizard::WizardButton which)
// {
//     switch (which) {
//     case QAWizard::BackButton:
//         return SLOT(back());
//     case QAWizard::NextButton:
//     case QAWizard::CommitButton:
//         return SLOT(next());
//     case QAWizard::FinishButton:
//         return SLOT(accept());
//     case QAWizard::CancelButton:
//         return SLOT(reject());
//     case QAWizard::HelpButton:
//         return SIGNAL(helpRequested());
//     case QAWizard::CustomButton1:
//     case QAWizard::CustomButton2:
//     case QAWizard::CustomButton3:
//     case QAWizard::Stretch:
//     case QAWizard::NoButton:
//         Q_UNREACHABLE();
//     };
//     return nullptr;
// };

// QAWizardLayoutInfo QAWizardPrivate::layoutInfoForCurrentPage()
// {
//     Q_Q(QAWizard);
//     QStyle *style = q->style();

//     QAWizardLayoutInfo info;

//     QStyleOption option;
//     option.initFrom(q);
//     const int layoutHorizontalSpacing = style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, &option, q);
//     info.topLevelMarginLeft = style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, q);
//     info.topLevelMarginRight = style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, q);
//     info.topLevelMarginTop = style->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, q);
//     info.topLevelMarginBottom = style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, q);
//     info.childMarginLeft = style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, titleLabel);
//     info.childMarginRight = style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, titleLabel);
//     info.childMarginTop = style->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, titleLabel);
//     info.childMarginBottom = style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, titleLabel);
//     info.hspacing = (layoutHorizontalSpacing == -1)
//         ? style->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Horizontal)
//         : layoutHorizontalSpacing;
//     info.vspacing = style->pixelMetric(QStyle::PM_LayoutVerticalSpacing, &option, q);
//     info.buttonSpacing = (layoutHorizontalSpacing == -1)
//         ? style->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal)
//         : layoutHorizontalSpacing;

//     if (wizStyle == QAWizard::MacStyle)
//         info.buttonSpacing = 12;

//     info.wizStyle = wizStyle;
//     if (info.wizStyle == QAWizard::AeroStyle
// #if QT_CONFIG(style_windowsvista)
//         && vistaDisabled()
// #endif
//         )
//         info.wizStyle = QAWizard::ModernStyle;

//     QString titleText;
//     QString subTitleText;
//     QPixmap backgroundPixmap;
//     QPixmap watermarkPixmap;

//     if (QAWizardPage *page = q->currentPage()) {
//         titleText = page->title();
//         subTitleText = page->subTitle();
//         backgroundPixmap = page->pixmap(QAWizard::BackgroundPixmap);
//         watermarkPixmap = page->pixmap(QAWizard::WatermarkPixmap);
//     }

//     info.header = (info.wizStyle == QAWizard::ClassicStyle || info.wizStyle == QAWizard::ModernStyle)
//         && !(opts & QAWizard::IgnoreSubTitles) && !subTitleText.isEmpty();
//     info.sideWidget = sideWidget;
//     info.watermark = (info.wizStyle != QAWizard::MacStyle) && (info.wizStyle != QAWizard::AeroStyle)
//         && !watermarkPixmap.isNull();
//     info.title = !info.header && !titleText.isEmpty();
//     info.subTitle = !(opts & QAWizard::IgnoreSubTitles) && !info.header && !subTitleText.isEmpty();
//     info.extension = (info.watermark || info.sideWidget) && (opts & QAWizard::ExtendedWatermarkPixmap);

//     return info;
// }

// void QAWizardPrivate::recreateLayout(const QAWizardLayoutInfo &info)
// {
//     Q_Q(QAWizard);

//     /*
//         Start by undoing the main layout.
//     */
//     for (int i = mainLayout->count() - 1; i >= 0; --i) {
//         QLayoutItem *item = mainLayout->takeAt(i);
//         if (item->layout()) {
//             item->layout()->setParent(nullptr);
//         } else {
//             delete item;
//         }
//     }
//     for (int i = mainLayout->columnCount() - 1; i >= 0; --i)
//         mainLayout->setColumnMinimumWidth(i, 0);
//     for (int i = mainLayout->rowCount() - 1; i >= 0; --i)
//         mainLayout->setRowMinimumHeight(i, 0);

//     /*
//         Now, recreate it.
//     */

//     bool mac = (info.wizStyle == QAWizard::MacStyle);
//     bool classic = (info.wizStyle == QAWizard::ClassicStyle);
//     bool modern = (info.wizStyle == QAWizard::ModernStyle);
//     bool aero = (info.wizStyle == QAWizard::AeroStyle);
//     int deltaMarginLeft = info.topLevelMarginLeft - info.childMarginLeft;
//     int deltaMarginRight = info.topLevelMarginRight - info.childMarginRight;
//     int deltaMarginTop = info.topLevelMarginTop - info.childMarginTop;
//     int deltaMarginBottom = info.topLevelMarginBottom - info.childMarginBottom;
//     int deltaVSpacing = info.topLevelMarginBottom - info.vspacing;

//     int row = 0;
//     int numColumns;
//     if (mac) {
//         numColumns = 3;
//     } else if (info.watermark || info.sideWidget) {
//         numColumns = 2;
//     } else {
//         numColumns = 1;
//     }
//     int pageColumn = qMin(1, numColumns - 1);

//     if (mac) {
//         mainLayout->setContentsMargins(QMargins());
//         mainLayout->setSpacing(0);
//         buttonLayout->setContentsMargins(MacLayoutLeftMargin, MacButtonTopMargin, MacLayoutRightMargin, MacLayoutBottomMargin);
//         pageVBoxLayout->setContentsMargins(7, 7, 7, 7);
//     } else {
//         if (modern) {
//             mainLayout->setContentsMargins(QMargins());
//             mainLayout->setSpacing(0);
//             pageVBoxLayout->setContentsMargins(deltaMarginLeft, deltaMarginTop,
//                                                deltaMarginRight, deltaMarginBottom);
//             buttonLayout->setContentsMargins(info.topLevelMarginLeft, info.topLevelMarginTop,
//                                              info.topLevelMarginRight, info.topLevelMarginBottom);
//         } else {
//             mainLayout->setContentsMargins(info.topLevelMarginLeft, info.topLevelMarginTop,
//                                            info.topLevelMarginRight, info.topLevelMarginBottom);
//             mainLayout->setHorizontalSpacing(info.hspacing);
//             mainLayout->setVerticalSpacing(info.vspacing);
//             pageVBoxLayout->setContentsMargins(0, 0, 0, 0);
//             buttonLayout->setContentsMargins(0, 0, 0, 0);
//         }
//     }
//     buttonLayout->setSpacing(info.buttonSpacing);

//     if (info.header) {
//         if (!headerWidget)
//             headerWidget = new QAWizardHeader(antiFlickerWidget);
//         headerWidget->setAutoFillBackground(modern);
//         mainLayout->addWidget(headerWidget, row++, 0, 1, numColumns);
//     }
//     if (headerWidget)
//         headerWidget->setVisible(info.header);

//     int watermarkStartRow = row;

//     if (mac)
//         mainLayout->setRowMinimumHeight(row++, 10);

//     if (info.title) {
//         if (!titleLabel) {
//             titleLabel = new QLabel(antiFlickerWidget);
//             titleLabel->setBackgroundRole(QPalette::Base);
//             titleLabel->setWordWrap(true);
//         }

//         QFont titleFont = q->font();
//         titleFont.setPointSize(titleFont.pointSize() + (mac ? 3 : 4));
//         titleFont.setBold(true);
//         titleLabel->setPalette(QPalette());

//         if (aero) {
//             // ### hardcoded for now:
//             titleFont = QFont("Segoe UI"_L1, 12);
//             QPalette pal(titleLabel->palette());
//             pal.setColor(QPalette::Text, QColor(0x00, 0x33, 0x99));
//             titleLabel->setPalette(pal);
//         }

//         titleLabel->setFont(titleFont);
//         const int aeroTitleIndent = 25; // ### hardcoded for now - should be calculated somehow
//         if (aero)
//             titleLabel->setIndent(aeroTitleIndent);
//         else if (mac)
//             titleLabel->setIndent(2);
//         else if (classic)
//             titleLabel->setIndent(info.childMarginLeft);
//         else
//             titleLabel->setIndent(info.topLevelMarginLeft);
//         if (modern) {
//             if (!placeholderWidget1) {
//                 placeholderWidget1 = new QWidget(antiFlickerWidget);
//                 placeholderWidget1->setBackgroundRole(QPalette::Base);
//             }
//             placeholderWidget1->setFixedHeight(info.topLevelMarginLeft + 2);
//             mainLayout->addWidget(placeholderWidget1, row++, pageColumn);
//         }
//         mainLayout->addWidget(titleLabel, row++, pageColumn);
//         if (modern) {
//             if (!placeholderWidget2) {
//                 placeholderWidget2 = new QWidget(antiFlickerWidget);
//                 placeholderWidget2->setBackgroundRole(QPalette::Base);
//             }
//             placeholderWidget2->setFixedHeight(5);
//             mainLayout->addWidget(placeholderWidget2, row++, pageColumn);
//         }
//         if (mac)
//             mainLayout->setRowMinimumHeight(row++, 7);
//     }
//     if (placeholderWidget1)
//         placeholderWidget1->setVisible(info.title && modern);
//     if (placeholderWidget2)
//         placeholderWidget2->setVisible(info.title && modern);

//     if (info.subTitle) {
//         if (!subTitleLabel) {
//             subTitleLabel = new QLabel(pageFrame);
//             subTitleLabel->setWordWrap(true);

//             subTitleLabel->setContentsMargins(info.childMarginLeft , 0,
//                                               info.childMarginRight , 0);

//             pageVBoxLayout->insertWidget(1, subTitleLabel);
//         }
//     }

//     // ### try to replace with margin.
//     changeSpacerSize(pageVBoxLayout, 0, 0, info.subTitle ? info.childMarginLeft : 0);

//     int hMargin = mac ? 1 : 0;
//     int vMargin = hMargin;

//     pageFrame->setFrameStyle(mac ? (QFrame::Box | QFrame::Raised) : QFrame::NoFrame);
//     pageFrame->setLineWidth(0);
//     pageFrame->setMidLineWidth(hMargin);

//     if (info.header) {
//         if (modern) {
//             hMargin = info.topLevelMarginLeft;
//             vMargin = deltaMarginBottom;
//         } else if (classic) {
//             hMargin = deltaMarginLeft + ClassicHMargin;
//             vMargin = 0;
//         }
//     }

//     if (aero) {
//         int leftMargin   = 18; // ### hardcoded for now - should be calculated somehow
//         int topMargin    = vMargin;
//         int rightMargin  = hMargin; // ### for now
//         int bottomMargin = vMargin;
//         pageFrame->setContentsMargins(leftMargin, topMargin, rightMargin, bottomMargin);
//     } else {
//         pageFrame->setContentsMargins(hMargin, vMargin, hMargin, vMargin);
//     }

//     if ((info.watermark || info.sideWidget) && !watermarkLabel) {
//         watermarkLabel = new QWatermarkLabel(antiFlickerWidget, sideWidget);
//         watermarkLabel->setBackgroundRole(QPalette::Base);
//         watermarkLabel->setMinimumHeight(1);
//         watermarkLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
//         watermarkLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
//     }

//     //bool wasSemiTransparent = pageFrame->testAttribute(Qt::WA_SetPalette);
//     const bool wasSemiTransparent =
//         pageFrame->palette().brush(QPalette::Window).color().alpha() < 255
//         || pageFrame->palette().brush(QPalette::Base).color().alpha() < 255;
//     if (mac) {
//         pageFrame->setAutoFillBackground(true);
//         antiFlickerWidget->setAutoFillBackground(false);
//     } else {
//         if (wasSemiTransparent)
//             pageFrame->setPalette(QPalette());

//         bool baseBackground = (modern && !info.header); // ### TAG1
//         pageFrame->setBackgroundRole(baseBackground ? QPalette::Base : QPalette::Window);

//         if (titleLabel)
//             titleLabel->setAutoFillBackground(baseBackground);
//         pageFrame->setAutoFillBackground(baseBackground);
//         if (watermarkLabel)
//             watermarkLabel->setAutoFillBackground(baseBackground);
//         if (placeholderWidget1)
//             placeholderWidget1->setAutoFillBackground(baseBackground);
//         if (placeholderWidget2)
//             placeholderWidget2->setAutoFillBackground(baseBackground);

//         if (aero) {
//             QPalette pal = pageFrame->palette();
//             pal.setBrush(QPalette::Window, QColor(255, 255, 255));
//             pageFrame->setPalette(pal);
//             pageFrame->setAutoFillBackground(true);
//             pal = antiFlickerWidget->palette();
//             pal.setBrush(QPalette::Window, QColor(255, 255, 255));
//             antiFlickerWidget->setPalette(pal);
//             antiFlickerWidget->setAutoFillBackground(true);
//         }
//     }

//     mainLayout->addWidget(pageFrame, row++, pageColumn);

//     int watermarkEndRow = row;
//     if (classic)
//         mainLayout->setRowMinimumHeight(row++, deltaVSpacing);

//     if (aero) {
//         buttonLayout->setContentsMargins(9, 9, 9, 9);
//         mainLayout->setContentsMargins(0, 11, 0, 0);
//     }

//     int buttonStartColumn = info.extension ? 1 : 0;
//     int buttonNumColumns = info.extension ? 1 : numColumns;

//     if (classic || modern) {
//         if (!bottomRuler)
//             bottomRuler = new QAWizardRuler(antiFlickerWidget);
//         mainLayout->addWidget(bottomRuler, row++, buttonStartColumn, 1, buttonNumColumns);
//     }

//     if (classic)
//         mainLayout->setRowMinimumHeight(row++, deltaVSpacing);

//     mainLayout->addLayout(buttonLayout, row++, buttonStartColumn, 1, buttonNumColumns);

//     if (info.watermark || info.sideWidget) {
//         if (info.extension)
//             watermarkEndRow = row;
//         mainLayout->addWidget(watermarkLabel, watermarkStartRow, 0,
//                               watermarkEndRow - watermarkStartRow, 1);
//     }

//     mainLayout->setColumnMinimumWidth(0, mac && !info.watermark ? 181 : 0);
//     if (mac)
//         mainLayout->setColumnMinimumWidth(2, 21);

//     if (headerWidget)
//         headerWidget->setVisible(info.header);
//     if (titleLabel)
//         titleLabel->setVisible(info.title);
//     if (subTitleLabel)
//         subTitleLabel->setVisible(info.subTitle);
//     if (bottomRuler)
//         bottomRuler->setVisible(classic || modern);
//     if (watermarkLabel)
//         watermarkLabel->setVisible(info.watermark || info.sideWidget);

//     layoutInfo = info;
// }

// void QAWizardPrivate::updateLayout()
// {
//     Q_Q(QAWizard);

//     disableUpdates();

//     QAWizardLayoutInfo info = layoutInfoForCurrentPage();
//     if (layoutInfo != info)
//         recreateLayout(info);
//     QAWizardPage *page = q->currentPage();

//     // If the page can expand vertically, let it stretch "infinitely" more
//     // than the QSpacerItem at the bottom. Otherwise, let the QSpacerItem
//     // stretch "infinitely" more than the page. Change the bottom item's
//     // policy accordingly. The case that the page has no layout is basically
//     // for Designer, only.
//     if (page) {
//         bool expandPage = !page->layout();
//         if (!expandPage) {
//             const QLayoutItem *pageItem = pageVBoxLayout->itemAt(pageVBoxLayout->indexOf(page));
//             expandPage = pageItem->expandingDirections() & Qt::Vertical;
//         }
//         QSpacerItem *bottomSpacer = pageVBoxLayout->itemAt(pageVBoxLayout->count() -  1)->spacerItem();
//         Q_ASSERT(bottomSpacer);
//         bottomSpacer->changeSize(0, 0, QSizePolicy::Ignored, expandPage ? QSizePolicy::Ignored : QSizePolicy::MinimumExpanding);
//         pageVBoxLayout->invalidate();
//     }

//     if (info.header) {
//         Q_ASSERT(page);
//         headerWidget->setup(info, page->title(), page->subTitle(),
//                             page->pixmap(QAWizard::LogoPixmap), page->pixmap(QAWizard::BannerPixmap),
//                             titleFmt, subTitleFmt);
//     }

//     if (info.watermark || info.sideWidget) {
//         QPixmap pix;
//         if (info.watermark) {
//             if (page)
//                 pix = page->pixmap(QAWizard::WatermarkPixmap);
//             else
//                 pix = q->pixmap(QAWizard::WatermarkPixmap);
//         }
//         watermarkLabel->setPixmap(pix); // in case there is no watermark and we show the side widget we need to clear the watermark
//     }

//     if (info.title) {
//         Q_ASSERT(page);
//         titleLabel->setTextFormat(titleFmt);
//         titleLabel->setText(page->title());
//     }
//     if (info.subTitle) {
//         Q_ASSERT(page);
//         subTitleLabel->setTextFormat(subTitleFmt);
//         subTitleLabel->setText(page->subTitle());
//     }

//     enableUpdates();
//     updateMinMaxSizes(info);
// }

// void QAWizardPrivate::updatePalette() {
//     if (wizStyle == QAWizard::MacStyle) {
//         // This is required to ensure visual semitransparency when
//         // switching from ModernStyle to MacStyle.
//         // See TAG1 in recreateLayout
//         // This additionally ensures that the colors are correct
//         // when the theme is changed.

//         // we should base the new palette on the default one
//         // so theme colors will be correct
//         QPalette newPalette = QApplication::palette(pageFrame);

//         QColor windowColor = newPalette.brush(QPalette::Window).color();
//         windowColor.setAlpha(153);
//         newPalette.setBrush(QPalette::Window, windowColor);

//         QColor baseColor = newPalette.brush(QPalette::Base).color();
//         baseColor.setAlpha(153);
//         newPalette.setBrush(QPalette::Base, baseColor);

//         pageFrame->setPalette(newPalette);
//     }
// }

// void QAWizardPrivate::updateMinMaxSizes(const QAWizardLayoutInfo &info)
// {
//     Q_Q(QAWizard);

//     int extraHeight = 0;
// #if QT_CONFIG(style_windowsvista)
//     if (isVistaThemeEnabled())
//         extraHeight = vistaHelper->titleBarSize() + vistaHelper->topOffset(q);
// #endif
//     QSize minimumSize = mainLayout->totalMinimumSize() + QSize(0, extraHeight);
//     QSize maximumSize = mainLayout->totalMaximumSize();
//     if (info.header && headerWidget->maximumWidth() != QWIDGETSIZE_MAX) {
//         minimumSize.setWidth(headerWidget->maximumWidth());
//         maximumSize.setWidth(headerWidget->maximumWidth());
//     }
//     if (info.watermark && !info.sideWidget) {
//         minimumSize.setHeight(mainLayout->totalSizeHint().height());
//     }
//     if (q->minimumWidth() == minimumWidth) {
//         minimumWidth = minimumSize.width();
//         q->setMinimumWidth(minimumWidth);
//     }
//     if (q->minimumHeight() == minimumHeight) {
//         minimumHeight = minimumSize.height();
//         q->setMinimumHeight(minimumHeight);
//     }
//     if (q->maximumWidth() == maximumWidth) {
//         maximumWidth = maximumSize.width();
//         q->setMaximumWidth(maximumWidth);
//     }
//     if (q->maximumHeight() == maximumHeight) {
//         maximumHeight = maximumSize.height();
//         q->setMaximumHeight(maximumHeight);
//     }
// }

// void QAWizardPrivate::updateCurrentPage()
// {
//     Q_Q(QAWizard);
//     if (q->currentPage()) {
//         canContinue = (q->nextId() != -1);
//         canFinish = q->currentPage()->isFinalPage();
//     } else {
//         canContinue = false;
//         canFinish = false;
//     }
//     _q_updateButtonStates();
//     updateButtonTexts();
// }

// static QString object_name_for_button(QAWizard::WizardButton which)
// {
//     switch (which) {
//     case QAWizard::CommitButton:
//         return u"qt_wizard_commit"_s;
//     case QAWizard::FinishButton:
//         return u"qt_wizard_finish"_s;
//     case QAWizard::CancelButton:
//         return u"qt_wizard_cancel"_s;
//     case QAWizard::BackButton:
//     case QAWizard::NextButton:
//     case QAWizard::HelpButton:
//     case QAWizard::CustomButton1:
//     case QAWizard::CustomButton2:
//     case QAWizard::CustomButton3:
//         // Make navigation buttons detectable as passive interactor in designer
//         return "__qt__passive_wizardbutton"_L1 + QString::number(which);
//     case QAWizard::Stretch:
//     case QAWizard::NoButton:
//     //case QAWizard::NStandardButtons:
//     //case QAWizard::NButtons:
//         ;
//     }
//     Q_UNREACHABLE_RETURN(QString());
// }

// bool QAWizardPrivate::ensureButton(QAWizard::WizardButton which) const
// {
//     Q_Q(const QAWizard);
//     if (uint(which) >= QAWizard::NButtons)
//         return false;

//     if (!btns[which]) {
//         QPushButton *pushButton = new QPushButton(antiFlickerWidget);
//         QStyle *style = q->style();
//         if (style != QApplication::style()) // Propagate style
//             pushButton->setStyle(style);
//         pushButton->setObjectName(object_name_for_button(which));
// #ifdef Q_OS_MACOS
//         pushButton->setAutoDefault(false);
// #endif
//         pushButton->hide();
// #ifdef Q_CC_HPACC
//         const_cast<QAWizardPrivate *>(this)->btns[which] = pushButton;
// #else
//         btns[which] = pushButton;
// #endif
//         if (which < QAWizard::NStandardButtons)
//             pushButton->setText(buttonDefaultText(wizStyle, which, this));

//         connectButton(which);
//     }
//     return true;
// }

// void QAWizardPrivate::connectButton(QAWizard::WizardButton which) const
// {
//     Q_Q(const QAWizard);
//     if (which < QAWizard::NStandardButtons) {
//         QObject::connect(btns[which], SIGNAL(clicked()), q, buttonSlots(which));
//     } else {
//         QObject::connect(btns[which], SIGNAL(clicked()), q, SLOT(_q_emitCustomButtonClicked()));
//     }
// }

// void QAWizardPrivate::updateButtonTexts()
// {
//     Q_Q(QAWizard);
//     for (int i = 0; i < QAWizard::NButtons; ++i) {
//         if (btns[i]) {
//             if (q->currentPage() && (q->currentPage()->d_func()->buttonCustomTexts.contains(i)))
//                 btns[i]->setText(q->currentPage()->d_func()->buttonCustomTexts.value(i));
//             else if (buttonCustomTexts.contains(i))
//                 btns[i]->setText(buttonCustomTexts.value(i));
//             else if (i < QAWizard::NStandardButtons)
//                 btns[i]->setText(buttonDefaultText(wizStyle, i, this));
//         }
//     }
//     // Vista: Add shortcut for 'next'. Note: native dialogs use ALT-Right
//     // even in RTL mode, so do the same, even if it might be counter-intuitive.
//     // The shortcut for 'back' is set in class QVistaBackButton.
// #if QT_CONFIG(shortcut) && QT_CONFIG(style_windowsvista)
//     if (btns[QAWizard::NextButton] && isVistaThemeEnabled()) {
//         if (vistaNextShortcut.isNull()) {
//             vistaNextShortcut =
//                 new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right),
//                               btns[QAWizard::NextButton], SLOT(animateClick()));
//         }
//     } else {
//         delete vistaNextShortcut;
//     }
// #endif // shortcut && style_windowsvista
// }

// void QAWizardPrivate::updateButtonLayout()
// {
//     if (buttonsHaveCustomLayout) {
//         QVarLengthArray<QAWizard::WizardButton, QAWizard::NButtons> array{
//                 buttonsCustomLayout.cbegin(), buttonsCustomLayout.cend()};
//         setButtonLayout(array.constData(), int(array.size()));
//     } else {
//         // Positions:
//         //     Help Stretch Custom1 Custom2 Custom3 Cancel Back Next Commit Finish Cancel Help

//         const int ArraySize = 12;
//         QAWizard::WizardButton array[ArraySize];
//         memset(array, -1, sizeof(array));
//         Q_ASSERT(array[0] == QAWizard::NoButton);

//         if (opts & QAWizard::HaveHelpButton) {
//             int i = (opts & QAWizard::HelpButtonOnRight) ? 11 : 0;
//             array[i] = QAWizard::HelpButton;
//         }
//         array[1] = QAWizard::Stretch;
//         if (opts & QAWizard::HaveCustomButton1)
//             array[2] = QAWizard::CustomButton1;
//         if (opts & QAWizard::HaveCustomButton2)
//             array[3] = QAWizard::CustomButton2;
//         if (opts & QAWizard::HaveCustomButton3)
//             array[4] = QAWizard::CustomButton3;

//         if (!(opts & QAWizard::NoCancelButton)) {
//             int i = (opts & QAWizard::CancelButtonOnLeft) ? 5 : 10;
//             array[i] = QAWizard::CancelButton;
//         }
//         array[6] = QAWizard::BackButton;
//         array[7] = QAWizard::NextButton;
//         array[8] = QAWizard::CommitButton;
//         array[9] = QAWizard::FinishButton;

//         setButtonLayout(array, ArraySize);
//     }
// }

// void QAWizardPrivate::setButtonLayout(const QAWizard::WizardButton *array, int size)
// {
//     QWidget *prev = pageFrame;

//     for (int i = buttonLayout->count() - 1; i >= 0; --i) {
//         QLayoutItem *item = buttonLayout->takeAt(i);
//         if (QWidget *widget = item->widget())
//             widget->hide();
//         delete item;
//     }

//     for (int i = 0; i < size; ++i) {
//         QAWizard::WizardButton which = array[i];
//         if (which == QAWizard::Stretch) {
//             buttonLayout->addStretch(1);
//         } else if (which != QAWizard::NoButton) {
//             ensureButton(which);
//             buttonLayout->addWidget(btns[which]);

//             // Back, Next, Commit, and Finish are handled in _q_updateButtonStates()
//             if (which != QAWizard::BackButton && which != QAWizard::NextButton
//                 && which != QAWizard::CommitButton && which != QAWizard::FinishButton)
//                 btns[which]->show();

//             if (prev)
//                 QWidget::setTabOrder(prev, btns[which]);
//             prev = btns[which];
//         }
//     }

//     _q_updateButtonStates();
// }

// bool QAWizardPrivate::buttonLayoutContains(QAWizard::WizardButton which)
// {
//     return !buttonsHaveCustomLayout || buttonsCustomLayout.contains(which);
// }

// void QAWizardPrivate::updatePixmap(QAWizard::WizardPixmap which)
// {
//     Q_Q(QAWizard);
//     if (which == QAWizard::BackgroundPixmap) {
//         if (wizStyle == QAWizard::MacStyle) {
//             q->update();
//             q->updateGeometry();
//         }
//     } else {
//         updateLayout();
//     }
// }

// #if QT_CONFIG(style_windowsvista)
// bool QAWizardPrivate::vistaDisabled() const
// {
//     Q_Q(const QAWizard);
//     const QVariant v = q->property("_q_wizard_vista_off");
//     return v.isValid() && v.toBool();
// }

// bool QAWizardPrivate::handleAeroStyleChange()
// {
//     Q_Q(QAWizard);

//     if (inHandleAeroStyleChange)
//         return false; // prevent recursion
//     // For top-level wizards, we need the platform window handle for the
//     // DWM changes. Delay aero initialization to the show event handling if
//     // it does not exist. If we are a child, skip DWM and just make room by
//     // moving the antiFlickerWidget.
//     const bool isWindow = q->isWindow();
//     if (isWindow && (!q->windowHandle() || !q->windowHandle()->handle()))
//         return false;
//     inHandleAeroStyleChange = true;

//     vistaHelper->disconnectBackButton();
//     q->removeEventFilter(vistaHelper);

//     bool vistaMargins = false;

//     if (isVistaThemeEnabled()) {
//         const int topOffset = vistaHelper->topOffset(q);
//         const int topPadding = vistaHelper->topPadding(q);
//         if (isWindow) {
//             vistaHelper->setDWMTitleBar(QVistaHelper::ExtendedTitleBar);
//             q->installEventFilter(vistaHelper);
//         }
//         q->setMouseTracking(true);
//         antiFlickerWidget->move(0, vistaHelper->titleBarSize() + topOffset);
//         vistaHelper->backButton()->move(
//             0, topOffset // ### should ideally work without the '+ 1'
//             - qMin(topOffset, topPadding + 1));
//         vistaMargins = true;
//         vistaHelper->backButton()->show();
//         if (isWindow)
//             vistaHelper->setTitleBarIconAndCaptionVisible(false);
//         QObject::connect(
//             vistaHelper->backButton(), SIGNAL(clicked()), q, buttonSlots(QAWizard::BackButton));
//         vistaHelper->backButton()->show();
//     } else {
//         q->setMouseTracking(true); // ### original value possibly different
// #ifndef QT_NO_CURSOR
//         q->unsetCursor(); // ### ditto
// #endif
//         antiFlickerWidget->move(0, 0);
//         vistaHelper->hideBackButton();
//         if (isWindow)
//             vistaHelper->setTitleBarIconAndCaptionVisible(true);
//     }

//     _q_updateButtonStates();

//     vistaHelper->updateCustomMargins(vistaMargins);

//     inHandleAeroStyleChange = false;
//     return true;
// }
// #endif

// bool QAWizardPrivate::isVistaThemeEnabled() const
// {
// #if QT_CONFIG(style_windowsvista)
//     return wizStyle == QAWizard::AeroStyle && !vistaDisabled();
// #else
//     return false;
// #endif
// }

// void QAWizardPrivate::disableUpdates()
// {
//     Q_Q(QAWizard);
//     if (disableUpdatesCount++ == 0) {
//         q->setUpdatesEnabled(false);
//         antiFlickerWidget->hide();
//     }
// }

// void QAWizardPrivate::enableUpdates()
// {
//     Q_Q(QAWizard);
//     if (--disableUpdatesCount == 0) {
//         antiFlickerWidget->show();
//         q->setUpdatesEnabled(true);
//     }
// }

// void QAWizardPrivate::_q_emitCustomButtonClicked()
// {
//     Q_Q(QAWizard);
//     QObject *button = q->sender();
//     for (int i = QAWizard::NStandardButtons; i < QAWizard::NButtons; ++i) {
//         if (btns[i] == button) {
//             emit q->customButtonClicked(QAWizard::WizardButton(i));
//             break;
//         }
//     }
// }

// void QAWizardPrivate::_q_updateButtonStates()
// {
//     Q_Q(QAWizard);

//     disableUpdates();

//     const QAWizardPage *page = q->currentPage();
//     bool complete = page && page->isComplete();

//     btn.back->setEnabled(history.size() > 1
//                          && !q->page(history.at(history.size() - 2))->isCommitPage()
//                          && (!canFinish || !(opts & QAWizard::DisabledBackButtonOnLastPage)));
//     btn.next->setEnabled(canContinue && complete);
//     btn.commit->setEnabled(canContinue && complete);
//     btn.finish->setEnabled(canFinish && complete);

//     const bool backButtonVisible = buttonLayoutContains(QAWizard::BackButton)
//         && (history.size() > 1 || !(opts & QAWizard::NoBackButtonOnStartPage))
//         && (canContinue || !(opts & QAWizard::NoBackButtonOnLastPage));
//     bool commitPage = page && page->isCommitPage();
//     btn.back->setVisible(backButtonVisible);
//     btn.next->setVisible(buttonLayoutContains(QAWizard::NextButton) && !commitPage
//                          && (canContinue || (opts & QAWizard::HaveNextButtonOnLastPage)));
//     btn.commit->setVisible(buttonLayoutContains(QAWizard::CommitButton) && commitPage
//                            && canContinue);
//     btn.finish->setVisible(buttonLayoutContains(QAWizard::FinishButton)
//                            && (canFinish || (opts & QAWizard::HaveFinishButtonOnEarlyPages)));

//     if (!(opts & QAWizard::NoCancelButton))
//         btn.cancel->setVisible(buttonLayoutContains(QAWizard::CancelButton)
//                                && (canContinue || !(opts & QAWizard::NoCancelButtonOnLastPage)));

//     bool useDefault = !(opts & QAWizard::NoDefaultButton);
//     if (QPushButton *nextPush = qobject_cast<QPushButton *>(btn.next))
//         nextPush->setDefault(canContinue && useDefault && !commitPage);
//     if (QPushButton *commitPush = qobject_cast<QPushButton *>(btn.commit))
//         commitPush->setDefault(canContinue && useDefault && commitPage);
//     if (QPushButton *finishPush = qobject_cast<QPushButton *>(btn.finish))
//         finishPush->setDefault(!canContinue && useDefault);

// #if QT_CONFIG(style_windowsvista)
//     if (isVistaThemeEnabled()) {
//         vistaHelper->backButton()->setEnabled(btn.back->isEnabled());
//         vistaHelper->backButton()->setVisible(backButtonVisible);
//         btn.back->setVisible(false);
//     }
// #endif

//     enableUpdates();
// }

// void QAWizardPrivate::_q_handleFieldObjectDestroyed(QObject *object)
// {
//     int destroyed_index = -1;
//     QList<QAWizardField>::iterator it = fields.begin();
//     while (it != fields.end()) {
//         const QAWizardField &field = *it;
//         if (field.object == object) {
//             destroyed_index = fieldIndexMap.value(field.name, -1);
//             fieldIndexMap.remove(field.name);
//             it = fields.erase(it);
//         } else {
//             ++it;
//         }
//     }
//     if (destroyed_index != -1) {
//         QMap<QString, int>::iterator it2 = fieldIndexMap.begin();
//         while (it2 != fieldIndexMap.end()) {
//             int index = it2.value();
//             if (index > destroyed_index) {
//                 QString field_name = it2.key();
//                 fieldIndexMap.insert(field_name, index-1);
//             }
//             ++it2;
//         }
//     }
// }

// void QAWizardPrivate::setStyle(QStyle *style)
// {
//     for (int i = 0; i < QAWizard::NButtons; i++)
//         if (btns[i])
//             btns[i]->setStyle(style);
//     const PageMap::const_iterator pcend = pageMap.constEnd();
//     for (PageMap::const_iterator it = pageMap.constBegin(); it != pcend; ++it)
//         it.value()->setStyle(style);
// }

// #ifdef Q_OS_MACOS
// QPixmap QAWizardPrivate::findDefaultBackgroundPixmap()
// {
//     auto *keyboardAssistantURL = [NSWorkspace.sharedWorkspace
//         URLForApplicationWithBundleIdentifier:@"com.apple.KeyboardSetupAssistant"];
//     auto *keyboardAssistantBundle = [NSBundle bundleWithURL:keyboardAssistantURL];
//     auto *assistantBackground = [keyboardAssistantBundle imageForResource:@"Background"];
//     auto size = QSizeF::fromCGSize(assistantBackground.size);
//     static const QSizeF expectedSize(242, 414);
//     if (size == expectedSize)
//         return qt_mac_toQPixmap(assistantBackground, size);

//     return QPixmap();
// }
// #endif

// #if QT_CONFIG(style_windowsvista)
// void QAWizardAntiFlickerWidget::paintEvent(QPaintEvent *)
// {
//     if (wizardPrivate->isVistaThemeEnabled()) {
//         int leftMargin, topMargin, rightMargin, bottomMargin;
//         wizardPrivate->buttonLayout->getContentsMargins(
//             &leftMargin, &topMargin, &rightMargin, &bottomMargin);
//         const int buttonLayoutTop = wizardPrivate->buttonLayout->contentsRect().top() - topMargin;
//         QPainter painter(this);
//         const QBrush brush(QColor(240, 240, 240)); // ### hardcoded for now
//         painter.fillRect(0, buttonLayoutTop, width(), height() - buttonLayoutTop, brush);
//         painter.setPen(QPen(QBrush(QColor(223, 223, 223)), 0)); // ### hardcoded for now
//         painter.drawLine(0, buttonLayoutTop, width(), buttonLayoutTop);
//     }
// }
// #endif

// /*!
//     \class QAWizard
//     \since 4.3
//     \brief The QAWizard class provides a framework for wizards.

//     \inmodule QtWidgets

//     A wizard (also called an assistant on \macos) is a special type
//     of input dialog that consists of a sequence of pages. A wizard's
//     purpose is to guide the user through a process step by step.
//     Wizards are useful for complex or infrequent tasks that users may
//     find difficult to learn.

//     QAWizard inherits QDialog and represents a wizard. Each page is a
//     QAWizardPage (a QWidget subclass). To create your own wizards, you
//     can use these classes directly, or you can subclass them for more
//     control.

//     Topics:

//     \tableofcontents

//     \section1 A Trivial Example

//     The following example illustrates how to create wizard pages and
//     add them to a wizard. For more advanced examples, see the
//     \l{dialogs/licensewizard}{License Wizard}.

//     \snippet dialogs/trivialwizard/trivialwizard.cpp 1
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 3
//     \dots
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 4
//     \codeline
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 5
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 7
//     \dots
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 8
//     \codeline
//     \snippet dialogs/trivialwizard/trivialwizard.cpp 10

//     \section1 Wizard Look and Feel

//     QAWizard supports four wizard looks:

//     \list
//     \li ClassicStyle
//     \li ModernStyle
//     \li MacStyle
//     \li AeroStyle
//     \endlist

//     You can explicitly set the look to use using setWizardStyle()
//     (e.g., if you want the same look on all platforms).

//     \table
//     \header \li ClassicStyle
//             \li ModernStyle
//             \li MacStyle
//             \li AeroStyle
//     \row    \li \inlineimage qtwizard-classic1.png
//             \li \inlineimage qtwizard-modern1.png
//             \li \inlineimage qtwizard-mac1.png
//             \li \inlineimage qtwizard-aero1.png
//     \row    \li \inlineimage qtwizard-classic2.png
//             \li \inlineimage qtwizard-modern2.png
//             \li \inlineimage qtwizard-mac2.png
//             \li \inlineimage qtwizard-aero2.png
//     \endtable

//     Note: AeroStyle has effect only on a Windows Vista system with alpha compositing enabled.
//     ModernStyle is used as a fallback when this condition is not met.

//     In addition to the wizard style, there are several options that
//     control the look and feel of the wizard. These can be set using
//     setOption() or setOptions(). For example, HaveHelpButton makes
//     QAWizard show a \uicontrol Help button along with the other wizard
//     buttons.

//     You can even change the order of the wizard buttons to any
//     arbitrary order using setButtonLayout(), and you can add up to
//     three custom buttons (e.g., a \uicontrol Print button) to the button
//     row. This is achieved by calling setButton() or setButtonText()
//     with CustomButton1, CustomButton2, or CustomButton3 to set up the
//     button, and by enabling the HaveCustomButton1, HaveCustomButton2,
//     or HaveCustomButton3 options. Whenever the user clicks a custom
//     button, customButtonClicked() is emitted. For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 29

//     \section1 Elements of a Wizard Page

//     Wizards consist of a sequence of \l{QAWizardPage}s. At any time,
//     only one page is shown. A page has the following attributes:

//     \list
//     \li A \l{QAWizardPage::}{title}.
//     \li A \l{QAWizardPage::}{subTitle}.
//     \li A set of pixmaps, which may or may not be honored, depending
//        on the wizard's style:
//         \list
//         \li WatermarkPixmap (used by ClassicStyle and ModernStyle)
//         \li BannerPixmap (used by ModernStyle)
//         \li LogoPixmap (used by ClassicStyle and ModernStyle)
//         \li BackgroundPixmap (used by MacStyle)
//         \endlist
//     \endlist

//     The diagram belows shows how QAWizard renders these attributes,
//     assuming they are all present and ModernStyle is used:

//     \image qtwizard-nonmacpage.png

//     When a \l{QAWizardPage::}{subTitle} is set, QAWizard displays it
//     in a header, in which case it also uses the BannerPixmap and the
//     LogoPixmap to decorate the header. The WatermarkPixmap is
//     displayed on the left side, below the header. At the bottom,
//     there is a row of buttons allowing the user to navigate through
//     the pages.

//     The page itself (the \l{QAWizardPage} widget) occupies the area
//     between the header, the watermark, and the button row. Typically,
//     the page is a QAWizardPage on which a QGridLayout is installed,
//     with standard child widgets (\l{QLabel}s, \l{QLineEdit}s, etc.).

//     If the wizard's style is MacStyle, the page looks radically
//     different:

//     \image qtwizard-macpage.png

//     The watermark, banner, and logo pixmaps are ignored by the
//     MacStyle. If the BackgroundPixmap is set, it is used as the
//     background for the wizard; otherwise, a default "assistant" image
//     is used.

//     The title and subtitle are set by calling
//     QAWizardPage::setTitle() and QAWizardPage::setSubTitle() on the
//     individual pages. They may be plain text or HTML (see titleFormat
//     and subTitleFormat). The pixmaps can be set globally for the
//     entire wizard using setPixmap(), or on a per-page basis using
//     QAWizardPage::setPixmap().

//     \target field mechanism
//     \section1 Registering and Using Fields

//     In many wizards, the contents of a page may affect the default
//     values of the fields of a later page. To make it easy to
//     communicate between pages, QAWizard supports a "field" mechanism
//     that allows you to register a field (e.g., a QLineEdit) on a page
//     and to access its value from any page. It is also possible to
//     specify mandatory fields (i.e., fields that must be filled before
//     the user can advance to the next page).

//     To register a field, call QAWizardPage::registerField() field.
//     For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 21

//     The above code registers three fields, \c className, \c
//     baseClass, and \c qobjectMacro, which are associated with three
//     child widgets. The asterisk (\c *) next to \c className denotes a
//     mandatory field.

//     \target initialize page
//     The fields of any page are accessible from any other page. For
//     example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 27

//     Here, we call QAWizardPage::field() to access the contents of the
//     \c details.email field (which was defined in the \c DetailsPage)
//     and use it to initialize the \c ConclusionPage. The field's
//     contents is returned as a QVariant.

//     When we create a field using QAWizardPage::registerField(), we
//     pass a unique field name and a widget. We can also provide a Qt
//     property name and a "changed" signal (a signal that is emitted
//     when the property changes) as third and fourth arguments;
//     however, this is not necessary for the most common Qt widgets,
//     such as QLineEdit, QCheckBox, and QComboBox, because QAWizard
//     knows which properties to look for.

//     \target mandatory fields

//     If an asterisk (\c *) is appended to the name when the property
//     is registered, the field is a \e{mandatory field}. When a page has
//     mandatory fields, the \uicontrol Next and/or \uicontrol Finish buttons are
//     enabled only when all mandatory fields are filled.

//     To consider a field "filled", QAWizard simply checks that the
//     field's current value doesn't equal the original value (the value
//     it had when initializePage() was called). For QLineEdit and
//     QAbstractSpinBox subclasses, QAWizard also checks that
//     \l{QLineEdit::hasAcceptableInput()}{hasAcceptableInput()} returns
//     true, to honor any validator or mask.

//     QAWizard's mandatory field mechanism is provided for convenience.
//     A more powerful (but also more cumbersome) alternative is to
//     reimplement QAWizardPage::isComplete() and to emit the
//     QAWizardPage::completeChanged() signal whenever the page becomes
//     complete or incomplete.

//     The enabled/disabled state of the \uicontrol Next and/or \uicontrol Finish
//     buttons is one way to perform validation on the user input.
//     Another way is to reimplement validateCurrentPage() (or
//     QAWizardPage::validatePage()) to perform some last-minute
//     validation (and show an error message if the user has entered
//     incomplete or invalid information). If the function returns \c true,
//     the next page is shown (or the wizard finishes); otherwise, the
//     current page stays up.

//     \section1 Creating Linear Wizards

//     Most wizards have a linear structure, with page 1 followed by
//     page 2 and so on until the last page. The \l{dialogs/trivialwizard}
//     {Trivial Wizard} example is such a wizard. With QAWizard, linear wizards
//     are created by instantiating the \l{QAWizardPage}s and inserting
//     them using addPage(). By default, the pages are shown in the
//     order in which they were added. For example:

//     \snippet dialogs/trivialwizard/trivialwizard.cpp linearAddPage

//     When a page is about to be shown, QAWizard calls initializePage()
//     (which in turn calls QAWizardPage::initializePage()) to fill the
//     page with default values. By default, this function does nothing,
//     but it can be reimplemented to initialize the page's contents
//     based on other pages' fields (see the \l{initialize page}{example
//     above}).

//     If the user presses \uicontrol Back, cleanupPage() is called (which in
//     turn calls QAWizardPage::cleanupPage()). The default
//     implementation resets the page's fields to their original values
//     (the values they had before initializePage() was called). If you
//     want the \uicontrol Back button to be non-destructive and keep the
//     values entered by the user, simply enable the IndependentPages
//     option.

//     \section1 Creating Non-Linear Wizards

//     Some wizards are more complex in that they allow different
//     traversal paths based on the information provided by the user.
//     The \l{dialogs/licensewizard}{License Wizard} example illustrates this.
//     It provides five wizard pages; depending on which options are
//     selected, the user can reach different pages.

//     \image licensewizard-flow.png

//     In complex wizards, pages are identified by IDs. These IDs are
//     typically defined using an enum. For example:

//     \snippet dialogs/licensewizard/licensewizard.h 0
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.h 2
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.h 3

//     The pages are inserted using setPage(), which takes an ID and an
//     instance of QAWizardPage (or of a subclass):

//     \snippet dialogs/licensewizard/licensewizard.cpp 1
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.cpp 8

//     By default, the pages are shown in increasing ID order. To
//     provide a dynamic order that depends on the options chosen by the
//     user, we must reimplement QAWizardPage::nextId(). For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 18
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 23
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 24
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 25
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 26

//     It would also be possible to put all the logic in one place, in a
//     QAWizard::nextId() reimplementation. For example:

//     \snippet code/src_gui_dialogs_QAWizard.cpp 0

//     To start at another page than the page with the lowest ID, call
//     setStartId().

//     To test whether a page has been visited or not, call
//     hasVisitedPage(). For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 27

//     \sa QAWizardPage, {Trivial Wizard Example}, {License Wizard Example}
// */

// /*!
//     \enum QAWizard::WizardButton

//     This enum specifies the buttons in a wizard.

//     \value BackButton  The \uicontrol Back button (\uicontrol {Go Back} on \macos)
//     \value NextButton  The \uicontrol Next button (\uicontrol Continue on \macos)
//     \value CommitButton  The \uicontrol Commit button
//     \value FinishButton  The \uicontrol Finish button (\uicontrol Done on \macos)
//     \value CancelButton  The \uicontrol Cancel button (see also NoCancelButton)
//     \value HelpButton    The \uicontrol Help button (see also HaveHelpButton)
//     \value CustomButton1  The first user-defined button (see also HaveCustomButton1)
//     \value CustomButton2  The second user-defined button (see also HaveCustomButton2)
//     \value CustomButton3  The third user-defined button (see also HaveCustomButton3)

//     The following value is only useful when calling setButtonLayout():

//     \value Stretch  A horizontal stretch in the button layout

//     \omitvalue NoButton
//     \omitvalue NStandardButtons
//     \omitvalue NButtons

//     \sa setButton(), setButtonText(), setButtonLayout(), customButtonClicked()
// */

// /*!
//     \enum QAWizard::WizardPixmap

//     This enum specifies the pixmaps that can be associated with a page.

//     \value WatermarkPixmap  The tall pixmap on the left side of a ClassicStyle or ModernStyle page
//     \value LogoPixmap  The small pixmap on the right side of a ClassicStyle or ModernStyle page header
//     \value BannerPixmap  The pixmap that occupies the background of a ModernStyle page header
//     \value BackgroundPixmap  The pixmap that occupies the background of a MacStyle wizard

//     \omitvalue NPixmaps

//     \sa setPixmap(), QAWizardPage::setPixmap(), {Elements of a Wizard Page}
// */

// /*!
//     \enum QAWizard::WizardStyle

//     This enum specifies the different looks supported by QAWizard.

//     \value ClassicStyle  Classic Windows look
//     \value ModernStyle  Modern Windows look
//     \value MacStyle  \macos look
//     \value AeroStyle  Windows Aero look

//     \omitvalue NStyles

//     \sa setWizardStyle(), WizardOption, {Wizard Look and Feel}
// */

// /*!
//     \enum QAWizard::WizardOption

//     This enum specifies various options that affect the look and feel
//     of a wizard.

//     \value IndependentPages  The pages are independent of each other
//                              (i.e., they don't derive values from each
//                              other).
//     \value IgnoreSubTitles  Don't show any subtitles, even if they are set.
//     \value ExtendedWatermarkPixmap  Extend any WatermarkPixmap all the
//                                     way down to the window's edge.
//     \value NoDefaultButton  Don't make the \uicontrol Next or \uicontrol Finish button the
//                             dialog's \l{QPushButton::setDefault()}{default button}.
//     \value NoBackButtonOnStartPage  Don't show the \uicontrol Back button on the start page.
//     \value NoBackButtonOnLastPage   Don't show the \uicontrol Back button on the last page.
//     \value DisabledBackButtonOnLastPage  Disable the \uicontrol Back button on the last page.
//     \value HaveNextButtonOnLastPage  Show the (disabled) \uicontrol Next button on the last page.
//     \value HaveFinishButtonOnEarlyPages  Show the (disabled) \uicontrol Finish button on non-final pages.
//     \value NoCancelButton  Don't show the \uicontrol Cancel button.
//     \value CancelButtonOnLeft  Put the \uicontrol Cancel button on the left of \uicontrol Back (rather than on
//                                the right of \uicontrol Finish or \uicontrol Next).
//     \value HaveHelpButton  Show the \uicontrol Help button.
//     \value HelpButtonOnRight  Put the \uicontrol Help button on the far right of the button layout
//                               (rather than on the far left).
//     \value HaveCustomButton1  Show the first user-defined button (CustomButton1).
//     \value HaveCustomButton2  Show the second user-defined button (CustomButton2).
//     \value HaveCustomButton3  Show the third user-defined button (CustomButton3).
//     \value NoCancelButtonOnLastPage   Don't show the \uicontrol Cancel button on the last page.

//     \sa setOptions(), setOption(), testOption()
// */

// /*!
//     Constructs a wizard with the given \a parent and window \a flags.

//     \sa parent(), windowFlags()
// */
// QAWizard::QAWizard(QWidget *parent, Qt::WindowFlags flags)
//     : QDialog(*new QAWizardPrivate, parent, flags)
// {
//     Q_D(QAWizard);
//     d->init();
// }

// /*!
//     Destroys the wizard and its pages, releasing any allocated resources.
// */
// QAWizard::~QAWizard()
// {
//     Q_D(QAWizard);
//     delete d->buttonLayout;
// }

// /*!
//     Adds the given \a page to the wizard, and returns the page's ID.

//     The ID is guaranteed to be larger than any other ID in the
//     QAWizard so far.

//     \sa setPage(), page(), pageAdded()
// */
// int QAWizard::addPage(QAWizardPage *page)
// {
//     Q_D(QAWizard);
//     int theid = 0;
//     if (!d->pageMap.isEmpty())
//         theid = d->pageMap.lastKey() + 1;
//     setPage(theid, page);
//     return theid;
// }

// /*!
//     \fn void QAWizard::setPage(int id, QAWizardPage *page)

//     Adds the given \a page to the wizard with the given \a id.

//     \note Adding a page may influence the value of the startId property
//     in case it was not set explicitly.

//     \sa addPage(), page(), pageAdded()
// */
// void QAWizard::setPage(int theid, QAWizardPage *page)
// {
//     Q_D(QAWizard);

//     if (Q_UNLIKELY(!page)) {
//         qWarning("QAWizard::setPage: Cannot insert null page");
//         return;
//     }

//     if (Q_UNLIKELY(theid == -1)) {
//         qWarning("QAWizard::setPage: Cannot insert page with ID -1");
//         return;
//     }

//     if (Q_UNLIKELY(d->pageMap.contains(theid))) {
//         qWarning("QAWizard::setPage: Page with duplicate ID %d ignored", theid);
//         return;
//     }

//     page->setParent(d->pageFrame);

//     QList<QAWizardField> &pendingFields = page->d_func()->pendingFields;
//     for (const auto &field : std::as_const(pendingFields))
//         d->addField(field);
//     pendingFields.clear();

//     connect(page, SIGNAL(completeChanged()), this, SLOT(_q_updateButtonStates()));

//     d->pageMap.insert(theid, page);
//     page->d_func()->wizard = this;

//     int n = d->pageVBoxLayout->count();

//     // disable layout to prevent layout updates while adding
//     bool pageVBoxLayoutEnabled = d->pageVBoxLayout->isEnabled();
//     d->pageVBoxLayout->setEnabled(false);

//     d->pageVBoxLayout->insertWidget(n - 1, page);

//     // hide new page and reset layout to old status
//     page->hide();
//     d->pageVBoxLayout->setEnabled(pageVBoxLayoutEnabled);

//     if (!d->startSetByUser && d->pageMap.constBegin().key() == theid)
//         d->start = theid;
//     emit pageAdded(theid);
// }

// /*!
//     Removes the page with the given \a id. cleanupPage() will be called if necessary.

//     \note Removing a page may influence the value of the startId property.

//     \since 4.5
//     \sa addPage(), setPage(), pageRemoved(), startId()
// */
// void QAWizard::removePage(int id)
// {
//     Q_D(QAWizard);

//     QAWizardPage *removedPage = nullptr;

//     // update startItem accordingly
//     if (d->pageMap.size() > 0) { // only if we have any pages
//         if (d->start == id) {
//             const int firstId = d->pageMap.constBegin().key();
//             if (firstId == id) {
//                 if (d->pageMap.size() > 1)
//                     d->start = (++d->pageMap.constBegin()).key(); // secondId
//                 else
//                     d->start = -1; // removing the last page
//             } else { // startSetByUser has to be "true" here
//                 d->start = firstId;
//             }
//             d->startSetByUser = false;
//         }
//     }

//     if (d->pageMap.contains(id))
//         emit pageRemoved(id);

//     if (!d->history.contains(id)) {
//         // Case 1: removing a page not in the history
//         removedPage = d->pageMap.take(id);
//         d->updateCurrentPage();
//     } else if (id != d->current) {
//         // Case 2: removing a page in the history before the current page
//         removedPage = d->pageMap.take(id);
//         d->history.removeOne(id);
//         d->_q_updateButtonStates();
//     } else if (d->history.size() == 1) {
//         // Case 3: removing the current page which is the first (and only) one in the history
//         d->reset();
//         removedPage = d->pageMap.take(id);
//         if (d->pageMap.isEmpty())
//             d->updateCurrentPage();
//         else
//             restart();
//     } else {
//         // Case 4: removing the current page which is not the first one in the history
//         back();
//         removedPage = d->pageMap.take(id);
//         d->updateCurrentPage();
//     }

//     if (removedPage) {
//         if (removedPage->d_func()->initialized) {
//             cleanupPage(id);
//             removedPage->d_func()->initialized = false;
//         }

//         d->pageVBoxLayout->removeWidget(removedPage);

//         for (int i = d->fields.size() - 1; i >= 0; --i) {
//             if (d->fields.at(i).page == removedPage) {
//                 removedPage->d_func()->pendingFields += d->fields.at(i);
//                 d->removeFieldAt(i);
//             }
//         }
//     }
// }

// /*!
//     \fn QAWizardPage *QAWizard::page(int id) const

//     Returns the page with the given \a id, or \nullptr if there is no
//     such page.

//     \sa addPage(), setPage()
// */
// QAWizardPage *QAWizard::page(int theid) const
// {
//     Q_D(const QAWizard);
//     return d->pageMap.value(theid);
// }

// /*!
//     \fn bool QAWizard::hasVisitedPage(int id) const

//     Returns \c true if the page history contains page \a id; otherwise,
//     returns \c false.

//     Pressing \uicontrol Back marks the current page as "unvisited" again.

//     \sa visitedIds()
// */
// bool QAWizard::hasVisitedPage(int theid) const
// {
//     Q_D(const QAWizard);
//     return d->history.contains(theid);
// }

// /*!
//     \since 5.15

//     Returns the list of IDs of visited pages, in the order in which the pages
//     were visited.

//     \sa hasVisitedPage()
// */
// QList<int> QAWizard::visitedIds() const
// {
//     Q_D(const QAWizard);
//     return d->history;
// }

// /*!
//     Returns the list of page IDs.
//    \since 4.5
// */
// QList<int> QAWizard::pageIds() const
// {
//   Q_D(const QAWizard);
//   return d->pageMap.keys();
// }

// /*!
//     \property QAWizard::startId
//     \brief the ID of the first page

//     If this property isn't explicitly set, this property defaults to
//     the lowest page ID in this wizard, or -1 if no page has been
//     inserted yet.

//     \sa restart(), nextId()
// */
// void QAWizard::setStartId(int theid)
// {
//     Q_D(QAWizard);
//     int newStart = theid;
//     if (theid == -1)
//         newStart = d->pageMap.size() ? d->pageMap.constBegin().key() : -1;

//     if (d->start == newStart) {
//         d->startSetByUser = theid != -1;
//         return;
//     }

//     if (Q_UNLIKELY(!d->pageMap.contains(newStart))) {
//         qWarning("QAWizard::setStartId: Invalid page ID %d", newStart);
//         return;
//     }
//     d->start = newStart;
//     d->startSetByUser = theid != -1;
// }

// int QAWizard::startId() const
// {
//     Q_D(const QAWizard);
//     return d->start;
// }

// /*!
//     Returns a pointer to the current page, or \nullptr if there is no
//     current page (e.g., before the wizard is shown).

//     This is equivalent to calling page(currentId()).

//     \sa page(), currentId(), restart()
// */
// QAWizardPage *QAWizard::currentPage() const
// {
//     Q_D(const QAWizard);
//     return page(d->current);
// }

// /*!
//     \property QAWizard::currentId
//     \brief the ID of the current page

//     This property cannot be set directly. To change the current page,
//     call next(), back(), or restart().

//     By default, this property has a value of -1, indicating that no page is
//     currently shown.

//     \sa currentPage()
// */
// int QAWizard::currentId() const
// {
//     Q_D(const QAWizard);
//     return d->current;
// }

// /*!
//     Sets the value of the field called \a name to \a value.

//     This function can be used to set fields on any page of the wizard.

//     \sa QAWizardPage::registerField(), QAWizardPage::setField(), field()
// */
// void QAWizard::setField(const QString &name, const QVariant &value)
// {
//     Q_D(QAWizard);

//     int index = d->fieldIndexMap.value(name, -1);
//     if (Q_UNLIKELY(index == -1)) {
//         qWarning("QAWizard::setField: No such field '%ls'", qUtf16Printable(name));
//         return;
//     }

//     const QAWizardField &field = d->fields.at(index);
//     if (Q_UNLIKELY(!field.object->setProperty(field.property, value)))
//         qWarning("QAWizard::setField: Couldn't write to property '%s'",
//                  field.property.constData());
// }

// /*!
//     Returns the value of the field called \a name.

//     This function can be used to access fields on any page of the wizard.

//     \sa QAWizardPage::registerField(), QAWizardPage::field(), setField()
// */
// QVariant QAWizard::field(const QString &name) const
// {
//     Q_D(const QAWizard);

//     int index = d->fieldIndexMap.value(name, -1);
//     if (Q_UNLIKELY(index == -1)) {
//         qWarning("QAWizard::field: No such field '%ls'", qUtf16Printable(name));
//         return QVariant();
//     }

//     const QAWizardField &field = d->fields.at(index);
//     return field.object->property(field.property);
// }

// /*!
//     \property QAWizard::wizardStyle
//     \brief the look and feel of the wizard

//     By default, QAWizard uses the AeroStyle on a Windows Vista system with alpha compositing
//     enabled, regardless of the current widget style. If this is not the case, the default
//     wizard style depends on the current widget style as follows: MacStyle is the default if
//     the current widget style is QMacStyle, ModernStyle is the default if the current widget
//     style is QWindowsStyle, and ClassicStyle is the default in all other cases.

//     \sa {Wizard Look and Feel}, options
// */
// void QAWizard::setWizardStyle(WizardStyle style)
// {
//     Q_D(QAWizard);

//     const bool styleChange = style != d->wizStyle;

// #if QT_CONFIG(style_windowsvista)
//     const bool aeroStyleChange =
//         d->vistaInitPending || d->vistaStateChanged || (styleChange && (style == AeroStyle || d->wizStyle == AeroStyle));
//     d->vistaStateChanged = false;
//     d->vistaInitPending = false;
// #endif

//     if (styleChange
// #if QT_CONFIG(style_windowsvista)
//         || aeroStyleChange
// #endif
//         ) {
//         d->disableUpdates();
//         d->wizStyle = style;
//         d->updateButtonTexts();
// #if QT_CONFIG(style_windowsvista)
//         if (aeroStyleChange) {
//             //Send a resizeevent since the antiflicker widget probably needs a new size
//             //because of the backbutton in the window title
//             QResizeEvent ev(geometry().size(), geometry().size());
//             QCoreApplication::sendEvent(this, &ev);
//         }
// #endif
//         d->updateLayout();
//         updateGeometry();
//         d->enableUpdates();
// #if QT_CONFIG(style_windowsvista)
//         // Delay initialization when activating Aero style fails due to missing native window.
//         if (aeroStyleChange && !d->handleAeroStyleChange() && d->wizStyle == AeroStyle)
//             d->vistaInitPending = true;
// #endif
//     }
// }

// QAWizard::WizardStyle QAWizard::wizardStyle() const
// {
//     Q_D(const QAWizard);
//     return d->wizStyle;
// }

// /*!
//     Sets the given \a option to be enabled if \a on is true;
//     otherwise, clears the given \a option.

//     \sa options, testOption(), setWizardStyle()
// */
// void QAWizard::setOption(WizardOption option, bool on)
// {
//     Q_D(QAWizard);
//     if (!(d->opts & option) != !on)
//         setOptions(d->opts ^ option);
// }

// /*!
//     Returns \c true if the given \a option is enabled; otherwise, returns
//     false.

//     \sa options, setOption(), setWizardStyle()
// */
// bool QAWizard::testOption(WizardOption option) const
// {
//     Q_D(const QAWizard);
//     return (d->opts & option) != 0;
// }

// /*!
//     \property QAWizard::options
//     \brief the various options that affect the look and feel of the wizard

//     By default, the following options are set (depending on the platform):

//     \list
//     \li Windows: HelpButtonOnRight.
//     \li \macos: NoDefaultButton and NoCancelButton.
//     \li X11 and QWS (Qt for Embedded Linux): none.
//     \endlist

//     \sa wizardStyle
// */
// void QAWizard::setOptions(WizardOptions options)
// {
//     Q_D(QAWizard);

//     WizardOptions changed = (options ^ d->opts);
//     if (!changed)
//         return;

//     d->disableUpdates();

//     d->opts = options;
//     if ((changed & IndependentPages) && !(d->opts & IndependentPages))
//         d->cleanupPagesNotInHistory();

//     if (changed & (NoDefaultButton | HaveHelpButton | HelpButtonOnRight | NoCancelButton
//                    | CancelButtonOnLeft | HaveCustomButton1 | HaveCustomButton2
//                    | HaveCustomButton3)) {
//         d->updateButtonLayout();
//     } else if (changed & (NoBackButtonOnStartPage | NoBackButtonOnLastPage
//                           | HaveNextButtonOnLastPage | HaveFinishButtonOnEarlyPages
//                           | DisabledBackButtonOnLastPage | NoCancelButtonOnLastPage)) {
//         d->_q_updateButtonStates();
//     }

//     d->enableUpdates();
//     d->updateLayout();
// }

// QAWizard::WizardOptions QAWizard::options() const
// {
//     Q_D(const QAWizard);
//     return d->opts;
// }

// /*!
//     Sets the text on button \a which to be \a text.

//     By default, the text on buttons depends on the wizardStyle. For
//     example, on \macos, the \uicontrol Next button is called \uicontrol
//     Continue.

//     To add extra buttons to the wizard (e.g., a \uicontrol Print button),
//     one way is to call setButtonText() with CustomButton1,
//     CustomButton2, or CustomButton3 to set their text, and make the
//     buttons visible using the HaveCustomButton1, HaveCustomButton2,
//     and/or HaveCustomButton3 options.

//     Button texts may also be set on a per-page basis using QAWizardPage::setButtonText().

//     \sa setButton(), button(), setButtonLayout(), setOptions(), QAWizardPage::setButtonText()
// */
// void QAWizard::setButtonText(WizardButton which, const QString &text)
// {
//     Q_D(QAWizard);

//     if (!d->ensureButton(which))
//         return;

//     d->buttonCustomTexts.insert(which, text);

//     if (!currentPage() || !currentPage()->d_func()->buttonCustomTexts.contains(which))
//         d->btns[which]->setText(text);
// }

// /*!
//     Returns the text on button \a which.

//     If a text has ben set using setButtonText(), this text is returned.

//     By default, the text on buttons depends on the wizardStyle. For
//     example, on \macos, the \uicontrol Next button is called \uicontrol
//     Continue.

//     \sa button(), setButton(), setButtonText(), QAWizardPage::buttonText(),
//     QAWizardPage::setButtonText()
// */
// QString QAWizard::buttonText(WizardButton which) const
// {
//     Q_D(const QAWizard);

//     if (!d->ensureButton(which))
//         return QString();

//     if (d->buttonCustomTexts.contains(which))
//         return d->buttonCustomTexts.value(which);

//     const QString defText = buttonDefaultText(d->wizStyle, which, d);
//     if (!defText.isNull())
//         return defText;

//     return d->btns[which]->text();
// }

// /*!
//     Sets the order in which buttons are displayed to \a layout, where
//     \a layout is a list of \l{WizardButton}s.

//     The default layout depends on the options (e.g., whether
//     HelpButtonOnRight) that are set. You can call this function if
//     you need more control over the buttons' layout than what \l
//     options already provides.

//     You can specify horizontal stretches in the layout using \l
//     Stretch.

//     Example:

//     \snippet code/src_gui_dialogs_QAWizard.cpp 1

//     \sa setButton(), setButtonText(), setOptions()
// */
// void QAWizard::setButtonLayout(const QList<WizardButton> &layout)
// {
//     Q_D(QAWizard);

//     for (int i = 0; i < layout.size(); ++i) {
//         WizardButton button1 = layout.at(i);

//         if (button1 == NoButton || button1 == Stretch)
//             continue;
//         if (!d->ensureButton(button1))
//             return;

//         // O(n^2), but n is very small
//         for (int j = 0; j < i; ++j) {
//             WizardButton button2 = layout.at(j);
//             if (Q_UNLIKELY(button2 == button1)) {
//                 qWarning("QAWizard::setButtonLayout: Duplicate button in layout");
//                 return;
//             }
//         }
//     }

//     d->buttonsHaveCustomLayout = true;
//     d->buttonsCustomLayout = layout;
//     d->updateButtonLayout();
// }

// /*!
//     Sets the button corresponding to role \a which to \a button.

//     To add extra buttons to the wizard (e.g., a \uicontrol Print button),
//     one way is to call setButton() with CustomButton1 to
//     CustomButton3, and make the buttons visible using the
//     HaveCustomButton1 to HaveCustomButton3 options.

//     \sa setButtonText(), setButtonLayout(), options
// */
// void QAWizard::setButton(WizardButton which, QAbstractButton *button)
// {
//     Q_D(QAWizard);

//     if (uint(which) >= NButtons || d->btns[which] == button)
//         return;

//     if (QAbstractButton *oldButton = d->btns[which]) {
//         d->buttonLayout->removeWidget(oldButton);
//         delete oldButton;
//     }

//     d->btns[which] = button;
//     if (button) {
//         button->setParent(d->antiFlickerWidget);
//         d->buttonCustomTexts.insert(which, button->text());
//         d->connectButton(which);
//     } else {
//         d->buttonCustomTexts.remove(which); // ### what about page-specific texts set for 'which'
//         d->ensureButton(which);             // (QAWizardPage::setButtonText())? Clear them as well?
//     }

//     d->updateButtonLayout();
// }

// /*!
//     Returns the button corresponding to role \a which.

//     \sa setButton(), setButtonText()
// */
// QAbstractButton *QAWizard::button(WizardButton which) const
// {
//     Q_D(const QAWizard);
// #if QT_CONFIG(style_windowsvista)
//     if (d->wizStyle == AeroStyle && which == BackButton)
//         return d->vistaHelper->backButton();
// #endif
//     if (!d->ensureButton(which))
//         return nullptr;
//     return d->btns[which];
// }

// /*!
//     \property QAWizard::titleFormat
//     \brief the text format used by page titles

//     The default format is Qt::AutoText.

//     \sa QAWizardPage::title, subTitleFormat
// */
// void QAWizard::setTitleFormat(Qt::TextFormat format)
// {
//     Q_D(QAWizard);
//     d->titleFmt = format;
//     d->updateLayout();
// }

// Qt::TextFormat QAWizard::titleFormat() const
// {
//     Q_D(const QAWizard);
//     return d->titleFmt;
// }

// /*!
//     \property QAWizard::subTitleFormat
//     \brief the text format used by page subtitles

//     The default format is Qt::AutoText.

//     \sa QAWizardPage::title, titleFormat
// */
// void QAWizard::setSubTitleFormat(Qt::TextFormat format)
// {
//     Q_D(QAWizard);
//     d->subTitleFmt = format;
//     d->updateLayout();
// }

// Qt::TextFormat QAWizard::subTitleFormat() const
// {
//     Q_D(const QAWizard);
//     return d->subTitleFmt;
// }

// /*!
//     Sets the pixmap for role \a which to \a pixmap.

//     The pixmaps are used by QAWizard when displaying a page. Which
//     pixmaps are actually used depend on the \l{Wizard Look and
//     Feel}{wizard style}.

//     Pixmaps can also be set for a specific page using
//     QAWizardPage::setPixmap().

//     \sa QAWizardPage::setPixmap(), {Elements of a Wizard Page}
// */
// void QAWizard::setPixmap(WizardPixmap which, const QPixmap &pixmap)
// {
//     Q_D(QAWizard);
//     Q_ASSERT(uint(which) < NPixmaps);
//     d->defaultPixmaps[which] = pixmap;
//     d->updatePixmap(which);
// }

// /*!
//     Returns the pixmap set for role \a which.

//     By default, the only pixmap that is set is the BackgroundPixmap on
//     \macos.

//     \sa QAWizardPage::pixmap(), {Elements of a Wizard Page}
// */
// QPixmap QAWizard::pixmap(WizardPixmap which) const
// {
//     Q_D(const QAWizard);
//     Q_ASSERT(uint(which) < NPixmaps);
// #ifdef Q_OS_MACOS
//     if (which == BackgroundPixmap && d->defaultPixmaps[BackgroundPixmap].isNull())
//         d->defaultPixmaps[BackgroundPixmap] = d->findDefaultBackgroundPixmap();
// #endif
//     return d->defaultPixmaps[which];
// }

// /*!
//     Sets the default property for \a className to be \a property,
//     and the associated change signal to be \a changedSignal.

//     The default property is used when an instance of \a className (or
//     of one of its subclasses) is passed to
//     QAWizardPage::registerField() and no property is specified.

//     QAWizard knows the most common Qt widgets. For these (or their
//     subclasses), you don't need to specify a \a property or a \a
//     changedSignal. The table below lists these widgets:

//     \table
//     \header \li Widget          \li Property                            \li Change Notification Signal
//     \row    \li QAbstractButton \li bool \l{QAbstractButton::}{checked} \li \l{QAbstractButton::}{toggled()}
//     \row    \li QAbstractSlider \li int \l{QAbstractSlider::}{value}    \li \l{QAbstractSlider::}{valueChanged()}
//     \row    \li QComboBox       \li int \l{QComboBox::}{currentIndex}   \li \l{QComboBox::}{currentIndexChanged()}
//     \row    \li QDateTimeEdit   \li QDateTime \l{QDateTimeEdit::}{dateTime} \li \l{QDateTimeEdit::}{dateTimeChanged()}
//     \row    \li QLineEdit       \li QString \l{QLineEdit::}{text}       \li \l{QLineEdit::}{textChanged()}
//     \row    \li QListWidget     \li int \l{QListWidget::}{currentRow}   \li \l{QListWidget::}{currentRowChanged()}
//     \row    \li QSpinBox        \li int \l{QSpinBox::}{value}           \li \l{QSpinBox::}{valueChanged()}
//     \endtable

//     \sa QAWizardPage::registerField()
// */
// void QAWizard::setDefaultProperty(const char *className, const char *property,
//                                  const char *changedSignal)
// {
//     Q_D(QAWizard);
//     for (int i = d->defaultPropertyTable.size() - 1; i >= 0; --i) {
//         if (qstrcmp(d->defaultPropertyTable.at(i).className, className) == 0) {
//             d->defaultPropertyTable.remove(i);
//             break;
//         }
//     }
//     d->defaultPropertyTable.append(QAWizardDefaultProperty(className, property, changedSignal));
// }

// /*!
//     \since 4.7

//     Sets the given \a widget to be shown on the left side of the wizard.
//     For styles which use the WatermarkPixmap (ClassicStyle and ModernStyle)
//     the side widget is displayed on top of the watermark, for other styles
//     or when the watermark is not provided the side widget is displayed
//     on the left side of the wizard.

//     Passing \nullptr shows no side widget.

//     When the \a widget is not \nullptr the wizard reparents it.

//     Any previous side widget is hidden.

//     You may call setSideWidget() with the same widget at different
//     times.

//     All widgets set here will be deleted by the wizard when it is
//     destroyed unless you separately reparent the widget after setting
//     some other side widget (or \nullptr).

//     By default, no side widget is present.
// */
// void QAWizard::setSideWidget(QWidget *widget)
// {
//     Q_D(QAWizard);

//     d->sideWidget = widget;
//     if (d->watermarkLabel) {
//         d->watermarkLabel->setSideWidget(widget);
//         d->updateLayout();
//     }
// }

// /*!
//     \since 4.7

//     Returns the widget on the left side of the wizard or \nullptr.

//     By default, no side widget is present.
// */
// QWidget *QAWizard::sideWidget() const
// {
//     Q_D(const QAWizard);

//     return d->sideWidget;
// }

// /*!
//     \reimp
// */
// void QAWizard::setVisible(bool visible)
// {
//     Q_D(QAWizard);
//     if (visible) {
//         if (d->current == -1)
//             restart();
//     }
//     QDialog::setVisible(visible);
// }

// /*!
//     \reimp
// */
// QSize QAWizard::sizeHint() const
// {
//     Q_D(const QAWizard);
//     QSize result = d->mainLayout->totalSizeHint();
//     QSize extra(500, 360);
//     if (d->wizStyle == MacStyle && d->current != -1) {
//         QSize pixmap(currentPage()->pixmap(BackgroundPixmap).size());
//         extra.setWidth(616);
//         if (!pixmap.isNull()) {
//             extra.setHeight(pixmap.height());

//             /*
//                 The width isn't always reliable as a size hint, as
//                 some wizard backgrounds just cover the leftmost area.
//                 Use a rule of thumb to determine if the width is
//                 reliable or not.
//             */
//             if (pixmap.width() >= pixmap.height())
//                 extra.setWidth(pixmap.width());
//         }
//     }
//     return result.expandedTo(extra);
// }

// /*!
//     \fn void QAWizard::currentIdChanged(int id)

//     This signal is emitted when the current page changes, with the new
//     current \a id.

//     \sa currentId(), currentPage()
// */

// /*!
//     \fn void QAWizard::pageAdded(int id)

//     \since 4.7

//     This signal is emitted whenever a page is added to the
//     wizard. The page's \a id is passed as parameter.

//     \sa addPage(), setPage(), startId()
// */

// /*!
//     \fn void QAWizard::pageRemoved(int id)

//     \since 4.7

//     This signal is emitted whenever a page is removed from the
//     wizard. The page's \a id is passed as parameter.

//     \sa removePage(), startId()
// */

// /*!
//     \fn void QAWizard::helpRequested()

//     This signal is emitted when the user clicks the \uicontrol Help button.

//     By default, no \uicontrol Help button is shown. Call
//     setOption(HaveHelpButton, true) to have one.

//     Example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 0
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.cpp 5
//     \snippet dialogs/licensewizard/licensewizard.cpp 7
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.cpp 8
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 10
//     \dots
//     \snippet dialogs/licensewizard/licensewizard.cpp 12
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 14
//     \codeline
//     \snippet dialogs/licensewizard/licensewizard.cpp 15

//     \sa customButtonClicked()
// */

// /*!
//     \fn void QAWizard::customButtonClicked(int which)

//     This signal is emitted when the user clicks a custom button. \a
//     which can be CustomButton1, CustomButton2, or CustomButton3.

//     By default, no custom button is shown. Call setOption() with
//     HaveCustomButton1, HaveCustomButton2, or HaveCustomButton3 to have
//     one, and use setButtonText() or setButton() to configure it.

//     \sa helpRequested()
// */

// /*!
//     Goes back to the previous page.

//     This is equivalent to pressing the \uicontrol Back button.

//     \sa next(), accept(), reject(), restart()
// */
// void QAWizard::back()
// {
//     Q_D(QAWizard);
//     int n = d->history.size() - 2;
//     if (n < 0)
//         return;
//     d->switchToPage(d->history.at(n), QAWizardPrivate::Backward);
// }

// /*!
//     Advances to the next page.

//     This is equivalent to pressing the \uicontrol Next or \uicontrol Commit button.

//     \sa nextId(), back(), accept(), reject(), restart()
// */
// void QAWizard::next()
// {
//     Q_D(QAWizard);

//     if (d->current == -1)
//         return;

//     if (validateCurrentPage()) {
//         int next = nextId();
//         if (next != -1) {
//             if (Q_UNLIKELY(d->history.contains(next))) {
//                 qWarning("QAWizard::next: Page %d already met", next);
//                 return;
//             }
//             if (Q_UNLIKELY(!d->pageMap.contains(next))) {
//                 qWarning("QAWizard::next: No such page %d", next);
//                 return;
//             }
//             d->switchToPage(next, QAWizardPrivate::Forward);
//         }
//     }
// }

// /*!
//     Sets currentId to \a id, without visiting the pages between currentId and \a id.

//     Returns without page change, if
//     \list
//     \li wizard holds no pages
//     \li current page is invalid
//     \li given page equals currentId()
//     \li given page is out of range
//     \endlist

//     Note: If pages have been forward skipped and \a id is 0, page visiting history
//     will be deleted
// */

// void QAWizard::setCurrentId(int id)
// {
//     Q_D(QAWizard);

//     if (d->current == -1)
//         return;

//     if (currentId() == id)
//         return;

//     if (!validateCurrentPage())
//         return;

//     if (id < 0 || Q_UNLIKELY(!d->pageMap.contains(id))) {
//         qWarning("QAWizard::setCurrentId: No such page: %d", id);
//         return;
//     }

//     d->switchToPage(id, (id < currentId()) ? QAWizardPrivate::Backward : QAWizardPrivate::Forward);
// }

// /*!
//     Restarts the wizard at the start page. This function is called automatically when the
//     wizard is shown.

//     \sa startId()
// */
// void QAWizard::restart()
// {
//     Q_D(QAWizard);
//     d->disableUpdates();
//     d->reset();
//     d->switchToPage(startId(), QAWizardPrivate::Forward);
//     d->enableUpdates();
// }

// /*!
//     \reimp
// */
// bool QAWizard::event(QEvent *event)
// {
//     Q_D(QAWizard);
//     if (event->type() == QEvent::StyleChange) { // Propagate style
//         d->setStyle(style());
//         d->updateLayout();
//     } else if (event->type() == QEvent::PaletteChange) { // Emitted on theme change
//         d->updatePalette();
//     }
// #if QT_CONFIG(style_windowsvista)
//     else if (event->type() == QEvent::Show && d->vistaInitPending) {
//         d->vistaInitPending = false;
//         d->wizStyle = AeroStyle;
//         d->handleAeroStyleChange();
//     }
//     else if (d->isVistaThemeEnabled()) {
//         if (event->type() == QEvent::Resize
//                 || event->type() == QEvent::LayoutDirectionChange) {
//             const int buttonLeft = (layoutDirection() == Qt::RightToLeft
//                                     ? width() - d->vistaHelper->backButton()->sizeHint().width()
//                                     : 0);

//             d->vistaHelper->backButton()->move(buttonLeft,
//                                                d->vistaHelper->backButton()->y());
//         }

//         d->vistaHelper->mouseEvent(event);
//     }
// #endif
//     return QDialog::event(event);
// }

// /*!
//     \reimp
// */
// void QAWizard::resizeEvent(QResizeEvent *event)
// {
//     Q_D(QAWizard);
//     int heightOffset = 0;
// #if QT_CONFIG(style_windowsvista)
//     if (d->isVistaThemeEnabled()) {
//         heightOffset = d->vistaHelper->topOffset(this);
//         heightOffset += d->vistaHelper->titleBarSize();
//     }
// #endif
//     d->antiFlickerWidget->resize(event->size().width(), event->size().height() - heightOffset);
// #if QT_CONFIG(style_windowsvista)
//     if (d->isVistaThemeEnabled())
//         d->vistaHelper->resizeEvent(event);
// #endif
//     QDialog::resizeEvent(event);
// }

// /*!
//     \reimp
// */
// void QAWizard::paintEvent(QPaintEvent * event)
// {
//     Q_D(QAWizard);
//     if (d->wizStyle == MacStyle && currentPage()) {
//         QPixmap backgroundPixmap = currentPage()->pixmap(BackgroundPixmap);
//         if (backgroundPixmap.isNull())
//             return;

//         QStylePainter painter(this);
//         painter.drawPixmap(0, (height() - backgroundPixmap.height()) / 2, backgroundPixmap);
//     }
// #if QT_CONFIG(style_windowsvista)
//     else if (d->isVistaThemeEnabled()) {
//         d->vistaHelper->paintEvent(event);
//     }
// #else
//     Q_UNUSED(event);
// #endif
// }

// #if defined(Q_OS_WIN) || defined(Q_QDOC)
// /*!
//     \reimp
// */
// bool QAWizard::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
// {
// #if QT_CONFIG(style_windowsvista)
//     Q_D(QAWizard);
//     if (d->isVistaThemeEnabled() && eventType == "windows_generic_MSG") {
//         MSG *windowsMessage = static_cast<MSG *>(message);
//         const bool winEventResult = d->vistaHelper->handleWinEvent(windowsMessage, result);
//         if (d->vistaDirty) {
//             // QTBUG-78300: When Qt::AA_NativeWindows is set, delay further
//             // window creation until after the platform window creation events.
//             if (windowsMessage->message == WM_GETICON) {
//                 d->vistaStateChanged = true;
//                 d->vistaDirty = false;
//                 setWizardStyle(AeroStyle);
//             }
//         }
//         return winEventResult;
//     } else {
//         return QDialog::nativeEvent(eventType, message, result);
//     }
// #else
//     return QDialog::nativeEvent(eventType, message, result);
// #endif
// }
// #endif

// /*!
//     \reimp
// */
// void QAWizard::done(int result)
// {
//     Q_D(QAWizard);
//     // canceling leaves the wizard in a known state
//     if (result == Rejected) {
//         d->reset();
//     } else {
//         if (!validateCurrentPage())
//             return;
//     }
//     QDialog::done(result);
// }

// /*!
//     \fn void QAWizard::initializePage(int id)

//     This virtual function is called by QAWizard to prepare page \a id
//     just before it is shown either as a result of QAWizard::restart()
//     being called, or as a result of the user clicking \uicontrol Next. (However, if the \l
//     QAWizard::IndependentPages option is set, this function is only
//     called the first time the page is shown.)

//     By reimplementing this function, you can ensure that the page's
//     fields are properly initialized based on fields from previous
//     pages.

//     The default implementation calls QAWizardPage::initializePage() on
//     page(\a id).

//     \sa QAWizardPage::initializePage(), cleanupPage()
// */
// void QAWizard::initializePage(int theid)
// {
//     QAWizardPage *page = this->page(theid);
//     if (page)
//         page->initializePage();
// }

// /*!
//     \fn void QAWizard::cleanupPage(int id)

//     This virtual function is called by QAWizard to clean up page \a id just before the
//     user leaves it by clicking \uicontrol Back (unless the \l QAWizard::IndependentPages option is set).

//     The default implementation calls QAWizardPage::cleanupPage() on
//     page(\a id).

//     \sa QAWizardPage::cleanupPage(), initializePage()
// */
// void QAWizard::cleanupPage(int theid)
// {
//     QAWizardPage *page = this->page(theid);
//     if (page)
//         page->cleanupPage();
// }

// /*!
//     This virtual function is called by QAWizard when the user clicks
//     \uicontrol Next or \uicontrol Finish to perform some last-minute validation.
//     If it returns \c true, the next page is shown (or the wizard
//     finishes); otherwise, the current page stays up.

//     The default implementation calls QAWizardPage::validatePage() on
//     the currentPage().

//     When possible, it is usually better style to disable the \uicontrol
//     Next or \uicontrol Finish button (by specifying \l{mandatory fields} or
//     by reimplementing QAWizardPage::isComplete()) than to reimplement
//     validateCurrentPage().

//     \sa QAWizardPage::validatePage(), currentPage()
// */
// bool QAWizard::validateCurrentPage()
// {
//     QAWizardPage *page = currentPage();
//     if (!page)
//         return true;

//     return page->validatePage();
// }

// /*!
//     This virtual function is called by QAWizard to find out which page
//     to show when the user clicks the \uicontrol Next button.

//     The return value is the ID of the next page, or -1 if no page follows.

//     The default implementation calls QAWizardPage::nextId() on the
//     currentPage().

//     By reimplementing this function, you can specify a dynamic page
//     order.

//     \sa QAWizardPage::nextId(), currentPage()
// */
// int QAWizard::nextId() const
// {
//     const QAWizardPage *page = currentPage();
//     if (!page)
//         return -1;

//     return page->nextId();
// }

// /*!
//     \class QAWizardPage
//     \since 4.3
//     \brief The QAWizardPage class is the base class for wizard pages.

//     \inmodule QtWidgets

//     QAWizard represents a wizard. Each page is a QAWizardPage. When
//     you create your own wizards, you can use QAWizardPage directly,
//     or you can subclass it for more control.

//     A page has the following attributes, which are rendered by
//     QAWizard: a \l title, a \l subTitle, and a \l{setPixmap()}{set of
//     pixmaps}. See \l{Elements of a Wizard Page} for details. Once a
//     page is added to the wizard (using QAWizard::addPage() or
//     QAWizard::setPage()), wizard() returns a pointer to the
//     associated QAWizard object.

//     Page provides five virtual functions that can be reimplemented to
//     provide custom behavior:

//     \list
//     \li initializePage() is called to initialize the page's contents
//        when the user clicks the wizard's \uicontrol Next button. If you
//        want to derive the page's default from what the user entered
//        on previous pages, this is the function to reimplement.
//     \li cleanupPage() is called to reset the page's contents when the
//        user clicks the wizard's \uicontrol Back button.
//     \li validatePage() validates the page when the user clicks \uicontrol
//        Next or \uicontrol Finish. It is often used to show an error message
//        if the user has entered incomplete or invalid information.
//     \li nextId() returns the ID of the next page. It is useful when
//        \l{creating non-linear wizards}, which allow different
//        traversal paths based on the information provided by the user.
//     \li isComplete() is called to determine whether the \uicontrol Next
//        and/or \uicontrol Finish button should be enabled or disabled. If
//        you reimplement isComplete(), also make sure that
//        completeChanged() is emitted whenever the complete state
//        changes.
//     \endlist

//     Normally, the \uicontrol Next button and the \uicontrol Finish button of a
//     wizard are mutually exclusive. If isFinalPage() returns \c true, \uicontrol
//     Finish is available; otherwise, \uicontrol Next is available. By
//     default, isFinalPage() is true only when nextId() returns -1. If
//     you want to show \uicontrol Next and \uicontrol Final simultaneously for a
//     page (letting the user perform an "early finish"), call
//     setFinalPage(true) on that page. For wizards that support early
//     finishes, you might also want to set the
//     \l{QAWizard::}{HaveNextButtonOnLastPage} and
//     \l{QAWizard::}{HaveFinishButtonOnEarlyPages} options on the
//     wizard.

//     In many wizards, the contents of a page may affect the default
//     values of the fields of a later page. To make it easy to
//     communicate between pages, QAWizard supports a \l{Registering and
//     Using Fields}{"field" mechanism} that allows you to register a
//     field (e.g., a QLineEdit) on a page and to access its value from
//     any page. Fields are global to the entire wizard and make it easy
//     for any single page to access information stored by another page,
//     without having to put all the logic in QAWizard or having the
//     pages know explicitly about each other. Fields are registered
//     using registerField() and can be accessed at any time using
//     field() and setField().

//     \sa QAWizard, {Trivial Wizard Example}, {License Wizard Example}
// */

// /*!
//     Constructs a wizard page with the given \a parent.

//     When the page is inserted into a wizard using QAWizard::addPage()
//     or QAWizard::setPage(), the parent is automatically set to be the
//     wizard.

//     \sa wizard()
// */
// QAWizardPage::QAWizardPage(QWidget *parent)
//     : QWidget(*new QAWizardPagePrivate, parent, { })
// {
//     connect(this, SIGNAL(completeChanged()), this, SLOT(_q_updateCachedCompleteState()));
// }

// /*!
//     Destructor.
// */
// QAWizardPage::~QAWizardPage()
// {
// }

// /*!
//     \property QAWizardPage::title
//     \brief the title of the page

//     The title is shown by the QAWizard, above the actual page. All
//     pages should have a title.

//     The title may be plain text or HTML, depending on the value of the
//     \l{QAWizard::titleFormat} property.

//     By default, this property contains an empty string.

//     \sa subTitle, {Elements of a Wizard Page}
// */
// void QAWizardPage::setTitle(const QString &title)
// {
//     Q_D(QAWizardPage);
//     d->title = title;
//     if (d->wizard && d->wizard->currentPage() == this)
//         d->wizard->d_func()->updateLayout();
// }

// QString QAWizardPage::title() const
// {
//     Q_D(const QAWizardPage);
//     return d->title;
// }

// /*!
//     \property QAWizardPage::subTitle
//     \brief the subtitle of the page

//     The subtitle is shown by the QAWizard, between the title and the
//     actual page. Subtitles are optional. In
//     \l{QAWizard::ClassicStyle}{ClassicStyle} and
//     \l{QAWizard::ModernStyle}{ModernStyle}, using subtitles is
//     necessary to make the header appear. In
//     \l{QAWizard::MacStyle}{MacStyle}, the subtitle is shown as a text
//     label just above the actual page.

//     The subtitle may be plain text or HTML, depending on the value of
//     the \l{QAWizard::subTitleFormat} property.

//     By default, this property contains an empty string.

//     \sa title, QAWizard::IgnoreSubTitles, {Elements of a Wizard Page}
// */
// void QAWizardPage::setSubTitle(const QString &subTitle)
// {
//     Q_D(QAWizardPage);
//     d->subTitle = subTitle;
//     if (d->wizard && d->wizard->currentPage() == this)
//         d->wizard->d_func()->updateLayout();
// }

// QString QAWizardPage::subTitle() const
// {
//     Q_D(const QAWizardPage);
//     return d->subTitle;
// }

// /*!
//     Sets the pixmap for role \a which to \a pixmap.

//     The pixmaps are used by QAWizard when displaying a page. Which
//     pixmaps are actually used depend on the \l{Wizard Look and
//     Feel}{wizard style}.

//     Pixmaps can also be set for the entire wizard using
//     QAWizard::setPixmap(), in which case they apply for all pages that
//     don't specify a pixmap.

//     \sa QAWizard::setPixmap(), {Elements of a Wizard Page}
// */
// void QAWizardPage::setPixmap(QAWizard::WizardPixmap which, const QPixmap &pixmap)
// {
//     Q_D(QAWizardPage);
//     Q_ASSERT(uint(which) < QAWizard::NPixmaps);
//     d->pixmaps[which] = pixmap;
//     if (d->wizard && d->wizard->currentPage() == this)
//         d->wizard->d_func()->updatePixmap(which);
// }

// /*!
//     Returns the pixmap set for role \a which.

//     Pixmaps can also be set for the entire wizard using
//     QAWizard::setPixmap(), in which case they apply for all pages that
//     don't specify a pixmap.

//     \sa QAWizard::pixmap(), {Elements of a Wizard Page}
// */
// QPixmap QAWizardPage::pixmap(QAWizard::WizardPixmap which) const
// {
//     Q_D(const QAWizardPage);
//     Q_ASSERT(uint(which) < QAWizard::NPixmaps);

//     const QPixmap &pixmap = d->pixmaps[which];
//     if (!pixmap.isNull())
//         return pixmap;

//     if (wizard())
//         return wizard()->pixmap(which);

//     return pixmap;
// }

// /*!
//     This virtual function is called by QAWizard::initializePage() to
//     prepare the page just before it is shown either as a result of QAWizard::restart()
//     being called, or as a result of the user clicking \uicontrol Next.
//     (However, if the \l QAWizard::IndependentPages option is set, this function is only
//     called the first time the page is shown.)

//     By reimplementing this function, you can ensure that the page's
//     fields are properly initialized based on fields from previous
//     pages. For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 27

//     The default implementation does nothing.

//     \sa QAWizard::initializePage(), cleanupPage(), QAWizard::IndependentPages
// */
// void QAWizardPage::initializePage()
// {
// }

// /*!
//     This virtual function is called by QAWizard::cleanupPage() when
//     the user leaves the page by clicking \uicontrol Back (unless the \l QAWizard::IndependentPages
//     option is set).

//     The default implementation resets the page's fields to their
//     original values (the values they had before initializePage() was
//     called).

//     \sa QAWizard::cleanupPage(), initializePage(), QAWizard::IndependentPages
// */
// void QAWizardPage::cleanupPage()
// {
//     Q_D(QAWizardPage);
//     if (d->wizard) {
//         const QList<QAWizardField> &fields = d->wizard->d_func()->fields;
//         for (const auto &field : fields) {
//             if (field.page == this)
//                 field.object->setProperty(field.property, field.initialValue);
//         }
//     }
// }

// /*!
//     This virtual function is called by QAWizard::validateCurrentPage()
//     when the user clicks \uicontrol Next or \uicontrol Finish to perform some
//     last-minute validation. If it returns \c true, the next page is shown
//     (or the wizard finishes); otherwise, the current page stays up.

//     The default implementation returns \c true.

//     When possible, it is usually better style to disable the \uicontrol
//     Next or \uicontrol Finish button (by specifying \l{mandatory fields} or
//     reimplementing isComplete()) than to reimplement validatePage().

//     \sa QAWizard::validateCurrentPage(), isComplete()
// */
// bool QAWizardPage::validatePage()
// {
//     return true;
// }

// /*!
//     This virtual function is called by QAWizard to determine whether
//     the \uicontrol Next or \uicontrol Finish button should be enabled or
//     disabled.

//     The default implementation returns \c true if all \l{mandatory
//     fields} are filled; otherwise, it returns \c false.

//     If you reimplement this function, make sure to emit completeChanged(),
//     from the rest of your implementation, whenever the value of isComplete()
//     changes. This ensures that QAWizard updates the enabled or disabled state of
//     its buttons. An example of the reimplementation is
//     available \l{http://doc.qt.io/archives/qq/qq22-QAWizard.html#validatebeforeitstoolate}
//     {here}.

//     \sa completeChanged(), isFinalPage()
// */
// bool QAWizardPage::isComplete() const
// {
//     Q_D(const QAWizardPage);

//     if (!d->wizard)
//         return true;

//     const QList<QAWizardField> &wizardFields = d->wizard->d_func()->fields;
//     const auto end = wizardFields.crend();
//     for (auto it = wizardFields.crbegin(); it != end; ++it) {
//         const QAWizardField &field = *it;
//         if (field.page == this && field.mandatory) {
//             QVariant value = field.object->property(field.property);
//             if (value == field.initialValue)
//                 return false;

// #if QT_CONFIG(lineedit)
//             if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(field.object)) {
//                 if (!lineEdit->hasAcceptableInput())
//                     return false;
//             }
// #endif
// #if QT_CONFIG(spinbox)
//             if (QAbstractSpinBox *spinBox = qobject_cast<QAbstractSpinBox *>(field.object)) {
//                 if (!spinBox->hasAcceptableInput())
//                     return false;
//             }
// #endif
//         }
//     }
//     return true;
// }

// /*!
//     Explicitly sets this page to be final if \a finalPage is true.

//     After calling setFinalPage(true), isFinalPage() returns \c true and the \uicontrol
//     Finish button is visible (and enabled if isComplete() returns
//     true).

//     After calling setFinalPage(false), isFinalPage() returns \c true if
//     nextId() returns -1; otherwise, it returns \c false.

//     \sa isComplete(), QAWizard::HaveFinishButtonOnEarlyPages
// */
// void QAWizardPage::setFinalPage(bool finalPage)
// {
//     Q_D(QAWizardPage);
//     d->explicitlyFinal = finalPage;
//     QAWizard *wizard = this->wizard();
//     if (wizard && wizard->currentPage() == this)
//         wizard->d_func()->updateCurrentPage();
// }

// /*!
//     This function is called by QAWizard to determine whether the \uicontrol
//     Finish button should be shown for this page or not.

//     By default, it returns \c true if there is no next page
//     (i.e., nextId() returns -1); otherwise, it returns \c false.

//     By explicitly calling setFinalPage(true), you can let the user perform an
//     "early finish".

//     \sa isComplete(), QAWizard::HaveFinishButtonOnEarlyPages
// */
// bool QAWizardPage::isFinalPage() const
// {
//     Q_D(const QAWizardPage);
//     if (d->explicitlyFinal)
//         return true;

//     QAWizard *wizard = this->wizard();
//     if (wizard && wizard->currentPage() == this) {
//         // try to use the QAWizard implementation if possible
//         return wizard->nextId() == -1;
//     } else {
//         return nextId() == -1;
//     }
// }

// /*!
//     Sets this page to be a commit page if \a commitPage is true; otherwise,
//     sets it to be a normal page.

//     A commit page is a page that represents an action which cannot be undone
//     by clicking \uicontrol Back or \uicontrol Cancel.

//     A \uicontrol Commit button replaces the \uicontrol Next button on a commit page. Clicking this
//     button simply calls QAWizard::next() just like clicking \uicontrol Next does.

//     A page entered directly from a commit page has its \uicontrol Back button disabled.

//     \sa isCommitPage()
// */
// void QAWizardPage::setCommitPage(bool commitPage)
// {
//     Q_D(QAWizardPage);
//     d->commit = commitPage;
//     QAWizard *wizard = this->wizard();
//     if (wizard && wizard->currentPage() == this)
//         wizard->d_func()->updateCurrentPage();
// }

// /*!
//     Returns \c true if this page is a commit page; otherwise returns \c false.

//     \sa setCommitPage()
// */
// bool QAWizardPage::isCommitPage() const
// {
//     Q_D(const QAWizardPage);
//     return d->commit;
// }

// /*!
//     Sets the text on button \a which to be \a text on this page.

//     By default, the text on buttons depends on the QAWizard::wizardStyle,
//     but may be redefined for the wizard as a whole using QAWizard::setButtonText().

//     \sa buttonText(), QAWizard::setButtonText(), QAWizard::buttonText()
// */
// void QAWizardPage::setButtonText(QAWizard::WizardButton which, const QString &text)
// {
//     Q_D(QAWizardPage);
//     d->buttonCustomTexts.insert(which, text);
//     if (wizard() && wizard()->currentPage() == this && wizard()->d_func()->btns[which])
//         wizard()->d_func()->btns[which]->setText(text);
// }

// /*!
//     Returns the text on button \a which on this page.

//     If a text has ben set using setButtonText(), this text is returned.
//     Otherwise, if a text has been set using QAWizard::setButtonText(),
//     this text is returned.

//     By default, the text on buttons depends on the QAWizard::wizardStyle.
//     For example, on \macos, the \uicontrol Next button is called \uicontrol
//     Continue.

//     \sa setButtonText(), QAWizard::buttonText(), QAWizard::setButtonText()
// */
// QString QAWizardPage::buttonText(QAWizard::WizardButton which) const
// {
//     Q_D(const QAWizardPage);

//     if (d->buttonCustomTexts.contains(which))
//         return d->buttonCustomTexts.value(which);

//     if (wizard())
//         return wizard()->buttonText(which);

//     return QString();
// }

// /*!
//     This virtual function is called by QAWizard::nextId() to find
//     out which page to show when the user clicks the \uicontrol Next button.

//     The return value is the ID of the next page, or -1 if no page follows.

//     By default, this function returns the lowest ID greater than the ID
//     of the current page, or -1 if there is no such ID.

//     By reimplementing this function, you can specify a dynamic page
//     order. For example:

//     \snippet dialogs/licensewizard/licensewizard.cpp 18

//     \sa QAWizard::nextId()
// */
// int QAWizardPage::nextId() const
// {
//     Q_D(const QAWizardPage);

//     if (!d->wizard)
//         return -1;

//     bool foundCurrentPage = false;

//     const QAWizardPrivate::PageMap &pageMap = d->wizard->d_func()->pageMap;
//     QAWizardPrivate::PageMap::const_iterator i = pageMap.constBegin();
//     QAWizardPrivate::PageMap::const_iterator end = pageMap.constEnd();

//     for (; i != end; ++i) {
//         if (i.value() == this) {
//             foundCurrentPage = true;
//         } else if (foundCurrentPage) {
//             return i.key();
//         }
//     }
//     return -1;
// }

// /*!
//     \fn void QAWizardPage::completeChanged()

//     This signal is emitted whenever the complete state of the page
//     (i.e., the value of isComplete()) changes.

//     If you reimplement isComplete(), make sure to emit
//     completeChanged() whenever the value of isComplete() changes, to
//     ensure that QAWizard updates the enabled or disabled state of its
//     buttons.

//     \sa isComplete()
// */

// /*!
//     Sets the value of the field called \a name to \a value.

//     This function can be used to set fields on any page of the wizard.
//     It is equivalent to calling
//     wizard()->\l{QAWizard::setField()}{setField(\a name, \a value)}.

//     \sa QAWizard::setField(), field(), registerField()
// */
// void QAWizardPage::setField(const QString &name, const QVariant &value)
// {
//     Q_D(QAWizardPage);
//     if (!d->wizard)
//         return;
//     d->wizard->setField(name, value);
// }

// /*!
//     Returns the value of the field called \a name.

//     This function can be used to access fields on any page of the
//     wizard. It is equivalent to calling
//     wizard()->\l{QAWizard::field()}{field(\a name)}.

//     Example:

//     \snippet dialogs/licensewizard/licensewizard.cpp accessField

//     \sa QAWizard::field(), setField(), registerField()
// */
// QVariant QAWizardPage::field(const QString &name) const
// {
//     Q_D(const QAWizardPage);
//     if (!d->wizard)
//         return QVariant();
//     return d->wizard->field(name);
// }

// /*!
//     Creates a field called \a name associated with the given \a
//     property of the given \a widget. From then on, that property
//     becomes accessible using field() and setField().

//     Fields are global to the entire wizard and make it easy for any
//     single page to access information stored by another page, without
//     having to put all the logic in QAWizard or having the pages know
//     explicitly about each other.

//     If \a name ends with an asterisk (\c *), the field is a mandatory
//     field. When a page has mandatory fields, the \uicontrol Next and/or
//     \uicontrol Finish buttons are enabled only when all mandatory fields
//     are filled. This requires a \a changedSignal to be specified, to
//     tell QAWizard to recheck the value stored by the mandatory field.

//     QAWizard knows the most common Qt widgets. For these (or their
//     subclasses), you don't need to specify a \a property or a \a
//     changedSignal. The table below lists these widgets:

//     \table
//     \header \li Widget          \li Property                            \li Change Notification Signal
//     \row    \li QAbstractButton \li bool \l{QAbstractButton::}{checked} \li \l{QAbstractButton::}{toggled()}
//     \row    \li QAbstractSlider \li int \l{QAbstractSlider::}{value}    \li \l{QAbstractSlider::}{valueChanged()}
//     \row    \li QComboBox       \li int \l{QComboBox::}{currentIndex}   \li \l{QComboBox::}{currentIndexChanged()}
//     \row    \li QDateTimeEdit   \li QDateTime \l{QDateTimeEdit::}{dateTime} \li \l{QDateTimeEdit::}{dateTimeChanged()}
//     \row    \li QLineEdit       \li QString \l{QLineEdit::}{text}       \li \l{QLineEdit::}{textChanged()}
//     \row    \li QListWidget     \li int \l{QListWidget::}{currentRow}   \li \l{QListWidget::}{currentRowChanged()}
//     \row    \li QSpinBox        \li int \l{QSpinBox::}{value}           \li \l{QSpinBox::}{valueChanged()}
//     \endtable

//     You can use QAWizard::setDefaultProperty() to add entries to this
//     table or to override existing entries.

//     To consider a field "filled", QAWizard simply checks that their
//     current value doesn't equal their original value (the value they
//     had before initializePage() was called). For QLineEdit, it also
//     checks that
//     \l{QLineEdit::hasAcceptableInput()}{hasAcceptableInput()} returns
//     true, to honor any validator or mask.

//     QAWizard's mandatory field mechanism is provided for convenience.
//     It can be bypassed by reimplementing QAWizardPage::isComplete().

//     \sa field(), setField(), QAWizard::setDefaultProperty()
// */
// void QAWizardPage::registerField(const QString &name, QWidget *widget, const char *property,
//                                 const char *changedSignal)
// {
//     Q_D(QAWizardPage);
//     QAWizardField field(this, name, widget, property, changedSignal);
//     if (d->wizard) {
//         d->wizard->d_func()->addField(field);
//     } else {
//         d->pendingFields += field;
//     }
// }

// /*!
//     Returns the wizard associated with this page, or \nullptr if this page
//     hasn't been inserted into a QAWizard yet.

//     \sa QAWizard::addPage(), QAWizard::setPage()
// */
// QAWizard *QAWizardPage::wizard() const
// {
//     Q_D(const QAWizardPage);
//     return d->wizard;
// }

// QT_END_NAMESPACE

// #include "moc_QAWizard.cpp"

QAWizard::QAWizard(QWidget *parent, Qt::WindowFlags flags)
{

}

QAWizard::~QAWizard()
{

}

int QAWizard::addPage(QAWizardPage *page)
{
    return 0;
}

void QAWizard::setPage(int id, QAWizardPage *page)
{

}

void QAWizard::removePage(int id)
{

}

QAWizardPage *QAWizard::page(int id) const
{
    return nullptr;
}

bool QAWizard::hasVisitedPage(int id) const
{
    return false;
}

QList<int> QAWizard::visitedIds() const
{
    return {};
}

QList<int> QAWizard::pageIds() const
{
    return {};

}

void QAWizard::setStartId(int id)
{

}

int QAWizard::startId() const
{
    return 0;
}

QAWizardPage *QAWizard::currentPage() const
{
    return 0;

}

int QAWizard::currentId() const
{
    return 0;

}

bool QAWizard::validateCurrentPage()
{
    return 0;

}

int QAWizard::nextId() const
{
    return 0;

}

void QAWizard::setField(const QString &name, const QVariant &value)
{

}

QVariant QAWizard::field(const QString &name) const
{
    return 0;

}

void QAWizard::setWizardStyle(WizardStyle style)
{

}

QAWizard::WizardStyle QAWizard::wizardStyle() const
{
    return QAWizard::WizardStyle::ClassicStyle;

}

void QAWizard::setOption(WizardOption option, bool on)
{

}

bool QAWizard::testOption(WizardOption option) const
{
    return 0;

}

void QAWizard::setOptions(WizardOptions options)
{

}

QAWizard::WizardOptions QAWizard::options() const
{
    return QAWizard::WizardOptions(0);

}

void QAWizard::setButtonText(WizardButton which, const QString &text)
{

}

QString QAWizard::buttonText(WizardButton which) const
{
    return {};

}

void QAWizard::setButtonLayout(const QList<WizardButton> &layout)
{

}

void QAWizard::setButton(WizardButton which, QAbstractButton *button)
{

}

QAbstractButton *QAWizard::button(WizardButton which) const
{
    return 0;

}

void QAWizard::setTitleFormat(Qt::TextFormat format)
{

}

Qt::TextFormat QAWizard::titleFormat() const
{
    return Qt::TextFormat(0);

}

void QAWizard::setSubTitleFormat(Qt::TextFormat format)
{

}

Qt::TextFormat QAWizard::subTitleFormat() const
{
    return Qt::TextFormat(0);

}

void QAWizard::setPixmap(WizardPixmap which, const QPixmap &pixmap)
{

}

QPixmap QAWizard::pixmap(WizardPixmap which) const
{
    return {};
}

void QAWizard::setSideWidget(QWidget *widget)
{

}

QWidget *QAWizard::sideWidget() const
{
    return nullptr;
}

void QAWizard::setDefaultProperty(const char *className, const char *property, const char *changedSignal)
{

}

void QAWizard::setVisible(bool visible)
{

}

QSize QAWizard::sizeHint() const
{
    return {};
}

void QAWizard::back()
{

}

void QAWizard::next()
{

}

void QAWizard::setCurrentId(int id)
{

}

void QAWizard::restart()
{

}

bool QAWizard::event(QEvent *event)
{
    return false;
}

void QAWizard::resizeEvent(QResizeEvent *event)
{

}

void QAWizard::paintEvent(QPaintEvent *event)
{

}

bool QAWizard::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    return false;
}

void QAWizard::done(int result)
{

}

void QAWizard::initializePage(int id)
{

}

void QAWizard::cleanupPage(int id)
{

}

QAWizardPage::QAWizardPage(QWidget *parent)
{

}

QAWizardPage::~QAWizardPage()
{

}

void QAWizardPage::setTitle(const QString &title)
{

}

QString QAWizardPage::title() const
{
    return {};

}

void QAWizardPage::setSubTitle(const QString &subTitle)
{

}

QString QAWizardPage::subTitle() const
{
    return {};

}

void QAWizardPage::setPixmap(QAWizard::WizardPixmap which, const QPixmap &pixmap)
{

}

QPixmap QAWizardPage::pixmap(QAWizard::WizardPixmap which) const
{
    return {};

}

void QAWizardPage::setFinalPage(bool finalPage)
{

}

bool QAWizardPage::isFinalPage() const
{
    return false;

}

void QAWizardPage::setCommitPage(bool commitPage)
{

}

bool QAWizardPage::isCommitPage() const
{
    return false;
}

void QAWizardPage::setButtonText(QAWizard::WizardButton which, const QString &text)
{

}

QString QAWizardPage::buttonText(QAWizard::WizardButton which) const
{
    return {};

}

void QAWizardPage::initializePage()
{

}

void QAWizardPage::cleanupPage()
{

}

bool QAWizardPage::validatePage()
{
    return false;

}

bool QAWizardPage::isComplete() const
{
    return false;

}

int QAWizardPage::nextId() const
{
    return 0;

}

void QAWizardPage::setField(const QString &name, const QVariant &value)
{

}

QVariant QAWizardPage::field(const QString &name) const
{
    return {};

}

void QAWizardPage::registerField(const QString &name, QWidget *widget, const char *property, const char *changedSignal)
{

}

QAWizard *QAWizardPage::wizard() const
{
    return nullptr;

}
