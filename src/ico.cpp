/*
 * kimgio import filter for MS Windows .ico files
 *
 * Distributed under the terms of the LGPL
 * Copyright (c) 2000 Malte Starostik <malte@kde.org>
 *
 */

#include "ico.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include <QImage>
#include <QBitmap>
#include <QApplication>
#include <QVector>
#include <QDesktopWidget>

namespace
{
    // Global header (see http://www.daubnet.com/formats/ICO.html)
    struct IcoHeader
    {
        enum Type { Icon = 1, Cursor };
        quint16 reserved;
        quint16 type;
        quint16 count;
    };

    inline QDataStream& operator >>( QDataStream& s, IcoHeader& h )
    {
        return s >> h.reserved >> h.type >> h.count;
    }

    // Based on qt_read_dib et al. from qimage.cpp
    // (c) 1992-2002 Trolltech AS.
    struct BMP_INFOHDR
    {
        static const quint32 Size = 40;
        quint32  biSize;                // size of this struct
        quint32  biWidth;               // pixmap width
        quint32  biHeight;              // pixmap height
        quint16  biPlanes;              // should be 1
        quint16  biBitCount;            // number of bits per pixel
        enum Compression { RGB = 0 };
        quint32  biCompression;         // compression method
        quint32  biSizeImage;           // size of image
        quint32  biXPelsPerMeter;       // horizontal resolution
        quint32  biYPelsPerMeter;       // vertical resolution
        quint32  biClrUsed;             // number of colors used
        quint32  biClrImportant;        // number of important colors
    };
    const quint32 BMP_INFOHDR::Size;

    QDataStream& operator >>( QDataStream &s, BMP_INFOHDR &bi )
    {
        s >> bi.biSize;
        if ( bi.biSize == BMP_INFOHDR::Size )
        {
            s >> bi.biWidth >> bi.biHeight >> bi.biPlanes >> bi.biBitCount;
            s >> bi.biCompression >> bi.biSizeImage;
            s >> bi.biXPelsPerMeter >> bi.biYPelsPerMeter;
            s >> bi.biClrUsed >> bi.biClrImportant;
        }
        return s;
    }

#if 0
    QDataStream &operator<<( QDataStream &s, const BMP_INFOHDR &bi )
    {
        s << bi.biSize;
        s << bi.biWidth << bi.biHeight;
        s << bi.biPlanes;
        s << bi.biBitCount;
        s << bi.biCompression;
        s << bi.biSizeImage;
        s << bi.biXPelsPerMeter << bi.biYPelsPerMeter;
        s << bi.biClrUsed << bi.biClrImportant;
        return s;
    }
#endif

    // Header for every icon in the file
    struct IconRec
    {
        unsigned char width;
        unsigned char height;
        quint16 colors;
        quint16 hotspotX;
        quint16 hotspotY;
        quint32 size;
        quint32 offset;
    };

    inline QDataStream& operator >>( QDataStream& s, IconRec& r )
    {
        return s >> r.width >> r.height >> r.colors
                 >> r.hotspotX >> r.hotspotY >> r.size >> r.offset;
    }

    struct LessDifference
    {
        LessDifference( unsigned s, unsigned c )
            : size( s ), colors( c ) {}

        bool operator ()( const IconRec& lhs, const IconRec& rhs ) const
        {
            // closest size match precedes everything else
            if ( std::abs( int( lhs.width - size ) ) <
                 std::abs( int( rhs.width - size ) ) ) return true;
            else if ( std::abs( int( lhs.width - size ) ) >
                 std::abs( int( rhs.width - size ) ) ) return false;
            else if ( colors == 0 )
            {
                // high/true color requested
                if ( lhs.colors == 0 ) return true;
                else if ( rhs.colors == 0 ) return false;
                else return lhs.colors > rhs.colors;
            }
            else
            {
                // indexed icon requested
                if ( lhs.colors == 0 && rhs.colors == 0 ) return false;
                else if ( lhs.colors == 0 ) return false;
                else return std::abs( int( lhs.colors - colors ) ) <
                            std::abs( int( rhs.colors - colors ) );
            }
        }
        unsigned size;
        unsigned colors;
    };

