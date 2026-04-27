/**
 * @file tst_memory_client.cpp
 * @brief Tests for MemoryClient: HTTP GET paths, error handling, JSON parsing.
 *
 * Uses a MockNam (QNetworkAccessManager subclass) that returns controlled
 * MockReply instances so no live server is required.
 */

#include "../src/memory/memory_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTest>

// ── Mock infrastructure ────────────────────────────────────────────────────

/**
 * @brief Fake QNetworkReply that returns a preset byte array and optional error.
 *
 * Emits finished() on the next event-loop turn so that callers can connect
 * before the signal fires.
 */
class MockReply : public QNetworkReply {
    Q_OBJECT
  public:
    explicit MockReply(const QByteArray &body,
                       QNetworkReply::NetworkError errCode = QNetworkReply::NoError,
                       QObject *parent = nullptr)
        : QNetworkReply(parent), m_body(body) {
        if (errCode != QNetworkReply::NoError)
            setError(errCode, QStringLiteral("mock network error"));
        setOpenMode(QIODevice::ReadOnly);
        QMetaObject::invokeMethod(this, &MockReply::fire, Qt::QueuedConnection);
    }

    void abort() override {}

    qint64 bytesAvailable() const override {
        return m_body.size() - m_pos + QIODevice::bytesAvailable();
    }

  protected:
    qint64 readData(char *buf, qint64 max) override {
        qint64 n = qMin(max, qint64(m_body.size() - m_pos));
        if (n <= 0)
            return 0;
        memcpy(buf, m_body.constData() + m_pos, static_cast<size_t>(n));
        m_pos += n;
        return n;
    }

  private:
    void fire() { emit finished(); }
    QByteArray m_body;
    qint64 m_pos = 0;
};

/**
 * @brief QNetworkAccessManager subclass that returns a MockReply for every GET.
 *
 * Set nextBody and nextError before each request to control the response.
 */
class MockNam : public QNetworkAccessManager {
  public:
    QByteArray nextBody;
    QNetworkReply::NetworkError nextError = QNetworkReply::NoError;
    QUrl lastUrl;

  protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req,
                                 QIODevice *outgoing) override {
        Q_UNUSED(op)
        Q_UNUSED(outgoing)
        lastUrl = req.url();
        return new MockReply(nextBody, nextError, this);
    }
};

// ── Test class ─────────────────────────────────────────────────────────────

using namespace pairion::memory;

class TestMemoryClient : public QObject {
    Q_OBJECT

