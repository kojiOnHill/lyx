// -*- C++ -*-
/**
 * \file GuiCollaborate.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUICOLLABORATE_H
#define GUICOLLABORATE_H

#include <QObject>

#include <QNetworkReply>

namespace lyx {
namespace frontend {

class GuiCollaborate : public QObject
{
	Q_OBJECT

public:
	explicit GuiCollaborate(QObject* parent = 0);
	~GuiCollaborate();
	void githubAuth();
	void githubAuthOld();
	void createRepository();
	enum GIT {
		auth,
		createRepo
	};

private Q_SLOTS:
	void onGranted();
	void onReadyRead();
	void onFinished(QNetworkReply*);
	void onTested();
	void onAskedToAuthorize(const QUrl &, const QString &, const QUrl &);

protected:
	void timerEvent(QTimerEvent *event) override;

Q_SIGNALS:
	void test();

private:
	struct Private;
	Private * const d;
};

} // namespace frontend
} // namespace lyx

#endif // GUICOLLABORATE_H
