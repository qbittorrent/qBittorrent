/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef PROPLISTDELEGATE_H
#define PROPLISTDELEGATE_H

#include <QItemDelegate>
#include <QStyleOptionProgressBarV2>
#include <QStyleOptionViewItemV2>
#include <QStyleOptionComboBox>
#include <QComboBox>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"
#include "propertieswidget.h"

#ifdef Q_OS_WIN
#include <QPlastiqueStyle>
#endif

// Defines for properties list columns
enum PropColumn {NAME, PCSIZE, PROGRESS, PRIORITY};

class PropListDelegate: public QItemDelegate {
  Q_OBJECT

private:
  PropertiesWidget *properties;

signals:
  void filteredFilesChanged() const;

public:
  PropListDelegate(PropertiesWidget* properties=0, QObject *parent=0) : QItemDelegate(parent), properties(properties) {
  }

  ~PropListDelegate() {}

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
    painter->save();
    QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
    switch(index.column()) {
    case PCSIZE:
      QItemDelegate::drawBackground(painter, opt, index);
      QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
      break;
    case PROGRESS:{
      if (index.data().toDouble() >= 0) {
          QStyleOptionProgressBarV2 newopt;
          qreal progress = index.data().toDouble()*100.;
          newopt.rect = opt.rect;
          // We don't want to display 100% unless
          // the torrent is really complete
          if (progress > 99.94 && progress < 100.)
            progress = 99.9;
          newopt.text = QString(QByteArray::number(progress, 'f', 1))+QString::fromUtf8("%");
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = true;
#ifndef Q_OS_WIN
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
#else
          // XXX: To avoid having the progress text on the right of the bar
          QPlastiqueStyle st;
          st.drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
#endif
      } else {
          // Do not display anything if the file is disabled (progress  == -1)
          QItemDelegate::drawBackground(painter, opt, index);
      }
      break;
    }
    case PRIORITY: {
      QItemDelegate::drawBackground(painter, opt, index);
      QString text = "";
      switch(index.data().toInt()) {
      case -1:
        text = tr("Mixed", "Mixed (priorities");
        break;
      case 0:
        text = tr("Not downloaded");
        break;
      case 2:
        text = tr("High", "High (priority)");
        break;
      case 7:
        text = tr("Maximum", "Maximum (priority)");
        break;
      default:
        text = tr("Normal", "Normal (priority)");
        break;
      }
      QItemDelegate::drawDisplay(painter, opt, option.rect, text);
      break;
    }
    default:
      QItemDelegate::paint(painter, option, index);
      break;
    }
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const {
    QVariant value = index.data(Qt::FontRole);
    QFont fnt = value.isValid() ? qvariant_cast<QFont>(value) : option.font;
    QFontMetrics fontMetrics(fnt);
    const QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect = QRect(0, 0, 0, fontMetrics.lineSpacing() * (text.count(QLatin1Char('\n')) + 1));
    textRect.setHeight(textRect.height()+4);
    return textRect.size();
  }

  void setEditorData(QWidget *editor, const QModelIndex &index) const {
    QComboBox *combobox = static_cast<QComboBox*>(editor);
    // Set combobox index
    switch(index.data().toInt()) {
    case 2:
      combobox->setCurrentIndex(1);
      break;
    case 7:
      combobox->setCurrentIndex(2);
      break;
    default:
      combobox->setCurrentIndex(0);
      break;
    }
  }

  QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &index) const {
    if (index.column() != PRIORITY) return 0;
    if (properties) {
      QTorrentHandle h = properties->getCurrentTorrent();
#if LIBTORRENT_VERSION_NUM >= 001600
      if (!h.is_valid() || !h.has_metadata() || h.status(0x0).is_seeding) return 0;
#else
      if (!h.is_valid() || !h.has_metadata() || static_cast<libtorrent::torrent_handle>(h).is_seed()) return 0;
#endif
    }
    if (index.data().toInt() <= 0) {
      // IGNORED or MIXED
      return 0;
    }
    QComboBox* editor = new QComboBox(parent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(tr("Normal", "Normal (priority)"));
    editor->addItem(tr("High", "High (priority)"));
    editor->addItem(tr("Maximum", "Maximum (priority)"));
    return editor;
  }

public slots:
  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    QComboBox *combobox = static_cast<QComboBox*>(editor);
    int value = combobox->currentIndex();
    qDebug("PropListDelegate: setModelData(%d)", value);
    switch(value)  {
    case 1:
      model->setData(index, 2); // HIGH
      break;
    case 2:
      model->setData(index, 7); // MAX
      break;
    default:
      model->setData(index, 1); // NORMAL
    }
    emit filteredFilesChanged();
  }

  void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const {
    qDebug("UpdateEditor Geometry called");
    editor->setGeometry(option.rect);
  }

};

#endif
