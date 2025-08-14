#include <QDir>
#include <QObject>
#include <QString>
#include <QTest>

#include "base/path.h"
#include "base/utils/fs.h"

class TestTildeExpansion : public QObject
{
    Q_OBJECT

private slots:
    void testExpandTilde_data();
    void testExpandTilde();
};

void TestTildeExpansion::testExpandTilde_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    
    const QString homeDir = QDir::homePath();
    
    QTest::newRow("tilde only") << u"~"_s << homeDir;
    QTest::newRow("tilde with path") << u"~/Downloads"_s << (homeDir + u"/Downloads"_s);
    QTest::newRow("tilde with nested path") << u"~/Documents/qBittorrent"_s << (homeDir + u"/Documents/qBittorrent"_s);
    QTest::newRow("absolute path") << u"/absolute/path"_s << u"/absolute/path"_s;
    QTest::newRow("relative path") << u"relative/path"_s << u"relative/path"_s;
    QTest::newRow("empty path") << u""_s << u""_s;
}

void TestTildeExpansion::testExpandTilde()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    
    const Path inputPath(input);
    const Path expandedPath = Utils::Fs::expandTilde(inputPath);
    
    QCOMPARE(expandedPath.data(), expected);
}

QTEST_APPLESS_MAIN(TestTildeExpansion)
#include "testtildeexpansion.moc"
