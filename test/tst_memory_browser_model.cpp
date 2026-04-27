/**
 * @file tst_memory_browser_model.cpp
 * @brief Tests for MemoryBrowserModel: state transitions, property bindings, error handling.
 *
 * Uses the same MockNam/MockReply infrastructure as tst_memory_client.cpp to
 * supply controlled HTTP responses without a live server.
 */

#include "../src/memory/memory_browser_model.h"
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
        if (n <= 0) return 0;
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
 * @brief MockNam with per-request response queue.
 *
 * Push responses via enqueue() before issuing requests; each createRequest()
 * pops the front of the queue. Defaults to empty array if queue is empty.
 */
class MockNam : public QNetworkAccessManager {
  public:
    struct Response {
        QByteArray body;
        QNetworkReply::NetworkError error = QNetworkReply::NoError;
    };

    void enqueue(const QByteArray &body,
                 QNetworkReply::NetworkError err = QNetworkReply::NoError) {
        m_queue.append({body, err});
    }

  protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req,
                                 QIODevice *outgoing) override {
        Q_UNUSED(op) Q_UNUSED(req) Q_UNUSED(outgoing)
        if (m_queue.isEmpty())
            return new MockReply(QJsonDocument(QJsonArray{}).toJson(),
                                 QNetworkReply::NoError, this);
        auto r = m_queue.takeFirst();
        return new MockReply(r.body, r.error, this);
    }

    QList<Response> m_queue;
};

// ── Helpers ────────────────────────────────────────────────────────────────

static QByteArray makeEpisodes(int count = 2) {
    QJsonArray arr;
    for (int i = 0; i < count; ++i) {
        arr.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("ep%1").arg(i + 1)},
            {QStringLiteral("title"), QStringLiteral("Episode %1").arg(i + 1)},
            {QStringLiteral("startTime"), QStringLiteral("2024-01-15T10:00:00Z")},
            {QStringLiteral("turnCount"), i * 3 + 1}
        });
    }
    return QJsonDocument(arr).toJson();
}

static QByteArray makeTurns() {
    QJsonArray arr;
    arr.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), QStringLiteral("Hello Jarvis")},
        {QStringLiteral("timestamp"), QStringLiteral("2024-01-15T10:00:30Z")}
    });
    arr.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("assistant")},
        {QStringLiteral("content"), QStringLiteral("Good morning.")},
        {QStringLiteral("timestamp"), QStringLiteral("2024-01-15T10:00:32Z")}
    });
    return QJsonDocument(arr).toJson();
}

static QByteArray makePreferences() {
    QJsonArray arr;
    arr.append(QJsonObject{
        {QStringLiteral("key"), QStringLiteral("theme")},
        {QStringLiteral("value"), QStringLiteral("dark")},
        {QStringLiteral("updatedAt"), QStringLiteral("2024-01-10T09:00:00Z")}
    });
    return QJsonDocument(arr).toJson();
}

// ── Test class ─────────────────────────────────────────────────────────────

using namespace pairion::memory;

class TestMemoryBrowserModel : public QObject {
    Q_OBJECT

  private slots:
    /// Initial state: empty lists, not loading, no error.
    void initialState() {
        MockNam nam;
        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QVERIFY(model.episodes().isEmpty());
        QVERIFY(model.selectedEpisodeTurns().isEmpty());
        QVERIFY(model.preferences().isEmpty());
        QCOMPARE(model.loading(), false);
        QVERIFY(model.errorMessage().isEmpty());
        QVERIFY(model.selectedEpisodeId().isEmpty());
        QCOMPARE(model.hasMemory(), false);
    }

    /// refresh() populates episodes and preferences; loading transitions correctly.
    void refreshSuccess() {
        MockNam nam;
        nam.enqueue(makeEpisodes(2));   // episodes request
        nam.enqueue(makePreferences()); // preferences request

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QSignalSpy loadingSpy(&model, &MemoryBrowserModel::loadingChanged);
        QSignalSpy episodesSpy(&model, &MemoryBrowserModel::episodesChanged);
        QSignalSpy prefsSpy(&model, &MemoryBrowserModel::preferencesChanged);
        QSignalSpy hasMemorySpy(&model, &MemoryBrowserModel::hasMemoryChanged);

        model.refresh();
        QCOMPARE(model.loading(), true);

        // Wait for both responses
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);

