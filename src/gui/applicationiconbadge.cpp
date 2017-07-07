#include "applicationiconbadge.h"

#include "miscellaneous/application.h"
#include "miscellaneous/settings.h"

#ifdef Q_OS_MAC
#include <QtMac>
#endif

ApplicationIconBadge::ApplicationIconBadge(QObject *parent) : QObject(parent) {

}

bool ApplicationIconBadge::isApplicationIconBadgeAvailable() {
#ifdef Q_OS_MAC
    return true;
#endif
    return false;
}

bool ApplicationIconBadge::isApplicationIconBadgeActivated() {
  return ApplicationIconBadge::isApplicationIconBadgeAvailable() && qApp->settings()->value(GROUP(GUI), SETTING(GUI::ApplicationIconBadge)).toBool();
}

void ApplicationIconBadge::setNumber(int number, bool any_new_message) {
    Q_UNUSED(any_new_message)
#ifdef Q_OS_MAC
    if (qApp->settings()->value(GROUP(GUI), SETTING(GUI::ApplicationIconBadge)).toBool() && (number > 0)) {
        QtMac::setBadgeLabelText(QString::number(number));
    } else {
        QtMac::setBadgeLabelText("");
    }
#endif
}
