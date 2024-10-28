/****************************************************************************
 **
 ** Copyright (C) 2024 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** All Rights Reserved.
 **
 ** NOTICE: All information contained herein is, and remains
 ** the property of The Qt Company Ltd, if any.
 ** The intellectual and technical concepts contained
 ** herein are proprietary to The Qt Company Ltd
 ** and its suppliers and may be covered by Finnish and Foreign Patents,
 ** patents in process, and are protected by trade secret or copyright law.
 ** Dissemination of this information or reproduction of this material
 ** is strictly forbidden unless prior written permission is obtained
 ** from The Qt Company Ltd.
 ****************************************************************************/

#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>

namespace QInstaller {

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(const QString &message, const QString &objectName, QWidget* parent = Q_NULLPTR);
    ~ClickableLabel() {};

signals:
    void clicked();

protected:
    bool event(QEvent *e) override;
    void mousePressEvent(QMouseEvent* event) override;
};

}

#endif // CLICKABLELABEL_H
