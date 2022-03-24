#ifndef EXF1TV_F1TV_HXX
#define EXF1TV_F1TV_HXX

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
  void ascendonTokenUpdated();
  void entitlementTokenUpdated();
  void pageQueried(int pageNumber, const QJsonObject &pageData);
  void liveSessionsAvailable(const QJsonArray &liveSessions);

public:
  F1TV(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);

public slots:
  void queryPage(int pageNumber);
  void queryLiveSessions();
  void querySessionChannels(long contentId);
  void queryTokenisedUrl(const QString &contentUrl);
  void searchSeasonEvents(int year);
  void searchSeasonEpisodes(int year);
  void searchEventVideos(const QString &meetingKey);
  void searchGenreVideos(const QString &genre);
  void revoke();
};

} // namespace eXF1TV::Service

#endif // EXF1TV_F1TV_HXX
