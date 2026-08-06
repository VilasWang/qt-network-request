#include "tasktabledelegate.h"
#include "downloadtaskmodel.h"
#include "downloadtask.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>

namespace
{

struct ThemeTokens
{
    QColor accent;
    QColor accentSubtle;
    QColor success;
    QColor successSubtle;
    QColor warning;
    QColor warningSubtle;
    QColor error;
    QColor errorSubtle;
    QColor inset;
    QColor hover;
    QColor textPrimary;
    QColor textSecondary;
    QColor textTertiary;
    QColor textDisabled;
};

ThemeTokens tokensFor(bool dark)
{
    ThemeTokens t;
    if (dark)
    {
        t.accent        = QColor("#7b8cff");
        t.accentSubtle  = QColor("#1e2048");
        t.success       = QColor("#34d399");
        t.successSubtle = QColor("#0a2e1e");
        t.warning       = QColor("#fbbf24");
        t.warningSubtle = QColor("#2e2008");
        t.error         = QColor("#f87171");
        t.errorSubtle   = QColor("#2e1010");
        t.inset         = QColor("#161621");
        t.hover         = QColor("#242438");
        t.textPrimary   = QColor("#e4e3ed");
        t.textSecondary = QColor("#a8a7b8");
        t.textTertiary  = QColor("#6e6d7e");
        t.textDisabled  = QColor("#4a4a5a");
    }
    else
    {
        t.accent        = QColor("#4361ee");
        t.accentSubtle  = QColor("#eef0ff");
        t.success       = QColor("#2d8a56");
        t.successSubtle = QColor("#eaf7ef");
        t.warning       = QColor("#d97706");
        t.warningSubtle = QColor("#fff8ed");
        t.error         = QColor("#dc2626");
        t.errorSubtle   = QColor("#fef2f2");
        t.inset         = QColor("#f1f0ec");
        t.hover         = QColor("#eeedf5");
        t.textPrimary   = QColor("#1a1a24");
        t.textSecondary = QColor("#5e5d6b");
        t.textTertiary  = QColor("#8f8e99");
        t.textDisabled  = QColor("#bfbec5");
    }
    return t;
}

QFont monoFont(int sizePx)
{
    QFont f(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(sizePx);
    return f;
}

// Resolve the file-type glyph + color pair for the icon square,
// mirroring .file-icon.img/.zip/.pdf/.video/.generic in the prototype.
void fileIconStyle(const QString &fileName, const ThemeTokens &t,
                   QString &glyph, QColor &bg, QColor &fg)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();

    if (ext == "iso" || ext == "img")
    {
        glyph = QStringLiteral("\xF0\x9F\x92\xBF"); // 💿
        bg = t.accentSubtle;
        fg = t.accent;
    }
    else if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" ||
             ext == "gz" || ext == "bz2" || ext == "xz")
    {
        glyph = QStringLiteral("\xF0\x9F\x93\xA6"); // 📦
        bg = t.warningSubtle;
        fg = t.warning;
    }
    else if (ext == "pdf")
    {
        glyph = QStringLiteral("\xF0\x9F\x93\x84"); // 📄
        bg = t.errorSubtle;
        fg = t.error;
    }
    else if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
             ext == "webm" || ext == "wmv")
    {
        glyph = QStringLiteral("\xF0\x9F\x8E\xAC"); // 🎬
        bg = t.successSubtle;
        fg = t.success;
    }
    else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" ||
             ext == "bmp" || ext == "webp" || ext == "svg")
    {
        glyph = QStringLiteral("\xF0\x9F\x96\xBC"); // 🖼
        bg = t.accentSubtle;
        fg = t.accent;
    }
    else
    {
        glyph = QStringLiteral("\xF0\x9F\x93\x81"); // 📁
        bg = t.hover;
        fg = t.textTertiary;
    }
}

QString statusText(QtNetworkRequest::NetworkDownloadTask::State state)
{
    using S = QtNetworkRequest::NetworkDownloadTask::State;
    switch (state)
    {
    case S::Waiting:   return QStringLiteral("等待中");
    case S::Running:   return QStringLiteral("下载中");
    case S::Paused:    return QStringLiteral("已暂停");
    case S::Completed: return QStringLiteral("已完成");
    case S::Error:     return QStringLiteral("错误");
    }
    return QString();
}

