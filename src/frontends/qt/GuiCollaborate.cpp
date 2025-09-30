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
#include <config.h>

#include "GuiCollaborate.h"
#include "support/debug.h"
#include "support/qstring_helpers.h"

#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkRequestFactory>
#include <QNetworkReply>

#include <QDesktopServices>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthUriSchemereplyHandler>

#include <QOAuth2DeviceAuthorizationFlow>
#include <QRestAccessManager>
#include <QRestReply>
#include <QUrl>

#include <QSignalSpy>
#include <QTest>

using namespace std;

namespace lyx {
namespace frontend {

struct GuiCollaborate::Private
{
	QNetworkReply * reply_ = nullptr;
};


GuiCollaborate::GuiCollaborate(QObject* parent): QObject(parent), d(new Private)
{
	connect(this, &GuiCollaborate::test, this, &GuiCollaborate::onTested);
	// connect(this, SIGNAL(test()), this, SLOT(onTested()));
}


GuiCollaborate::~GuiCollaborate()
{
	delete d;
}


void GuiCollaborate::githubAuth()
{
	QString const auth_host = "github.com";
	QString const auth_path = "/login/device/code";
	QString const client_id = "Iv23liuXnC0RE3zhAaji";

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

	QNetworkAccessManager manager;
	// manager.connectToHostEncrypted(auth_host, 443, ssl_config);
	// QRestAccessManager rest(&manager);

	// LYXERR0("connectToHostEncrypted done");

	QNetworkRequest request(auth_url);
	// request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	// request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, true);
	// request.setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
	// request.setRawHeader(QByteArray("Accept"), QByteArray("application/xml"));

	// const QByteArray data("client_id=Iv23liuXnC0RE3zhAaji&scope=identity");
	// rest.post(request, data, this, [this](QRestReply &reply){
	// 	if (!reply.isSuccess()) {
	// 		qDebug() << "not success";
	// 	}
	// 	qDebug() << "rest replied";
	// });

	request.setTransferTimeout();

	// Use the custom network access manager set up above
	QOAuth2DeviceAuthorizationFlow *device_flow = new QOAuth2DeviceAuthorizationFlow;
	device_flow->setNetworkAccessManager(&manager);
	// device_flow->prepareRequest(&request, QByteArray("POST"), QByteArray("client_id=Iv23liuXnC0RE3zhAaji&scope=repo"));
	device_flow->setAuthorizationUrl(auth_url);
	device_flow->setTokenUrl(QUrl("https://github.com/login/oauth/access_token"));
	device_flow->setRequestedScopeTokens({"repo", "user"});
	device_flow->setClientIdentifier(client_id);
	device_flow->setContentType(QAbstractOAuth::ContentType::WwwFormUrlEncoded);

	// If your app requires a client secret for the device flow, set it here:
	// device_flow->setClientIdentifierSharedKey("YOUR_GITHUB_CLIENT_SECRET");

	// connect(&manager, &QNetworkAccessManager::destroyed, &rest, &QRestAccessManager::deleteLater);

	connect(device_flow, &QOAuth2DeviceAuthorizationFlow::authorizeWithUserCode,
	        this, [=](const QUrl &verificationUrl, const QString &userCode, const QUrl &completeVerificationUrl)
	{
		if(completeVerificationUrl.isValid()) {
			QDesktopServices::openUrl(completeVerificationUrl);
			qDebug() << "Complete verification uri:" << completeVerificationUrl;
		} else {
			QDesktopServices::openUrl(verificationUrl);
			qDebug() << "Verification uri and usercode:" << verificationUrl << userCode;
		}
	} );

	connect(device_flow, &QAbstractOAuth::requestFailed,
	        this, [=](const QAbstractOAuth::Error error){
		qDebug() << "Request failure received: Error code = " << (int)error;
	});

	connect(device_flow, &QAbstractOAuth2::authorizationCallbackReceived,
	        this, [=](const QVariantMap &data){
		Q_UNUSED(data);
		qDebug() << "callback received";
	});

	connect(device_flow, &QAbstractOAuth2::accessTokenAboutToExpire,
	        this, [=](){qDebug() << "access token about to expire";});

	connect(device_flow, &QAbstractOAuth2::stateChanged,
	        this, [=](const QString &state){qDebug() << state;});

	connect(device_flow, &QAbstractOAuth2::serverReportedErrorOccurred, this,
	    [=](const QString &error, const QString &errorDescription, const QUrl &uri) {
	        // Check server reported error details if needed
	        Q_UNUSED(error);
	        Q_UNUSED(errorDescription);
	        Q_UNUSED(uri);
			qDebug() << "Server error occured";
	    }
	);

	connect(device_flow, &QAbstractOAuth::granted, this, &GuiCollaborate::onGranted);

	LYXERR0("Authorization URL = " << device_flow->authorizationUrl().toString());
	LYXERR0("Client Identifier = " << device_flow->clientIdentifier());
	LYXERR0("Response type   = " << device_flow->responseType());
	device_flow->grant();

	QSignalSpy spy(device_flow, &QOAuth2DeviceAuthorizationFlow::authorizeWithUserCode);

	spy.wait(30000);

	LYXERR0("user code = " << device_flow->userCode());

	// QUrl url;
	// url.setScheme("https");
	// url.setAuthority("github.com");
	// url.setPath("/login/device/code");

	// QNetworkAccessManager * manager = new QNetworkAccessManager(this);
	// connect(manager, &QNetworkAccessManager::finished, this, &GuiCollaborate::onFinished);

	// // QNetworkRequest request(QUrl("https://github.com/login/device/code"));
	// QNetworkRequest request(url);
	// request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	// request.setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
	// request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, true);
	// QUrlQuery urlQuery;
	// urlQuery.addQueryItem("client_id", "Iv23liuXnC0RE3zhAaji");
	// urlQuery.addQueryItem("scope", "repo");
	// // const QByteArray data("client_id=Iv23liuXnC0RE3zhAaji&scope=identity");

	// QUrl params;
	// params.setQuery(urlQuery);

	// QHttpMultiPart *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	// QHttpPart text_part;
	// // Commenting out the below line causes "500 Internal Server Error"
	// // text_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
	// text_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"text\""));

	// // text_part.setBody(params.toEncoded());
	// text_part.setBody(QByteArray("client_id=Iv23liuXnC0RE3zhAaji&scope=repo"));
	// multi_part->append(text_part);
	// LYXERR0("dest URL = " << fromqstr(request.url().toDisplayString()));

	// manager->connectToHostEncrypted("https://github.com");
	// QNetworkReply* reply = manager->post(request, multi_part);
	// // QNetworkReply* reply = manager->post(request, params.toEncoded());
	// int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	// LYXERR0("return to sent post: " << v);

	// QEventLoop eventLoop;
	// connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
	// eventLoop.exec();

	// auto data = reply->readAll();
	// multi_part->setParent(reply);
	// reply->deleteLater();
	// LYXERR0("received data = " << data.toStdString());
	// connect(reply, &QNetworkReply::readyRead, this, &GuiCollaborate::onReadyRead);
}


void GuiCollaborate::onGranted()
{
	// QNetworkRequestFactory api{{device_flow->tokenUrl()}};

	LYXERR0("Granted!");
	// api.setBearerToken(device_flow->token().toLatin1());
	// qDebug() << "obtained token:" << device_flow->token().toStdString();
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
