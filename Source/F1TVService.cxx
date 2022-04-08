#include "F1TV.hxx"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace eXF1TV::Service {
QString F1TV::m_baseUrl = "https://f1tv.formula1.com";

F1TV::F1TV(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam), m_language("ENG"), m_platform("WEB_HLS"),
      m_locationGroupId(0) {
  if (m_nam == nullptr)
    m_nam = new QNetworkAccessManager(this);

  QNetworkInformation *netInfo = QNetworkInformation::instance();
  if (!netInfo &&
      QNetworkInformation::load(QNetworkInformation::Feature::Reachability))
    netInfo = QNetworkInformation::instance();

  if (netInfo) {
    connect(netInfo, &QNetworkInformation::reachabilityChanged, this,
            &F1TV::onNetReachChanged);
    updateLocation();
  }

  connect(QWebEngineProfile::defaultProfile()->cookieStore(),
          &QWebEngineCookieStore::cookieAdded, this,
          [this](const QNetworkCookie &cookie) {
            if (cookie.name() == "login-session" &&
                cookie.domain().endsWith("formula1.com")) {
              QString token =
                  QJsonDocument::fromJson(
                      QUrl::fromPercentEncoding(cookie.value()).toUtf8())
                      .object()["data"]
                      .toObject()["subscriptionToken"]
                      .toString();

              if (m_ascendonToken != token) {
                m_ascendonToken = token;
                emit this->ascendonTokenChanged();
              }
            }
          });

  connect(this, &F1TV::ascendonTokenChanged, this, &F1TV::updateEntitlement);
}

void F1TV::updateEntitlement() {
  if (authStatus() == "R" && subStatus() != "Registered") {
    QNetworkRequest req(QUrl(m_baseUrl + "/2.0/" + authStatus() + "/" +
                             m_language + "/" + m_platform +
                             "/ALL/USER/ENTITLEMENT"));
    req.setRawHeader("ascendontoken", m_ascendonToken.toUtf8());

    auto res = m_nam->get(req);
    connect(res, &QNetworkReply::finished, this, [this, res]() {
      if (res->error() != QNetworkReply::NoError)
        return;

      QJsonObject entitlementJson =
          QJsonDocument::fromJson(res->readAll()).object();
      QString token = entitlementJson["resultObj"]
                          .toObject()["entitlementToken"]
                          .toString();

      if (m_entitlementToken != token) {
        m_entitlementToken = token;
        emit this->entitlementTokenChanged();
      }
    });

    connect(res, &QNetworkReply::finished, res, &QNetworkReply::deleteLater);
  }
}

void F1TV::updateLocation() {
  QUrl reqUrl(m_baseUrl + "/1.0/" + authStatus() + "/" + m_language + "/" +
              m_platform + "/ALL/USER/LOCATION");

  if (authStatus() == "R") {
    QUrlQuery query;
    query.addQueryItem("homeCountry", homeCountry());
    reqUrl.setQuery(query);
  }

  QNetworkRequest req(reqUrl);

  if (authStatus() == "R")
    req.setRawHeader("ascendontoken", m_ascendonToken.toUtf8());

  QNetworkReply *res = m_nam->get(req);
  connect(res, &QNetworkReply::finished, this, [this, res]() {
    if (res->error() != QNetworkReply::NoError)
      return;

    QJsonObject locationJson = QJsonDocument::fromJson(res->readAll()).object();
    int newLocationId = locationJson["resultObj"]
                            .toObject()["userLocation"]
                            .toArray()[0]
                            .toObject()["groupId"]
                            .toInt();

    if (newLocationId != m_locationGroupId) {
      m_locationGroupId = newLocationId;
      emit this->locationGroupIdChanged(m_locationGroupId);
    }
  });

  connect(res, &QNetworkReply::finished, res, &QNetworkReply::deleteLater);
}

void F1TV::onNetReachChanged(QNetworkInformation::Reachability reachability) {
  if (reachability == QNetworkInformation::Reachability::Online)
    updateLocation();
}

void F1TV::queryPage(int pageNumber) {
  QNetworkRequest req(QUrl(m_baseUrl + "/2.0/" + authStatus() + "/" +
                           m_language + "/" + m_platform + "/ALL/PAGE/" +
                           QString::number(pageNumber) + "/" + subStatus() +
                           "/" + QString::number(m_locationGroupId)));

  if (!m_entitlementToken.isEmpty())
    req.setRawHeader("entitlementtoken", m_entitlementToken.toUtf8());

  auto res = m_nam->get(req);
  connect(res, &QNetworkReply::finished, this, [this, pageNumber, res]() {
    if (res->error() != QNetworkReply::NoError) {
      qDebug() << "Error1:" << res->readAll();
      return;
    }

    emit this->pageQueried(pageNumber,
                           QJsonDocument::fromJson(res->readAll()).object());
  });
  connect(res, &QNetworkReply::finished, res, &QNetworkReply::deleteLater);
}