        QCOMPARE(model.episodes().size(), 2);
        QCOMPARE(model.preferences().size(), 1);
        QCOMPARE(model.hasMemory(), true);
        QVERIFY(episodesSpy.count() >= 1);
        QVERIFY(prefsSpy.count() >= 1);
        QVERIFY(hasMemorySpy.count() >= 1);

        // Check formatted timestamp stored
        QVariantMap ep = model.episodes().at(0).toMap();
        QVERIFY(!ep[QStringLiteral("startTime")].toString().isEmpty());
    }

    /// refresh() clears selection before fetching.
    void refreshClearsSelection() {
        MockNam nam;
        nam.enqueue(makeTurns()); // selectEpisode request
        nam.enqueue(makeEpisodes());
        nam.enqueue(makePreferences());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        // Pre-select an episode
        model.selectEpisode(QStringLiteral("ep1"));
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QCOMPARE(model.selectedEpisodeId(), QStringLiteral("ep1"));
        QCOMPARE(model.selectedEpisodeTurns().size(), 2);

        model.refresh();
        QVERIFY(model.selectedEpisodeId().isEmpty());
        QVERIFY(model.selectedEpisodeTurns().isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
    }

    /// selectEpisode() fetches turns and sets selectedEpisodeId.
    void selectEpisodeSuccess() {
        MockNam nam;
        nam.enqueue(makeTurns());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QSignalSpy turnsSpy(&model, &MemoryBrowserModel::selectedEpisodeTurnsChanged);
        QSignalSpy idSpy(&model, &MemoryBrowserModel::selectedEpisodeIdChanged);

        model.selectEpisode(QStringLiteral("ep1"));
        QCOMPARE(model.selectedEpisodeId(), QStringLiteral("ep1"));
        QCOMPARE(model.loading(), true);

        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QCOMPARE(model.selectedEpisodeTurns().size(), 2);
        QVERIFY(turnsSpy.count() >= 1);
        QVERIFY(idSpy.count() >= 1);

        QVariantMap turn = model.selectedEpisodeTurns().at(0).toMap();
        QCOMPARE(turn[QStringLiteral("role")].toString(), QStringLiteral("user"));
        QCOMPARE(turn[QStringLiteral("content")].toString(), QStringLiteral("Hello Jarvis"));
    }

    /// selectEpisode() with same ID still re-fetches turns.
    void selectEpisodeSameId() {
        MockNam nam;
        nam.enqueue(makeTurns());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        // Set selectedEpisodeId via property manipulation isn't possible directly,
        // so prime it via a previous selectEpisode call
        model.selectEpisode(QStringLiteral("ep1"));
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);

        // Now select same episode again — should re-fetch (increments pending)
        nam.enqueue(makeTurns());
        QSignalSpy turnsSpy(&model, &MemoryBrowserModel::selectedEpisodeTurnsChanged);
        model.selectEpisode(QStringLiteral("ep1")); // same ID — no idChanged signal
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QVERIFY(turnsSpy.count() >= 1);
    }

    /// clearSelection() clears ID and turns.
    void clearSelection() {
        MockNam nam;
        nam.enqueue(makeTurns());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.selectEpisode(QStringLiteral("ep1"));
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QCOMPARE(model.selectedEpisodeTurns().size(), 2);

        QSignalSpy idSpy(&model, &MemoryBrowserModel::selectedEpisodeIdChanged);
        QSignalSpy turnsSpy(&model, &MemoryBrowserModel::selectedEpisodeTurnsChanged);

        model.clearSelection();
        QVERIFY(model.selectedEpisodeId().isEmpty());
        QVERIFY(model.selectedEpisodeTurns().isEmpty());
        QVERIFY(idSpy.count() >= 1);
        QVERIFY(turnsSpy.count() >= 1);
    }

    /// clearSelection() when already empty emits no signals.
    void clearSelectionWhenEmpty() {
        MockNam nam;
        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QSignalSpy idSpy(&model, &MemoryBrowserModel::selectedEpisodeIdChanged);
        QSignalSpy turnsSpy(&model, &MemoryBrowserModel::selectedEpisodeTurnsChanged);

        model.clearSelection();
        QCOMPARE(idSpy.count(), 0);
        QCOMPARE(turnsSpy.count(), 0);
    }

    /// fetchError sets errorMessage and loading returns to false.
    void fetchErrorSetsMessage() {
        MockNam nam;
        nam.enqueue(QByteArray{}, QNetworkReply::ConnectionRefusedError); // episodes
        nam.enqueue(QByteArray{}, QNetworkReply::ConnectionRefusedError); // preferences

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QSignalSpy errorSpy(&model, &MemoryBrowserModel::errorMessageChanged);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QVERIFY(!model.errorMessage().isEmpty());
        QVERIFY(errorSpy.count() >= 1);
    }

    /// refresh() clears a prior error message.
    void refreshClearsPriorError() {
        MockNam nam;
        // First refresh: both fail
        nam.enqueue(QByteArray{}, QNetworkReply::ConnectionRefusedError);
        nam.enqueue(QByteArray{}, QNetworkReply::ConnectionRefusedError);

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        QVERIFY(!model.errorMessage().isEmpty());

        // Second refresh: both succeed — error should be cleared
        nam.enqueue(makeEpisodes());
        nam.enqueue(makePreferences());

        model.refresh();
        QVERIFY(model.errorMessage().isEmpty()); // cleared immediately on refresh()
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
    }

    /// hasMemory transitions from false to true when episodes arrive.
    void hasMemoryTransition() {
        MockNam nam;
        nam.enqueue(makeEpisodes(1));
        nam.enqueue(makePreferences());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        QCOMPARE(model.hasMemory(), false);

        QSignalSpy spy(&model, &MemoryBrowserModel::hasMemoryChanged);
        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.hasMemory(), true, 2000);
        QVERIFY(spy.count() >= 1);
    }

    /// hasMemory stays true on a second refresh (no duplicate hasMemoryChanged).
    void hasMemoryNoSpuriousChange() {
        MockNam nam;
        nam.enqueue(makeEpisodes(2));
        nam.enqueue(makePreferences());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.hasMemory(), true, 2000);

        nam.enqueue(makeEpisodes(2));
        nam.enqueue(makePreferences());
        QSignalSpy spy(&model, &MemoryBrowserModel::hasMemoryChanged);
        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);
        // hasMemory stays true — should not emit again
        QCOMPARE(spy.count(), 0);
    }

    /// Timestamp formatting: valid ISO 8601 string is reformatted to locale display.
    void timestampFormatted() {
        MockNam nam;
        QJsonArray arr;
        arr.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("ep1")},
            {QStringLiteral("title"), QStringLiteral("T")},
            {QStringLiteral("startTime"), QStringLiteral("2024-06-15T14:30:00Z")},
            {QStringLiteral("turnCount"), 0}
        });
        nam.enqueue(QJsonDocument(arr).toJson());
        nam.enqueue(QJsonDocument(QJsonArray{}).toJson());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);

        QString ts = model.episodes().at(0).toMap()[QStringLiteral("startTime")].toString();
        // Must not be the raw ISO string — some locale-formatted content expected
        QVERIFY(ts != QStringLiteral("2024-06-15T14:30:00Z"));
        QVERIFY(!ts.isEmpty());
    }

    /// Timestamp formatting: unparseable string passes through unchanged.
    void timestampInvalidPassthrough() {
        MockNam nam;
        QJsonArray arr;
        arr.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("ep1")},
            {QStringLiteral("title"), QStringLiteral("T")},
            {QStringLiteral("startTime"), QStringLiteral("not-a-date")},
            {QStringLiteral("turnCount"), 0}
        });
        nam.enqueue(QJsonDocument(arr).toJson());
        nam.enqueue(QJsonDocument(QJsonArray{}).toJson());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);

        QString ts = model.episodes().at(0).toMap()[QStringLiteral("startTime")].toString();
        QCOMPARE(ts, QStringLiteral("not-a-date"));
    }

    /// Timestamp formatting: empty string passes through unchanged.
    void timestampEmptyPassthrough() {
        MockNam nam;
        QJsonArray arr;
        arr.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("ep1")},
            {QStringLiteral("title"), QStringLiteral("T")},
            {QStringLiteral("startTime"), QStringLiteral("")},
            {QStringLiteral("turnCount"), 0}
        });
        nam.enqueue(QJsonDocument(arr).toJson());
        nam.enqueue(QJsonDocument(QJsonArray{}).toJson());

        MemoryClient client(&nam, QStringLiteral("http://localhost:18789/v1"));
        MemoryBrowserModel model(&client);

        model.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(model.loading(), false, 2000);

        QString ts = model.episodes().at(0).toMap()[QStringLiteral("startTime")].toString();
        QCOMPARE(ts, QStringLiteral(""));
    }
};

QTEST_GUILESS_MAIN(TestMemoryBrowserModel)
#include "tst_memory_browser_model.moc"
