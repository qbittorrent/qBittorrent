#include "checkboxwordwrap.h"

CheckBoxWordWrap::CheckBoxWordWrap(QWidget *parent)
    : QWidget(parent)
    , m_hMainLayout(new QHBoxLayout(this))
    , m_checkBox(new QCheckBox(this))
    , m_label(new ClickableLabel(this))
{
    init();
}

CheckBoxWordWrap::CheckBoxWordWrap(const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_hMainLayout(new QHBoxLayout(this))
    , m_checkBox(new QCheckBox(this))
    , m_label(new ClickableLabel(text, this))
{
    init();
}

CheckBoxWordWrap::~CheckBoxWordWrap()
{
    delete m_label;
    delete m_checkBox;
    delete m_hMainLayout;
}

bool CheckBoxWordWrap::isChecked() const
{
    return m_checkBox->isChecked();
}

bool CheckBoxWordWrap::isWordWrap() const
{
    return m_label->wordWrap();
}

void CheckBoxWordWrap::setWordWrap(bool wordwrap)
{
    m_label->setWordWrap(wordwrap);
}

QString CheckBoxWordWrap::text() const
{
    return m_label->text();
}

void CheckBoxWordWrap::setText(const QString &text)
{
    m_label->setText(text);
}

QSize CheckBoxWordWrap::sizeHint() const
{
    QFontMetrics fm(m_label->font());
    QRect bRect = fm.boundingRect(m_label->rect(), int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap), m_label->text());
    QSize ret = QSize(QWidget::sizeHint().width(), bRect.height());
    return ret;
}

void CheckBoxWordWrap::setChecked(bool checked)
{
    m_checkBox->setChecked(checked);
}

void CheckBoxWordWrap::labelIsClicked()
{
    m_checkBox->setChecked(!isChecked());
}

void CheckBoxWordWrap::chIsClicked()
{
    emit clicked(isChecked());
}

void CheckBoxWordWrap::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGeometry();
}

void CheckBoxWordWrap::init()
{
    m_hMainLayout->setContentsMargins(0, 0, 0, 0);
    m_hMainLayout->addWidget(m_checkBox);
    m_hMainLayout->addWidget(m_label);

    m_checkBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_label->setWordWrap(true);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    connect(m_label, SIGNAL(clicked()), this, SLOT(labelIsClicked()));
    connect(m_label, SIGNAL(clicked()), this, SLOT(chIsClicked()));
    connect(m_checkBox, SIGNAL(clicked()), this, SLOT(chIsClicked()));
}
