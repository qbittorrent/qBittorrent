#ifndef CHECKBOXWORDWRAP_H
#define CHECKBOXWORDWRAP_H

#include <QWidget>
#include <QHBoxLayout>
#include <QCheckBox>

#include "clickablelabel.h"

class CheckBoxWordWrap : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(bool checked READ isChecked WRITE setChecked)
    Q_PROPERTY(bool wordwrap READ isWordWrap WRITE setWordWrap)
    Q_PROPERTY(QString text READ text WRITE setText)

public:
    CheckBoxWordWrap(QWidget *parent = Q_NULLPTR);
    CheckBoxWordWrap(const QString &text, QWidget *parent = Q_NULLPTR);
    ~CheckBoxWordWrap();
    bool isChecked() const;
    bool isWordWrap() const;
    void setWordWrap(bool wordwrap);
    QString text() const;
    void setText(const QString &text);
    QSize sizeHint() const override;

signals:
    void clicked(bool Checked);

public slots:
    void setChecked(bool checked);
    void chIsClicked();

private slots:
    void labelIsClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void init();
    QHBoxLayout     *m_hMainLayout;
    QCheckBox       *m_checkBox;
    ClickableLabel  *m_label;

};

#endif
