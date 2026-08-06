#ifndef TASKTABLEDELEGATE_H
#define TASKTABLEDELEGATE_H

// ================================================================
// TaskTableDelegate — prototype-faithful rendering for the task table.
//
// Mirrors qt-downloader-prototype.html:
//   * File column   : colored icon square + file name + muted URL
//   * Progress col  : thin rounded bar + percentage text
//   * Status column : pill badge with pulsing-style status dot
//   * Numeric cells : monospace, muted when data is not available
// ================================================================

#include <QStyledItemDelegate>

namespace QtNetworkRequest
{

class TaskTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TaskTableDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

} // namespace QtNetworkRequest

#endif // TASKTABLEDELEGATE_H
