/**
 * \file GuiCollaborate.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <QtCore/qcoreevent.h>
#include <QtCore/qeventloop.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmessagebox.h>
#include <config.h>

#include "GuiCollaborate.h"
#include "qt_helpers.h"
#include "support/debug.h"
#include "support/Package.h"
#include "support/qstring_helpers.h"

#include <QHttp2Configuration>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkRequestFactory>
#include <QNetworkReply>

#include <QDesktopServices>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuth2DeviceAuthorizationFlow>
#include <QUrl>
#include <QSystemTrayIcon>
#include <QTimerEvent>

#include <QAbstractOAuthReplyHandler>
#include <QSignalSpy>
#include <QTest>

using namespace std;

namespace lyx {
namespace frontend {

struct GuiCollaborate::Private
{
	// Github information
	QString const auth_scheme_ = "https";
	QString const auth_host_ = "github.com";
	QString const auth_path_ = "/login/device/code";
	QString const client_id_ = "Iv23liuXnC0RE3zhAaji";
	QSet<QByteArray> const scope_ = {"repo", "user"};
	QUrl const token_url_ = QUrl("https://github.com/login/oauth/access_token");

	QOAuth2DeviceAuthorizationFlow * device_flow_ = new QOAuth2DeviceAuthorizationFlow;
	QNetworkReply * reply_ = nullptr;
};


GuiCollaborate::GuiCollaborate(QObject* parent): QObject(parent), d(new Private)
{
}


GuiCollaborate::~GuiCollaborate()
{
	delete d;
}


void GuiCollaborate::githubAuth()
{
	// See "OAuth 2.0 Device Authorization Grant" at
	//     https://datatracker.ietf.org/doc/html/rfc8628
	// for the grant procedure
	QUrl auth_url;
	auth_url.setHost(d->auth_host_);
	auth_url.setScheme(d->auth_scheme_);
	auth_url.setPath(d->auth_path_);

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
	QNetworkRequest request(auth_url);
	request.setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
	request.setTransferTimeout();
	request.setAttribute(QNetworkRequest::Http2AllowedAttribute, QVariant(true));

	d->device_flow_->setAuthorizationUrl(auth_url);
	d->device_flow_->setTokenUrl(d->token_url_);
	d->device_flow_->setRequestedScopeTokens(d->scope_);
	d->device_flow_->setClientIdentifier(d->client_id_);
	d->device_flow_->setContentType(QAbstractOAuth::ContentType::WwwFormUrlEncoded);
	// Need to ask Github to respond in JSON format
	d->device_flow_->setNetworkRequestModifier(this, [this](QNetworkRequest& req, QAbstractOAuth::Stage stage){
		// Q_UNUSED(stage)
		if (stage == QAbstractOAuth::Stage::RequestingAuthorization)
			req.setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
		else
			req.setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
		req.setTransferTimeout();
		if (stage == QAbstractOAuth::Stage::RequestingAccessToken)
			qDebug() << "Requesting access token";
		if (stage == QAbstractOAuth::Stage::RefreshingAccessToken)
			qDebug() << "Refreshing access token, cur token = " << d->device_flow_->token();
	});

	connect(d->device_flow_, &QOAuth2DeviceAuthorizationFlow::authorizeWithUserCode,
	        this, &GuiCollaborate::onAskedToAuthorize);
	connect(d->device_flow_, &QAbstractOAuth::requestFailed,
	        this, [=](const QAbstractOAuth::Error error){
		qDebug() << "Request failure received: Error code = " << (int)error;
	});
	connect(d->device_flow_, &QAbstractOAuth2::serverReportedErrorOccurred, this,
	    [=](const QString &error, const QString &errorDescription, const QUrl &uri) {
	        // Check server reported error details if needed
	        Q_UNUSED(error);
	        Q_UNUSED(errorDescription);
	        Q_UNUSED(uri);
			qDebug() << "Server error occured";
	    }
	);
	connect(d->device_flow_, &QAbstractOAuth2::authorizationCallbackReceived,
	        this, [=](const QVariantMap &data){
		Q_UNUSED(data);
		qDebug() << "callback received";
	});
	connect(d->device_flow_, &QAbstractOAuth2::accessTokenAboutToExpire,
	        this, [=](){qDebug() << "access token about to expire";});
	connect(d->device_flow_, &QAbstractOAuth2::stateChanged,
	        this, [=](const QString &state){qDebug() << "State changed. state = " << state;});
	connect(d->device_flow_, &QAbstractOAuth2::tokenChanged,
	        this, [=](const QString &token){qDebug() << "Token changed. token = " << token;});
	connect(d->device_flow_, &QAbstractOAuth::granted, this, &GuiCollaborate::onGranted);

	// QEventLoop loop;
	// connect(d->device_flow_, &QAbstractOAuth::granted, &loop, &QEventLoop::quit);
	// connect(d->device_flow_, &QAbstractOAuth::requestFailed, &loop, &QEventLoop::quit);
	// loop.exec();

	d->device_flow_->grant();

	d->device_flow_->setAutoRefresh(true);

	QSignalSpy spy(d->device_flow_, &QOAuth2DeviceAuthorizationFlow::authorizeWithUserCode);
	QSignalSpy callback_spy(d->device_flow_, &QOAuth2DeviceAuthorizationFlow::authorizationCallbackReceived);

	spy.wait(30000);
	callback_spy.wait(120000);
#endif // QT_VERSION
}


void GuiCollaborate::onGranted()
{
	QNetworkRequestFactory api{{d->device_flow_->tokenUrl()}};

	LYXERR0("Granted!");
	api.setBearerToken(d->device_flow_->token().toLatin1());
	qDebug() << "obtained token:" << d->device_flow_->token().toStdString();
}

void GuiCollaborate::onTested()
{
	LYXERR0("Tested!");
}

void GuiCollaborate::onReadyRead()
{
	LYXERR0(d->reply_->readAll().toStdString());
}

void GuiCollaborate::onFinished(QNetworkReply* reply)
{
	LYXERR0(reply->readAll().toStdString());
}

void GuiCollaborate::onAskedToAuthorize(QUrl const &verification_url,
                                        QString const &user_code,
                                        QUrl const &complete_verification_url)
{
	if(complete_verification_url.isValid()) {
		QDesktopServices::openUrl(complete_verification_url);
		qDebug() << "Complete verification uri:" << complete_verification_url;
	} else {
		// Note for macOS:
		// - The Growl notification system must be installed for
		//   QSystemTrayIcon::showMessage() to display messages (2025/10/1)
		//       (https://doc.qt.io/qt-6/qtwidgets-desktop-systray-example.html)
		// - On the other hand, Growl does not support Apple Silicon and also
		//   has ceased its development
		//       (https://growl.github.io/growl/)
#ifndef Q_OS_MACOS
		QSystemTrayIcon tray;
		QPixmap pix_icon(toqstr(support::package().system_support().absFileName() + "images/lyx.png"));
		LYXERR0("is null? " << pix_icon.isNull());
		QIcon lyx_icon(pix_icon);
		QMenu menu;
		tray.setIcon(lyx_icon);
		tray.setToolTip("This is a tooltip");
		tray.setContextMenu(&menu);
		if (tray.supportsMessages()) {
			tray.showMessage("Code", user_code, QSystemTrayIcon::Information);
		}
		tray.show();
#endif
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Information);
		msgBox.setText(qt_("Jumping to Device Activation Page"));
		msgBox.setInformativeText(qt_("User code = " + user_code));
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.exec();

		QDesktopServices::openUrl(verification_url);

		QMessageBox msgBox2;
		msgBox2.setIcon(QMessageBox::Information);
		msgBox2.setText(qt_("Enter the following code"));
		msgBox2.setInformativeText(qt_("User code = " + user_code));
		msgBox2.setStandardButtons(QMessageBox::Ok);
		msgBox2.setDefaultButton(QMessageBox::Ok);
		msgBox2.exec();

		qDebug() << "Verification uri and usercode:" << verification_url <<
		            user_code;
	}
}


void GuiCollaborate::githubAuthOld()
{
	// requires Qt 6.9 or later

	QString const auth_host = "github.com";
	QString const auth_path = "/login/device/code";
	QString const client_id = "Iv23liuXnC0RE3zhAaji";

	// Somehow the below doesn't work with setPath() (2025/9/30)
	// QUrl auth_url(auth_host);
	QUrl auth_url;
	auth_url.setHost(auth_host);
	auth_url.setScheme("https");
	auth_url.setPath(auth_path);

	// Allow HTTP/2 preconnection
	QSslConfiguration ssl_config;
	ssl_config.setAllowedNextProtocols(
	            QList<QByteArray>(
	                {
	                    QSslConfiguration::ALPNProtocolHTTP2,
	                    QSslConfiguration::NextProtocolHttp1_1
	                }));

	QNetworkAccessManager * manager = new QNetworkAccessManager;
	// manager->connectToHostEncrypted(auth_host, 443, ssl_config);
	// QNetworkRequest request(QUrl("https://api.github.com"));
	// request.setRawHeader(QByteArray("Authorization"), QByteArray("token OAUTH-TOKEN"));
	// d->reply_= manager->get(request);
	// connect(d->reply_, &QIODevice::readyRead, this, &GuiCollaborate::onReadyRead);
	// const QSet<QByteArray> scope = {QByteArray("identity"), QByteArray("read")};

	QNetworkRequest request(auth_url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	// Specifying below will start connecting immediately after post
	// request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, true);
	request.setTransferTimeout();
	LYXERR0("request timeout: " << request.transferTimeout());

	QByteArray data("client_id=Iv23liuXnC0RE3zhAaji&scope=repo");

	d->reply_ = manager->post(request, data);
	connect(d->reply_, &QNetworkReply::finished, this, [this](){qDebug() << "read data = "<< d->reply_->readAll().toStdString();});
	// connect(reply, &QNetworkReply::requestSent, this, [](){qDebug() << "request sent";});
	connect(d->reply_, &QNetworkReply::requestSent, this, &GuiCollaborate::onTested);
	connect(d->reply_, &QNetworkReply::socketStartedConnecting, this,
	        [](){qDebug() << "Socket started connecting";});
	connect(d->reply_, &QNetworkReply::errorOccurred, this, [](QNetworkReply::NetworkError code){qDebug() << "Error occurred: " << (int)code;});

	// d->reply_->startTimer(30s);
	// d->reply_->deleteLater();

	// QSignalSpy spy(d->reply_, &QNetworkReply::socketStartedConnecting);
	QSignalSpy spy(d->reply_, &QNetworkReply::errorOccurred);

	spy.wait(30000);

	LYXERR0("Is finished? " << d->reply_->isFinished() << " count: " << spy.count());
	LYXERR0("Is Running?  " << d->reply_->isRunning());
	// // QUrl authorizationUrl("https://github.com/login/oauth/authorize");
	// // QUrl accessTokenUrl("https://github.com/login/oauth/access_token");
	// QUrl authorizationUrl("https://github.com/login/oauth/authorize?scope=repo&client_id=" + client_id);
	// QUrl accessTokenUrl("https://api.github.com/app/installations/87407074/access_tokens");

	// QOAuth2AuthorizationCodeFlow m_oauth;
	// QOAuthUriSchemeReplyHandler m_handler;
	// QNetworkRequestFactory m_api;

	// m_oauth.setAuthorizationUrl(authorizationUrl);
	// m_oauth.setTokenUrl(QUrl(accessTokenUrl));
	// m_oauth.setClientIdentifier(client_id);
	// m_oauth.setRequestedScopeTokens(scope);

	// connect(&m_oauth, &QAbstractOAuth::authorizeWithBrowser, this, &QDesktopServices::openUrl);
	// connect(&m_oauth, &QAbstractOAuth::granted, this, [&]() {
	//     // Here we use QNetworkRequestFactory to store the access token
	//     m_api.setBearerToken(m_oauth.token().toLatin1());
	//     m_handler.close();
	// });

	// // m_handler.setRedirectUrl(QUrl{"com.example.myqtapp://oauth2redirect"_L1});
	// m_oauth.setReplyHandler(&m_handler);

	// // Initiate the authorization
	// if (m_handler.isListening()) {
	//     m_oauth.grant();
	// }
}

void GuiCollaborate::timerEvent(QTimerEvent *event)
{
    qDebug() << "Timer ID:" << event->id();
}

} // namespace frontend

} // namespace lyx

#include "moc_GuiCollaborate.cpp"
