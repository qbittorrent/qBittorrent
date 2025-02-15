#pragma once

#include <QObject>
#include <QPersistentModelIndex>

class TransferListWidget;

// Workaround for QTBUG-115838: Shift-click range selection not working properly on macOS
class MacOSShiftClickHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MacOSShiftClickHandler)

public:
    explicit MacOSShiftClickHandler(TransferListWidget *parent);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    TransferListWidget *m_treeView;
    QPersistentModelIndex m_lastClickedIndex;
}; 