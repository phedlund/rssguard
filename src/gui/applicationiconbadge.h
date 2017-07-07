#ifndef APPLICATIONICONBADGE_H
#define APPLICATIONICONBADGE_H

#include <QObject>

class ApplicationIconBadge : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationIconBadge(QObject *parent = 0);

    // Sets the number to be visible in the icon badge, number <= 0 removes it.
    void setNumber(int number = -1, bool any_new_message = false);

    // Returns true if application icon badge CAN be used on this machine.
    static bool isApplicationIconBadgeAvailable();

    // Returns true if application icon badge CAN be costructed and IS enabled in
    // application settings.
    static bool isApplicationIconBadgeActivated();

signals:

public slots:
};

#endif // APPLICATIONICONBADGE_H
