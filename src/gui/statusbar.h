// This file is part of RSS Guard.
//
// Copyright (C) 2011-2015 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>


class QProgressBar;
class QToolButton;
class QLabel;

class StatusBar : public QStatusBar {
    Q_OBJECT

  public:
    // Constructors and destructors.
    explicit StatusBar(QWidget *parent = 0);
    virtual ~StatusBar();

    inline QToolButton *fullscreenSwitcher() const {
      return m_fullscreenSwitcher;
    }

  public slots:
    // Progress bar operations
    void showProgressFeeds(int progress, const QString &label);
    void clearProgressFeeds();

    void showProgressDownload(int progress, const QString &tooltip);
    void clearProgressDownload();

  private:
    QProgressBar *m_barProgressFeeds;
    QLabel *m_lblProgressFeeds;
    QProgressBar *m_barProgressDownload;
    QLabel *m_lblProgressDownload;
    QToolButton *m_fullscreenSwitcher;
    
};

#endif // STATUSBAR_H
