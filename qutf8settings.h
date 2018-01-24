#ifndef QUTF8SETTINGS_H
#define QUTF8SETTINGS_H

#include <QSettings>
#include <QTextCodec>

class QUtf8Settings : public QSettings
{
    Q_OBJECT
public:
    explicit QUtf8Settings(QObject *parent = 0)
        : QSettings(parent)
    {
        setIniCodec("UTF-8");
    }

signals:

public slots:
};

#endif // QUTF8SETTINGS_H
