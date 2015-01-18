/*
 * ico.h - kimgio import filter for MS Windows .ico files
 *
 * Distributed under the terms of the LGPL
 * Copyright (c) 2000 Malte Starostik <malte@kde.org>
 *
 */

// You can use QImageIO::setParameters() to request a specific
// Icon out of an .ico file:
//
// Options consist of a name=value pair and are separated by a semicolon.
// Available options are:
//     size=<size>   select the icon that most closely matches <size> (pixels)
//                   default: 32
//     colors=<num>  select the icon that has <num> colors (or comes closest)
//                   default: 1 << display depth or 0 (RGB) if display depth > 8
//     index=<index> select the indexth icon from the file. If this option
//                   is present, the size and colors options will be ignored.
//                   default: none
// If both size and colors are given, size takes precedence.
//
// The old format is still supported:
//     the parameters consist of a single string in the form
//     "<size>[:<colors>]" which correspond to the options above
//
// If an icon was returned (i.e. the file is valid and the index option
// if present was not out of range), the icon's index within the .ico
// file is returned in the text tag "X-Index" of the image.
// If the icon is in fact a cursor, its hotspot coordinates are returned
// in the text tags "X-HotspotX" and "X-HotspotY".

#ifndef	_ICO_H_
#define _ICO_H_

#include <QtGui/QImageIOPlugin>

class ICOHandler : public QImageIOHandler
{
public:
    ICOHandler();

    bool canRead() const;
    bool read(QImage *image);
    bool write(const QImage &image);

    QByteArray name() const;

    static bool canRead(QIODevice *device);
};

#endif
