#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include "preferences.h"

enum AdvSettingsCols {PROPERTY, VALUE};
enum AdvSettingsRows {DISK_CACHE};
#define ROW_COUNT 1
enum AdvValueTYPE {UINT=1001};

class AdvancedSettings: public QTableWidget {
  Q_OBJECT

private:
  QSpinBox *spin_cache;

public:
  AdvancedSettings(QWidget *parent=0): QTableWidget(parent) {
    // Set visual appearance
    setColumnCount(2);
    QStringList header;
    header << tr("Property") << tr("Value");
    setHorizontalHeaderLabels(header);
    setColumnWidth(0, width()/2);
    horizontalHeader()->setStretchLastSection(true);
    setRowCount(ROW_COUNT);
    // Load settings
    loadAdvancedSettings();
  }

  ~AdvancedSettings() {
    delete spin_cache;
  }

  public slots:
  void saveAdvancedSettings() {
    // Disk cache
    Preferences::setDiskCacheSize(spin_cache->value());
  }

protected slots:
  void loadAdvancedSettings() {
    // Disk write cache
    setItem(DISK_CACHE, PROPERTY, new QTableWidgetItem(tr("Disk write cache size (MiB)"), UINT));
    spin_cache = new QSpinBox();
    connect(spin_cache, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    spin_cache->setMinimum(1);
    spin_cache->setMaximum(200);
    spin_cache->setValue(Preferences::diskCacheSize());
    setCellWidget(DISK_CACHE, VALUE, spin_cache);
  }

  void emitSettingsChanged() {
    emit settingsChanged();
  }

signals:
  void settingsChanged();
};

#endif // ADVANCEDSETTINGS_H
