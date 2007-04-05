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
 * Contact : chris@qbittorrent.org
 */

#ifndef PROPLISTDELEGATE_H
#define PROPLISTDELEGATE_H

#include <QItemDelegate>
#include <QStyleOptionProgressBarV2>
#include <QStyleOptionComboBox>
#include <QComboBox>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for properties list columns
#define NAME 0
#define SIZE 1
#define PROGRESS 2
#define SELECTED 3

class PropListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    PropListDelegate(QObject *parent=0) : QItemDelegate(parent){}

    ~PropListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItem opt = option;
      char tmp[MAX_CHAR_TMP];
      // set text color
      QVariant value = index.data(Qt::TextColorRole);
      if (value.isValid() && qvariant_cast<QColor>(value).isValid()){
          opt.palette.setColor(QPalette::Text, qvariant_cast<QColor>(value));
      }
      QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                              ? QPalette::Normal : QPalette::Disabled;
      if (option.state & QStyle::State_Selected){
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
      }else{
        painter->setPen(opt.palette.color(cg, QPalette::Text));
      }
      // draw the background color
      if(index.column() != PROGRESS){
        if (option.showDecorationSelected && (option.state & QStyle::State_Selected)){
          if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)){
              cg = QPalette::Inactive;
          }
          painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
        }else{
          value = index.data(Qt::BackgroundColorRole);
          if (value.isValid() && qvariant_cast<QColor>(value).isValid()){
            painter->fillRect(option.rect, qvariant_cast<QColor>(value));
          }
        }
      }
      switch(index.column()){
        case SIZE:
          painter->drawText(option.rect, Qt::AlignCenter, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case NAME:
          painter->drawText(option.rect, Qt::AlignLeft, index.data().toString());
          break;
        case PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          float progress = index.data().toDouble()*100.;
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", progress);
          newopt.rect = opt.rect;
          newopt.text = QString(tmp)+"%";
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = false;
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt,
          painter);
          //We prefer to display text manually to control color/font/boldness
          if (option.state & QStyle::State_Selected){
            opt.palette.setColor(QPalette::Text, QColor("grey"));
            painter->setPen(opt.palette.color(cg, QPalette::Text));
          }
          painter->drawText(option.rect, Qt::AlignCenter, newopt.text);
          break;
        }
        case SELECTED:{
          QStyleOptionComboBox newopt;
          newopt.rect = opt.rect;
          if(index.data().toBool()){
//             painter->drawText(option.rect, Qt::AlignCenter, tr("True"));
            newopt.currentText = tr("True");
          }else{
//             painter->drawText(option.rect, Qt::AlignCenter, tr("False"));
            newopt.currentText = tr("False");
          }
          newopt.state |= QStyle::State_Enabled;
//           newopt.frame = true;
//           newopt.editable = true;
          QApplication::style()->drawComplexControl(QStyle::CC_ComboBox, &newopt,
          painter);
          opt.palette.setColor(QPalette::Text, QColor("black"));
          painter->setPen(opt.palette.color(cg, QPalette::Text));
          painter->drawText(option.rect, Qt::AlignLeft, " "+newopt.currentText);
          break;
        }
        default:
          painter->drawText(option.rect, Qt::AlignCenter, index.data().toString());
      }
    }

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex & index) const {
      if(index.column() != SELECTED) return 0;
      QComboBox* editor = new QComboBox(parent);
      editor->setFocusPolicy(Qt::StrongFocus);
      editor->addItem(tr("True"));
      editor->addItem(tr("False"));
      return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const {
      bool value = index.model()->data(index, Qt::DisplayRole).toBool();
      QComboBox *combobox = static_cast<QComboBox*>(editor);
      if(value) {
        combobox->setCurrentIndex(0);
      } else {
        combobox->setCurrentIndex(1);
      }
    }

//     bool editorEvent(QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index ){
//       qDebug("Event!!!!");
//       return false;
//     }

    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QVariant value = index.data(Qt::FontRole);
      QFont fnt = value.isValid() ? qvariant_cast<QFont>(value) : option.font;
      QFontMetrics fontMetrics(fnt);
      const QString text = index.data(Qt::DisplayRole).toString();
      QRect textRect = QRect(0, 0, 0, fontMetrics.lineSpacing() * (text.count(QLatin1Char('\n')) + 1));
      return textRect.size();
    }

    public slots:
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
      QComboBox *combobox = static_cast<QComboBox*>(editor);
//       combobox->interpretText();
      int value = combobox->currentIndex();
      qDebug("Setting combobox value in index: %d", value);
      QString color;
      if(value == 0) {
        model->setData(index, true);
        color = "green";
      } else {
        model->setData(index, false);
        color = "red";
      }
      for(int i=0; i<model->columnCount(); ++i){
        model->setData(model->index(index.row(), i), QVariant(QColor(color)), Qt::TextColorRole);
      }
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const {
      editor->setGeometry(option.rect);
    }
};

#endif
