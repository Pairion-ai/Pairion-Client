/**
 * @file tst_constants.cpp
 * @brief Tests for application-wide constants in constants.h / version.h.
 *
 * Verifies that kClientVersion is non-empty and matches the CMake project
 * VERSION injected at build time, and that the corrected constant name
 * kWakeSuppressionMs (single 's' in "Suppression") compiles with a positive value.
 */

#include "../src/core/constants.h"

#include <QTest>

class TestConstants : public QObject {
    Q_OBJECT

  private slots:
    /// kClientVersion must be non-empty (injected by CMake configure_file).
    void clientVersionIsNonEmpty() { QVERIFY(qstrlen(pairion::kClientVersion) > 0); }

    /// kClientVersion must equal the CMake project VERSION (0.3.0).
    void clientVersionIsExpected() {
        QCOMPARE(QString::fromUtf8(pairion::kClientVersion), QStringLiteral("0.3.0"));
    }

    /// kWakeSuppressionMs (correct spelling) is positive.
    void wakeSuppressionMsIsPositive() { QVERIFY(pairion::kWakeSuppressionMs > 0); }

    /// kBargeInVadThreshold must be strictly higher than the default normal VAD threshold (0.5).
    void bargeInVadThresholdIsHigherThanDefault() {
        QVERIFY(pairion::kBargeInVadThreshold > 0.5);
        QVERIFY(pairion::kBargeInVadThreshold <= 1.0);
    }

    /// kBargeInMinDurationMs must be a positive, reasonable duration.
    void bargeInMinDurationMsIsPositive() {
        QVERIFY(pairion::kBargeInMinDurationMs > 0);
        QVERIFY(pairion::kBargeInMinDurationMs < 2000); // sanity: under 2 seconds
    }
};

QTEST_GUILESS_MAIN(TestConstants)
#include "tst_constants.moc"
