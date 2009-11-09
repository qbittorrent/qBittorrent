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

// Defines for properties list columns
enum PropColumn {NAME, SIZE, PROGRESS, PRIORITY, INDEX};
enum PropPriority {IGNORED=0, NORMAL=1, HIGH=2, MAXIMUM=7};

class PropListDelegate: public QItemDelegate {
  Q_OBJECT

  signals:
    void filteredFilesChanged();

  public:
    PropListDelegate(QObject *parent=0) : QItemDelegate(parent){
    }

    ~PropListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
      QPalette::ColorGroup cg = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
      switch(index.column()){
        case SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          float progress = index.data().toDouble()*100.;
          newopt.rect = opt.rect;
          newopt.text = QString(QByteArray::number(progress, 'f', 1))+QString::fromUtf8("%");
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = true;
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
          break;
        }
        case PRIORITY:{
          QStyleOptionComboBox newopt;
          newopt.rect = opt.rect;
          switch(index.data().toInt()){
            case IGNORED:
              newopt.currentText = tr("Ignored");
              break;
            case NORMAL:
              newopt.currentText = tr("Normal", "Normal (priority)");
              break;
            case HIGH:
              newopt.currentText = tr("High", "High (priority)");
              break;
            case MAXIMUM:
              newopt.currentText = tr("Maximum", "Maximum (priority)");
              break;
            default:
              qDebug("Unhandled priority, setting NORMAL");
              newopt.currentText = tr("Normal", "Normal (priority)");
          }
          newopt.state |= QStyle::State_Enabled;
          QApplication::style()->drawComplexControl(QStyle::CC_ComboBox, &newopt,
          painter);
          opt.palette.setColor(QPalette::Text, QColor("black"));
          painter->setPen(opt.palette.color(cg, QPalette::Text));
          painter->drawText(option.rect, Qt::AlignLeft, QString::fromUtf8(" ")+newopt.currentText);
          break;
        }
        default:
          QItemDelegate::paint(painter, option, index);
      }
    }

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex & index) const {
      if(index.column() != PRIORITY) return 0;
      QComboBox* editor = new QComboBox(parent);
      editor->setFocusPolicy(Qt::StrongFocus);
      editor->addItem(tr("Ignored"));
      editor->addItem(tr("Normal", "Normal (priority)"));
      editor->addItem(tr("High", "High (priority)"));
      editor->addItem(tr("Maximum", "Maximum (priority)"));
      return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const {
      unsigned short val = index.model()->data(index, Qt::DisplayRole).toInt();
      QComboBox *combobox = static_cast<QComboBox*>(editor);
      qDebug("Set Editor data: Prio is %d", val);
      switch(val){
        case IGNORED:
          combobox->setCurrentIndex(0);
          break;
        case NORMAL:
          combobox->setCurrentIndex(1);
          break;
        case HIGH:
          combobox->setCurrentIndex(2);
          break;
        case MAXIMUM:
          combobox->setCurrentIndex(3);
          break;
        default:
          qDebug("Unhandled priority, setting to NORMAL");
          combobox->setCurrentIndex(1);
      }
    }

    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QVariant value = index.data(Qt::FontRole);
      QFont fnt = value.isValid() ? qvariant_cast<QFont>(value) : option.font;
      QFontMetrics fontMetrics(fnt);
      const QString text = index.data(Qt::DisplayRole).toString();
      QRect textRect = QRect(0, 0, 0, fontMetrics.lineSpacing() * (text.count(QLatin1Char('\n')) + 1));
      return textRect.size();
    }

    public slots:
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) {
      QComboBox *combobox = static_cast<QComboBox*>(editor);
      int value = combobox->currentIndex();
      qDebug("Setting combobox value in index: %d", value);
      unsigned short old_val = index.model()->data(index, Qt::DisplayRole).toInt();
      switch(value){
        case 0:
          if(old_val != IGNORED){
            model->setData(index, QVariant(IGNORED));
            emit filteredFilesChanged();
          } else {
            // XXX: hack to force the model to send the itemChanged() signal
            model->setData(index, QVariant(NORMAL));
            model->setData(index, QVariant(IGNORED));
          }
          break;
        case 1:
//           if(old_val != NORMAL){
//             model->setData(index, QVariant(NORMAL));
//             if(filteredFilesChanged != 0)
//               *filteredFilesChanged = true;
//           } else {
            model->setData(index, QVariant(HIGH));
            model->setData(index, QVariant(NORMAL));
            emit filteredFilesChanged();
//           }
          break;
        case 2:
          if(old_val != HIGH){
            model->setData(index, QVariant(HIGH));
            emit filteredFilesChanged();
          } else {
            model->setData(index, QVariant(NORMAL));
            model->setData(index, QVariant(HIGH));
          }
          break;
        case 3:
          if(old_val != MAXIMUM){
            model->setData(index, QVariant(MAXIMUM));
            emit filteredFilesChanged();
          } else {
            model->setData(index, QVariant(HIGH));
            model->setData(index, QVariant(MAXIMUM));
          }
          break;
        default:
          if(old_val != NORMAL){
            model->setData(index, QVariant(NORMAL));
            emit filteredFilesChanged();
          } else {
            model->setData(index, QVariant(HIGH));
            model->setData(index, QVariant(NORMAL));
          }
      }
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const {
      editor->setGeometry(option.rect);
    }

};

#endif
