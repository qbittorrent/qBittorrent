#pragma once

#include <QTreeView>

class TorrentPiecesTreeView final : public QTreeView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentPiecesTreeView)

public:
    explicit TorrentPiecesTreeView(QWidget *parent = nullptr);
};