    bool loadFromDIB( QDataStream& stream, const IconRec& rec, QImage& icon )
    {
        BMP_INFOHDR header;
        stream >> header;
        if ( stream.atEnd() || header.biSize != BMP_INFOHDR::Size ||
             header.biSize > rec.size ||
             header.biCompression != BMP_INFOHDR::RGB ||
             ( header.biBitCount != 1 && header.biBitCount != 4 &&
               header.biBitCount != 8 && header.biBitCount != 24 &&
               header.biBitCount != 32 ) ) return false;

        unsigned paletteSize, paletteEntries;

        if (header.biBitCount > 8)
        {
            paletteEntries = 0;
            paletteSize    = 0;
        }
        else
        {
            paletteSize    = (1 << header.biBitCount);
            paletteEntries = paletteSize;
            if (header.biClrUsed && header.biClrUsed < paletteSize)
                paletteEntries = header.biClrUsed;
        }

        // Always create a 32-bit image to get the mask right
        // Note: this is safe as rec.width, rec.height are bytes
        icon = QImage( rec.width, rec.height, QImage::Format_ARGB32 );
        if ( icon.isNull() ) return false;

        QVector< QRgb > colorTable( paletteSize );

        colorTable.fill( QRgb( 0 ) );
        for ( unsigned i = 0; i < paletteEntries; ++i )
        {
            unsigned char rgb[ 4 ];
            stream.readRawData( reinterpret_cast< char* >( &rgb ),
                                 sizeof( rgb ) );
            colorTable[ i ] = qRgb( rgb[ 2 ], rgb[ 1 ], rgb[ 0 ] );
        }

        unsigned bpl = ( rec.width * header.biBitCount + 31 ) / 32 * 4;

        unsigned char* buf = new unsigned char[ bpl ];
        for ( unsigned y = rec.height; !stream.atEnd() && y--; )
        {
            stream.readRawData( reinterpret_cast< char* >( buf ), bpl );
            unsigned char* pixel = buf;
            QRgb* p = reinterpret_cast< QRgb* >( icon.scanLine(y));
            switch ( header.biBitCount )
            {
                case 1:
                    for ( unsigned x = 0; x < rec.width; ++x )
                        *p++ = colorTable[
                            ( pixel[ x / 8 ] >> ( 7 - ( x & 0x07 ) ) ) & 1 ];
                    break;
                case 4:
                    for ( unsigned x = 0; x < rec.width; ++x )
                        if ( x & 1 ) *p++ = colorTable[ pixel[ x / 2 ] & 0x0f ];
                        else *p++ = colorTable[ pixel[ x / 2 ] >> 4 ];
                    break;
                case 8:
                    for ( unsigned x = 0; x < rec.width; ++x )
                        *p++ = colorTable[ pixel[ x ] ];
                    break;
                case 24:
                    for ( unsigned x = 0; x < rec.width; ++x )
                        *p++ = qRgb( pixel[ 3 * x + 2 ],
                                     pixel[ 3 * x + 1 ],
                                     pixel[ 3 * x ] );
                    break;
                case 32:
                    for ( unsigned x = 0; x < rec.width; ++x )
                        *p++ = qRgba( pixel[ 4 * x + 2 ],
                                      pixel[ 4 * x + 1 ],
                                      pixel[ 4 * x ],
                                      pixel[ 4 * x  + 3] );
                    break;
            }
        }
        delete[] buf;

        if ( header.biBitCount < 32 )
        {
            // Traditional 1-bit mask
            bpl = ( rec.width + 31 ) / 32 * 4;
            buf = new unsigned char[ bpl ];
            for ( unsigned y = rec.height; y--; )
            {
                stream.readRawData( reinterpret_cast< char* >( buf ), bpl );
                QRgb* p = reinterpret_cast< QRgb* >(icon.scanLine(y));
                for ( unsigned x = 0; x < rec.width; ++x, ++p )
                    if ( ( ( buf[ x / 8 ] >> ( 7 - ( x & 0x07 ) ) ) & 1 ) )
                        *p &= RGB_MASK;
            }
            delete[] buf;
        }
        return true;
    }
}

ICOHandler::ICOHandler()
{
}

bool ICOHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("ico");
        return true;
    }
    return false;
}

bool ICOHandler::read(QImage *outImage)
{

    qint64 offset = device()->pos();

    QDataStream stream( device() );
    stream.setByteOrder( QDataStream::LittleEndian );
    IcoHeader header;
    stream >> header;
    if ( stream.atEnd() || !header.count ||
         ( header.type != IcoHeader::Icon && header.type != IcoHeader::Cursor) )
        return false;

    unsigned requestedSize = 32;
    unsigned requestedColors =  QApplication::desktop()->depth() > 8 ? 0 : QApplication::desktop()->depth();
    int requestedIndex = -1;
#if 0
    if ( io->parameters() )
    {
        QStringList params = QString(io->parameters()).split( ';', QString::SkipEmptyParts );
        QMap< QString, QString > options;
        for ( QStringList::ConstIterator it = params.begin();
              it != params.end(); ++it )
        {
            QStringList tmp = (*it).split( '=', QString::SkipEmptyParts );
            if ( tmp.count() == 2 ) options[ tmp[ 0 ] ] = tmp[ 1 ];
        }
        if ( options[ "index" ].toUInt() )
            requestedIndex = options[ "index" ].toUInt();
        if ( options[ "size" ].toUInt() )
            requestedSize = options[ "size" ].toUInt();
        if ( options[ "colors" ].toUInt() )
            requestedColors = options[ "colors" ].toUInt();
    }
#endif

    typedef std::vector< IconRec > IconList;
    IconList icons;
    for ( unsigned i = 0; i < header.count; ++i )
    {
        if ( stream.atEnd() )
            return false;
        IconRec rec;
        stream >> rec;
        icons.push_back( rec );
    }
    IconList::const_iterator selected;
    if (requestedIndex >= 0) {
        selected = std::min( icons.begin() + requestedIndex, icons.end() );
    } else {
        selected = std::min_element( icons.begin(), icons.end(),
                                     LessDifference( requestedSize, requestedColors ) );
    }
    if ( stream.atEnd() || selected == icons.end() ||
         offset + selected->offset > device()->size() )
        return false;

    device()->seek( offset + selected->offset );
    QImage icon;
    if ( loadFromDIB( stream, *selected, icon ) )
    {
        icon.setText( "X-Index", 0, QString::number( selected - icons.begin() ) );
        if ( header.type == IcoHeader::Cursor )
        {
            icon.setText( "X-HotspotX", 0, QString::number( selected->hotspotX ) );
            icon.setText( "X-HotspotY", 0, QString::number( selected->hotspotY ) );
        }

        *outImage = icon;
        return true;
    }
    return false;
}