void F1TV::queryLiveContents() {
  auto reqCtx = new QObject;
  connect(this, &F1TV::pageQueried, reqCtx,
          [this, reqCtx](int pageNumber, const QJsonObject &pageData) {
            if (pageNumber == 395) {
              QJsonArray containers =
                  pageData["resultObj"].toObject()["containers"].toArray();
              QJsonArray contentContainers;
              for (auto &&container : containers) {
                auto subContainers = container.toObject()["retrieveItems"]
                                         .toObject()["resultObj"]
                                         .toObject()["containers"]
                                         .toArray();
                for (auto &&contentContainer : subContainers) {
                  auto subContainerJson = contentContainer.toObject();
                  if (subContainerJson.contains("metadata") &&
                      subContainerJson["contentType"].toString() == "VIDEO" &&
                      subContainerJson["contentSubtype"].toString() == "LIVE")
                    contentContainers.append(contentContainer);
                }
              }

              if (!contentContainers.isEmpty())
                emit this->liveSessionsAvailable(contentContainers);

              reqCtx->deleteLater();
            }
          });
  queryPage(395);
}

void F1TV::queryContentStreams(long contentId) {
  QUrl reqUrl(m_baseUrl + "/3.0/" + authStatus() + "/" + m_language + "/" +
              m_platform + "/ALL/CONTENT/VIDEO/" + QString::number(contentId) +
              "/" + subStatus() + "/" + QString::number(m_locationGroupId));

  QUrlQuery query;
  query.addQueryItem("contentId", QString::number(contentId));
  query.addQueryItem("entitlement", subStatus());

  if (authStatus() == "R")
    query.addQueryItem("homeCountry", homeCountry());

  reqUrl.setQuery(query);
  QNetworkRequest req(reqUrl);

  if (!m_entitlementToken.isEmpty())
    req.setRawHeader("entitlementtoken", m_entitlementToken.toUtf8());

  auto res = m_nam->get(req);

  connect(res, &QNetworkReply::finished, this, [this, contentId, res]() {
    if (res->error() != QNetworkReply::NoError) {
      qDebug() << "Error2:" << res->readAll();
      return;
    }

    QJsonArray streams;
    auto kkk = QJsonDocument::fromJson(res->readAll()).object();
    QJsonObject contentMetadataJson = kkk["resultObj"]
                                          .toObject()["containers"]
                                          .toArray()[0]
                                          .toObject()["metadata"]
                                          .toObject();
    if (contentMetadataJson.contains("additionalStreams")) {
      for (auto &&stream : contentMetadataJson["additionalStreams"].toArray()) {
        streams.append(stream);
      }
    } else {
      QJsonObject contentMainFeedStreamJson;
      contentMainFeedStreamJson["type"] = "main";
      contentMainFeedStreamJson["title"] = "MAIN FEED";
      contentMainFeedStreamJson["playbackUrl"] =
          QString("CONTENT/PLAY?contentId=" + QString::number(contentId));
      streams.append(contentMainFeedStreamJson);
    }

    emit this->contentStreams(contentId, streams);
  });
  connect(res, &QNetworkReply::finished, res, &QNetworkReply::deleteLater);
}

void F1TV::queryTokenisedUrl(const QString &playbackUrl) {
  if (m_ascendonToken.isEmpty())
    return;

  QNetworkRequest req(QUrl(m_baseUrl + "/2.0/" + authStatus() + "/" +
                           m_language + "/" + m_platform + "/ALL/" +
                           playbackUrl));
  req.setRawHeader("ascendontoken", m_ascendonToken.toUtf8());

  if (!m_entitlementToken.isEmpty())
    req.setRawHeader("entitlementtoken", m_entitlementToken.toUtf8());

  auto res = m_nam->get(req);
  connect(res, &QNetworkReply::finished, this, [this, playbackUrl, res]() {
    if (res->error() != QNetworkReply::NoError) {
      qDebug() << "Error3:" << res->readAll();
      return;
    }

    emit this->tokenisedUrl(playbackUrl, QJsonDocument::fromJson(res->readAll())
                                             .object()["resultObj"]
                                             .toObject()["url"]
                                             .toString());
  });
  connect(res, &QNetworkReply::finished, res, &QNetworkReply::deleteLater);
}

void F1TV::revoke() {
  m_ascendonToken = QString();
  m_entitlementToken = QString();
}

QString F1TV::authStatus() const {
  return m_ascendonToken.isEmpty() ? "A" : "R";
}

QString F1TV::homeCountry() const {
  if (m_ascendonToken.isEmpty())
    return QString();

  return QJsonDocument::fromJson(
             QByteArray::fromBase64(m_ascendonToken.split(".").at(1).toUtf8(),
                                    QByteArray::Base64Encoding))
      .object()["ExternalAuthorizationsContextData"]
      .toString();
}
QString F1TV::subStatus() const {
  if (m_ascendonToken.isEmpty())
    return "Anonymous";

  QJsonObject subscriptionData =
      QJsonDocument::fromJson(
          QByteArray::fromBase64(m_ascendonToken.split(".").at(1).toUtf8(),
                                 QByteArray::Base64Encoding))
          .object();

  QString subscribedProduct = subscriptionData["SubscribedProduct"].toString();

  if (subscribedProduct.isEmpty())
    return "Registered";

  return subscribedProduct.replace(" ", "_");
}

} // namespace eXF1TV::Service