  private slots:
    /// fetchEpisodes emits episodesReceived with parsed array on success.
    void fetchEpisodesSuccess() {
        MockNam nam;
        QJsonArray arr;
        arr.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("ep1")},
                               {QStringLiteral("title"), QStringLiteral("Morning")},
                               {QStringLiteral("startTime"), QStringLiteral("2024-01-15T10:00:00Z")},
                               {QStringLiteral("turnCount"), 5}});
        nam.nextBody = QJsonDocument(arr).toJson();

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy spy(&client, &MemoryClient::episodesReceived);

        client.fetchEpisodes();
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        QJsonArray result = spy.at(0).at(0).value<QJsonArray>();
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).toObject().value(QStringLiteral("id")).toString(),
                 QStringLiteral("ep1"));

        // URL contains expected path and query
        QVERIFY(nam.lastUrl.path().contains(QStringLiteral("/memory/episodes")));
        QVERIFY(nam.lastUrl.query().contains(QStringLiteral("userId=default-user")));
    }

    /// fetchEpisodes handles object response with "items" wrapper.
    void fetchEpisodesItemsWrapper() {
        MockNam nam;
        QJsonArray items;
        items.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("ep2")}});
        QJsonObject wrapped;
        wrapped[QStringLiteral("items")] = items;
        nam.nextBody = QJsonDocument(wrapped).toJson();

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy spy(&client, &MemoryClient::episodesReceived);

        client.fetchEpisodes();
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        QJsonArray result = spy.at(0).at(0).value<QJsonArray>();
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).toObject().value(QStringLiteral("id")).toString(),
                 QStringLiteral("ep2"));
    }

    /// fetchEpisodes emits fetchError on network error.
    void fetchEpisodesNetworkError() {
        MockNam nam;
        nam.nextBody.clear();
        nam.nextError = QNetworkReply::ConnectionRefusedError;

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy errorSpy(&client, &MemoryClient::fetchError);
        QSignalSpy successSpy(&client, &MemoryClient::episodesReceived);

        client.fetchEpisodes();
        QVERIFY(errorSpy.wait(1000));
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("fetchEpisodes"));
        QCOMPARE(successSpy.count(), 0);
    }

    /// fetchEpisodes emits fetchError on malformed JSON.
    void fetchEpisodesJsonParseError() {
        MockNam nam;
        nam.nextBody = QByteArray("not valid json {{{{");

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy errorSpy(&client, &MemoryClient::fetchError);

        client.fetchEpisodes();
        QVERIFY(errorSpy.wait(1000));
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.at(0).at(1).toString().contains(QStringLiteral("JSON parse error")));
    }

    /// fetchEpisodes emits episodesReceived with empty array from empty object.
    void fetchEpisodesEmptyObject() {
        MockNam nam;
        nam.nextBody = QJsonDocument(QJsonObject{}).toJson();

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy spy(&client, &MemoryClient::episodesReceived);

        client.fetchEpisodes();
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QJsonArray>().size(), 0);
    }

    /// fetchTurns emits turnsReceived with correct URL.
    void fetchTurnsSuccess() {
        MockNam nam;
        QJsonArray turns;
        turns.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                 {QStringLiteral("content"), QStringLiteral("Hello")},
                                 {QStringLiteral("timestamp"), QStringLiteral("2024-01-15T10:01:00Z")}});
        nam.nextBody = QJsonDocument(turns).toJson();

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy spy(&client, &MemoryClient::turnsReceived);

        client.fetchTurns(QStringLiteral("ep42"));
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        QJsonArray result = spy.at(0).at(0).value<QJsonArray>();
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).toObject().value(QStringLiteral("role")).toString(),
                 QStringLiteral("user"));

        QVERIFY(nam.lastUrl.path().contains(QStringLiteral("/memory/episodes/ep42/turns")));
        QVERIFY(nam.lastUrl.query().contains(QStringLiteral("userId=default-user")));
    }

    /// fetchTurns emits fetchError on network error.
    void fetchTurnsNetworkError() {
        MockNam nam;
        nam.nextError = QNetworkReply::ConnectionRefusedError;

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy errorSpy(&client, &MemoryClient::fetchError);

        client.fetchTurns(QStringLiteral("ep1"));
        QVERIFY(errorSpy.wait(1000));
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("fetchTurns"));
    }

    /// fetchPreferences emits preferencesReceived with correct URL.
    void fetchPreferencesSuccess() {
        MockNam nam;
        QJsonArray prefs;
        prefs.append(QJsonObject{{QStringLiteral("key"), QStringLiteral("theme")},
                                 {QStringLiteral("value"), QStringLiteral("dark")},
                                 {QStringLiteral("updatedAt"), QStringLiteral("2024-01-10T09:00:00Z")}});
        nam.nextBody = QJsonDocument(prefs).toJson();

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy spy(&client, &MemoryClient::preferencesReceived);

        client.fetchPreferences();
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        QJsonArray result = spy.at(0).at(0).value<QJsonArray>();
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).toObject().value(QStringLiteral("key")).toString(),
                 QStringLiteral("theme"));

        QVERIFY(nam.lastUrl.path().contains(QStringLiteral("/memory/preferences")));
        QVERIFY(nam.lastUrl.query().contains(QStringLiteral("userId=default-user")));
    }

    /// fetchPreferences emits fetchError on network error.
    void fetchPreferencesNetworkError() {
        MockNam nam;
        nam.nextError = QNetworkReply::ConnectionRefusedError;

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        QSignalSpy errorSpy(&client, &MemoryClient::fetchError);

        client.fetchPreferences();
        QVERIFY(errorSpy.wait(1000));
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("fetchPreferences"));
    }
};

QTEST_GUILESS_MAIN(TestMemoryClient)
#include "tst_memory_client.moc"