bool ICOHandler::write(const QImage &/*image*/)
{
#if 0
    if (image.isNull())
        return;

    QByteArray dibData;
    QDataStream dib(dibData, QIODevice::ReadWrite);
    dib.setByteOrder(QDataStream::LittleEndian);

    QImage pixels = image;
    QImage mask;
    if (io->image().hasAlphaBuffer())
        mask = image.createAlphaMask();
    else
        mask = image.createHeuristicMask();
    mask.invertPixels();
    for ( int y = 0; y < pixels.height(); ++y )
        for ( int x = 0; x < pixels.width(); ++x )
            if ( mask.pixel( x, y ) == 0 ) pixels.setPixel( x, y, 0 );

    if (!qt_write_dib(dib, pixels))
        return;

   uint hdrPos = dib.device()->at();
    if (!qt_write_dib(dib, mask))
        return;
    memmove(dibData.data() + hdrPos, dibData.data() + hdrPos + BMP_WIN + 8, dibData.size() - hdrPos - BMP_WIN - 8);
    dibData.resize(dibData.size() - BMP_WIN - 8);

    QDataStream ico(device());
    ico.setByteOrder(QDataStream::LittleEndian);
    IcoHeader hdr;
    hdr.reserved = 0;
    hdr.type = Icon;
    hdr.count = 1;
    ico << hdr.reserved << hdr.type << hdr.count;
    IconRec rec;
    rec.width = image.width();
    rec.height = image.height();
    if (image.numColors() <= 16)
        rec.colors = 16;
    else if (image.depth() <= 8)
        rec.colors = 256;
    else
        rec.colors = 0;
    rec.hotspotX = 0;
    rec.hotspotY = 0;
    rec.dibSize = dibData.size();
    ico << rec.width << rec.height << rec.colors
        << rec.hotspotX << rec.hotspotY << rec.dibSize;
    rec.dibOffset = ico.device()->at() + sizeof(rec.dibOffset);
    ico << rec.dibOffset;

    BMP_INFOHDR dibHeader;
    dib.device()->at(0);
    dib >> dibHeader;
    dibHeader.biHeight = image.height() << 1;
    dib.device()->at(0);
    dib << dibHeader;

    ico.writeRawBytes(dibData.data(), dibData.size());
    return true;
#endif
    return false;
}

QByteArray ICOHandler::name() const
{
    return "ico";
}

bool ICOHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("ICOHandler::canRead() called with no device");
        return false;
    }

    const qint64 oldPos = device->pos();

    char head[8];
    qint64 readBytes = device->read(head, sizeof(head));
    const bool readOk = readBytes == sizeof(head);

    if (device->isSequential()) {
        while (readBytes > 0)
            device->ungetChar(head[readBytes-- - 1]);
    } else {
        device->seek(oldPos);
    }

    if ( !readOk )
        return false;

    return head[2] == '\001' && head[3] == '\000' && // type should be 1
        ( head[6] == 16 || head[6] == 32 || head[6] == 64 ) && // width can only be one of those
        ( head[7] == 16 || head[7] == 32 || head[7] == 64 );   // same for height
}

class ICOPlugin : public QImageIOPlugin
{
public:
    QStringList keys() const;
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const;
};

QStringList ICOPlugin::keys() const
{
    return QStringList() << "ico" << "ICO";
}

QImageIOPlugin::Capabilities ICOPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "ico" || format == "ICO")
        return Capabilities(CanRead);
    if (!format.isEmpty())
        return 0;
    if (!device->isOpen())
        return 0;

    Capabilities cap;
    if (device->isReadable() && ICOHandler::canRead(device))
        cap |= CanRead;
    return cap;
}

QImageIOHandler *ICOPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new ICOHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

Q_EXPORT_STATIC_PLUGIN(ICOPlugin)
Q_EXPORT_PLUGIN2(ico, ICOPlugin)
