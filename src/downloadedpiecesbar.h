#ifndef DOWNLOADEDPIECESBAR_H
#define DOWNLOADEDPIECESBAR_H

#include <QWidget>
#include <QPainter>
#include <QList>
#include <QPixmap>
#include <libtorrent/bitfield.hpp>

using namespace libtorrent;
#define BAR_HEIGHT 18

class DownloadedPiecesBar: public QWidget {
  Q_OBJECT

private:
  QPixmap pixmap;


public:
  DownloadedPiecesBar(QWidget *parent): QWidget(parent) {
    setFixedHeight(BAR_HEIGHT);
  }

  void setProgress(bitfield pieces) {
    if(pieces.empty()) {
      // Empty bar
      pixmap = QPixmap(1, 1);
      QPainter painter(&pixmap);
      painter.setPen(Qt::white);
      painter.drawPoint(0,0);
    } else {
      pixmap = QPixmap(pieces.size(), 1);
      QPainter painter(&pixmap);
      for(uint i=0; i<pieces.size(); ++i) {
        if(pieces[i])
          painter.setPen(Qt::blue);
        else
          painter.setPen(Qt::white);
        painter.drawPoint(i,0);
      }
    }
    update();
  }

  void clear() {
    pixmap = QPixmap();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) {
    if(pixmap.isNull()) return;
    QPainter painter(this);
    painter.drawPixmap(rect(), pixmap);
  }

};

#endif // DOWNLOADEDPIECESBAR_H
