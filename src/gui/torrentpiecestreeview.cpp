#include "torrentpiecestreeview.h"

#include "base/bittorrent/session.h"

TorrentPiecesTreeView::TorrentPiecesTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setExpandsOnDoubleClick(false);
}