QPainterPath roundedRectPath(const QRectF &r, qreal radius)
{
    QPainterPath p;
    p.addRoundedRect(r, radius, radius);
    return p;
}

} // namespace

QtNetworkRequest::TaskTableDelegate::TaskTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize QtNetworkRequest::TaskTableDelegate::sizeHint(const QStyleOptionViewItem &option,
                                                    const QModelIndex &index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    return QSize(base.width(), qMax(base.height(), 52));
}

void QtNetworkRequest::TaskTableDelegate::paint(QPainter *painter,
                                                const QStyleOptionViewItem &option,
                                                const QModelIndex &index) const
{
    // Let the style paint the row background (hover / selected states from QSS),
    // then draw the prototype content on top.
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();
    opt.icon = QIcon();
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QtNetworkRequest::NetworkDownloadTask task =
        index.data(QtNetworkRequest::NetworkDownloadTaskModel::FullTaskRole)
            .value<QtNetworkRequest::NetworkDownloadTask>();

    const bool dark = opt.palette.color(QPalette::Window).lightness() < 128;
    const ThemeTokens t = tokensFor(dark);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cell = opt.rect.adjusted(16, 0, -12, 0);
    const int col = index.column();
    using C = QtNetworkRequest::NetworkDownloadTaskModel::Column;

    if (col == static_cast<int>(C::ColumnFileName))
    {
        // Icon square
        QRect iconRect(cell.left(), cell.center().y() - 16, 32, 32);
        QString glyph;
        QColor iconBg, iconFg;
        fileIconStyle(task.fileName, t, glyph, iconBg, iconFg);
        painter->fillPath(roundedRectPath(iconRect, 5), iconBg);
        QFont emojiFont(QStringLiteral("Segoe UI Emoji"));
        emojiFont.setPixelSize(14);
        painter->setFont(emojiFont);
        painter->setPen(iconFg);
        painter->drawText(iconRect, Qt::AlignCenter, glyph);

        // File name + URL
        const int textLeft = iconRect.right() + 10;
        QRect textRect(textLeft, cell.top(), cell.right() - textLeft, cell.height());

        QFont nameFont = opt.font;
        nameFont.setPixelSize(13);
        nameFont.setWeight(QFont::DemiBold);
        QFontMetrics nameFm(nameFont);
        QFont urlFont = monoFont(11);
        QFontMetrics urlFm(urlFont);

        const QString name = nameFm.elidedText(task.fileName, Qt::ElideRight, textRect.width());
        QString urlStr = task.url.toString();
        if (urlStr.startsWith(QStringLiteral("https://")))
            urlStr.remove(0, 8);
        else if (urlStr.startsWith(QStringLiteral("http://")))
            urlStr.remove(0, 7);
        urlStr = urlFm.elidedText(urlStr, Qt::ElideRight, textRect.width());

        painter->setFont(nameFont);
        painter->setPen(t.textPrimary);
        painter->drawText(textRect.adjusted(0, 8, 0, 0), Qt::AlignLeft | Qt::AlignTop, name);

        painter->setFont(urlFont);
        painter->setPen(t.textTertiary);
        painter->drawText(textRect.adjusted(0, 0, 0, -8), Qt::AlignLeft | Qt::AlignBottom, urlStr);
    }
    else if (col == static_cast<int>(C::ColumnProgress))
    {
        // Thin rounded progress bar + percentage text
        const int textW = 40;
        QRect barRect(cell.left(), cell.center().y() - 3, cell.width() - textW - 10, 6);
        painter->fillPath(roundedRectPath(barRect, 3), t.inset);

        int pct = task.progress;
        QString pctText;
        QColor fill;
        using S = QtNetworkRequest::NetworkDownloadTask::State;
        switch (task.state)
        {
        case S::Completed: fill = t.success; break;
        case S::Error:     fill = t.error;   break;
        case S::Paused:    fill = t.warning; break;
        default:           fill = t.accent;  break;
        }

        if (task.state == S::Waiting && task.totalBytes <= 0)
        {
            pct = 0;
            pctText = QStringLiteral("--");
        }
        else
        {
            pctText = QStringLiteral("%1%").arg(pct);
        }

        if (pct > 0)
        {
            QRectF fillRect(barRect.left(), barRect.top(),
                            barRect.width() * qMin(pct, 100) / 100.0, barRect.height());
            if (fillRect.width() >= 1.0)
                painter->fillPath(roundedRectPath(fillRect, 3), fill);
        }

        QFont pctFont = opt.font;
        pctFont.setPixelSize(11);
        pctFont.setWeight(QFont::Bold);
        painter->setFont(pctFont);
        painter->setPen(t.textSecondary);
        painter->drawText(QRect(barRect.right() + 10, cell.top(), textW, cell.height()),
                          Qt::AlignRight | Qt::AlignVCenter, pctText);
    }
    else if (col == static_cast<int>(C::ColumnState))
    {
        // Pill badge: subtle background + status dot + label
        using S = QtNetworkRequest::NetworkDownloadTask::State;
        QColor bg, fg;
        switch (task.state)
        {
        case S::Running:   bg = t.accentSubtle;  fg = t.accent;  break;
        case S::Completed: bg = t.successSubtle; fg = t.success; break;
        case S::Paused:    bg = t.warningSubtle; fg = t.warning; break;
        case S::Error:     bg = t.errorSubtle;   fg = t.error;   break;
        default:           bg = t.hover;         fg = t.textTertiary; break;
        }

        const QString label = statusText(task.state);
        QFont badgeFont = opt.font;
        badgeFont.setPixelSize(11);
        badgeFont.setWeight(QFont::DemiBold);
        QFontMetrics badgeFm(badgeFont);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        const int labelW = badgeFm.horizontalAdvance(label);
#else
        const int labelW = badgeFm.width(label);
#endif
        const int badgeW = 7 + 6 + labelW + 10 + 10; // dot + gaps + text + padding
        QRect badgeRect(cell.left(), cell.center().y() - 11, badgeW, 22);

        painter->fillPath(roundedRectPath(badgeRect, 11), bg);

        QColor dotColor = fg;
        if (task.state == S::Waiting)
            dotColor = t.textDisabled;
        painter->setPen(Qt::NoPen);
        painter->setBrush(dotColor);
        painter->drawEllipse(QPointF(badgeRect.left() + 10 + 3.5, badgeRect.center().y()), 3.5, 3.5);

        painter->setFont(badgeFont);
        painter->setPen(fg);
        painter->drawText(QRect(badgeRect.left() + 23, badgeRect.top(),
                                badgeRect.width() - 33, badgeRect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    else
    {
        // Numeric monospace cells: size / downloaded / speed / time
        QString text;
        QColor color = t.textPrimary;
        QFont f = monoFont(11);
        using S = QtNetworkRequest::NetworkDownloadTask::State;

        if (col == static_cast<int>(C::ColumnFileSize))
        {
            text = task.formatFileSize(task.totalBytes);
        }
        else if (col == static_cast<int>(C::ColumnDownloaded))
        {
            if (task.state == S::Waiting && task.downloadedBytes <= 0)
                text = QStringLiteral("--");
            else
                text = task.formatFileSize(task.downloadedBytes);
        }
        else if (col == static_cast<int>(C::ColumnSpeed))
        {
            if (task.speed > 0)
            {
                text = task.formatSpeed();
                color = task.speed >= 5 * 1024 * 1024 ? t.success : t.accent;
            }
            else
            {
                text = QStringLiteral("--");
                color = t.textTertiary;
            }
        }
        else if (col == static_cast<int>(C::ColumnTime))
        {
            text = task.formatTime();
            color = text == QStringLiteral("--") ? t.textTertiary : t.textSecondary;
        }

        if (text == QStringLiteral("--"))
            color = t.textTertiary;

        painter->setFont(f);
        painter->setPen(color);
        painter->drawText(cell, Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    painter->restore();
}
