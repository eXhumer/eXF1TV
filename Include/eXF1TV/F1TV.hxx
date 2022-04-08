#ifndef EXF1TV_F1TV_HXX
#define EXF1TV_F1TV_HXX

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkInformation>

namespace eXF1TV::Service {
class F1TV : public QObject {
  Q_OBJECT

private:
  static QString m_baseUrl;
  int m_locationGroupId;
  QNetworkAccessManager *m_nam;
  QString m_ascendonToken, m_entitlementToken, m_language, m_platform;
  QString authStatus() const;
  QString homeCountry() const;
  QString subStatus() const;

private slots:
  void updateEntitlement();
  void updateLocation();
  void onNetReachChanged(QNetworkInformation::Reachability reachability);

signals:
  void locationGroupIdChanged(int newLocationId);
  void ascendonTokenChanged();
  void entitlementTokenChanged();
  void contentStreams(long contentId, const QJsonArray &streams);
  void pageQueried(int pageNumber, const QJsonObject &pageData);
  void liveSessionsAvailable(const QJsonArray &liveSessions);
  void tokenisedUrl(const QString &playbackUrl, const QString &tokenisedUrl);

public:
  F1TV(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);

public slots:
  void queryPage(int pageNumber);
  void queryLiveContents();
  void queryContentStreams(long contentId);
  void queryTokenisedUrl(const QString &playbackUrl);
  void revoke();
};

} // namespace eXF1TV::Service

#endif // EXF1TV_F1TV_HXX
