/**
 * NmeaParser - NMEA 0183 sentence parser and generator
 *
 * Supports: GGA, GSA, GSV, RMC, ZDA, GLL, VTG, GST, GRS, GBS
 * Both standard and proprietary ($P...) sentences.
 */

#ifndef NMEAPARSER_H
#define NMEAPARSER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDateTime>
#include <QPointF>

class NmeaParser
{
public:
    // Sentence types
    enum SentenceType {
        Unknown,
        GGA,    // Global Positioning System Fix Data
        GSA,    // GPS DOP and Active Satellites
        GSV,    // GPS Satellites in View
        RMC,    // Recommended Minimum Specific GNSS Data
        ZDA,    // Time & Date - UTC, day, month, year and local time zone
        GLL,    // Geographic Position - Latitude/Longitude
        VTG,    // Course Over Ground and Ground Speed
        GST,    // GNSS Pseudorange Error Statistics
        GRS,    // GNSS Range Residuals
        GBS,    // GNSS Satellite Fault Detection
        PType   // Proprietary sentence ($P...)
    };

    // Fix quality for GGA
    enum FixQuality {
        Invalid = 0,
        GPSFix = 1,
        DGPSFix = 2,
        PPSFix = 3,
        RTKInteger = 4,
        RTKFloat = 5,
        Estimated = 6,
        ManualInput = 7,
        Simulator = 8
    };

    // GGA data structure
    struct GgaData {
        QDateTime time;
        double latitude = 0;        // degrees, positive = N
        double longitude = 0;       // degrees, positive = E
        FixQuality fixQuality = Invalid;
        int satellitesUsed = 0;
        double hdop = 0;
        double altitudeMsl = 0;     // meters
        double geoidSeparation = 0; // meters
        double dgpsAge = -1;
        int dgpsStationId = -1;
    };

    // GSA data structure
    struct GsaData {
        QString mode;           // 'M' = Manual, 'A' = Automatic
        FixQuality fixQuality = Invalid;
        QList<int> prns;         // Up to 12 satellite PRNs
        double pdop = 0;
        double hdop = 0;
        double vdop = 0;
    };

    // Satellite info for GSV
    struct SatelliteInfo {
        int prn = 0;
        int elevation = 0;      // degrees (0-90)
        int azimuth = 0;        // degrees (0-359)
        int snr = 0;            // dB-Hz (0-99)
    };

    // GSV data structure
    struct GsvData {
        int totalSentences = 1;
        int sentenceNumber = 1;
        int totalSatellites = 0;
        QList<SatelliteInfo> satellites;
    };

    // RMC data structure
    struct RmcData {
        QDateTime time;
        bool valid = false;
        double latitude = 0;
        double longitude = 0;
        double speedKnots = 0;
        double course = 0;          // degrees true
        double variation = 0;       // magnetic variation
        bool variationEast = false;
        QString navigationStatus;   // A=Autonomous, D=Differential, etc.
    };

    // ZDA data structure
    struct ZdaData {
        QDateTime utcTime;
        int localHours = 0;
        int localMinutes = 0;
    };

    static SentenceType parseSentenceType(const QString &sentence);
    static bool isValidChecksum(const QString &sentence);
    static QString computeChecksum(const QString &sentence);

    // Parse full sentences
    static bool parseGGA(const QString &sentence, GgaData &out);
    static bool parseGSA(const QString &sentence, GsaData &out);
    static bool parseGSV(const QString &sentence, GsvData &out);
    static bool parseRMC(const QString &sentence, RmcData &out);
    static bool parseZDA(const QString &sentence, ZdaData &out);

    // Generate NMEA sentences (matching original strsvr's templates)
    static QString makeGGA(const GgaData &data, const QString &talker = "GP");
    static QString makeGSA(const GsaData &data, const QString &talker = "GP");
    static QString makeGSV(const GsvData &data, const QString &talker = "GP");
    static QString makeRMC(const RmcData &data, const QString &talker = "GP");
    static QString makeZDA(const ZdaData &data);

    // Convert lat/lon between decimal degrees and NMEA format (ddmm.mmm)
    static double nmeaToDecimal(const QString &coord, const QString &hemisphere);
    static QString decimalToNmea(double value, char dir);

    // Process raw byte stream, extract complete sentences
    static QList<QString> extractSentences(const QByteArray &data);

    // Convert fix quality number to string
    static QString fixQualityToString(FixQuality q);

private:
    static QStringList splitSentence(const QString &sentence);
    static QDateTime parseNmeaTime(const QString &timeStr);
    static QDateTime parseNmeaDate(const QString &dateStr, const QDateTime &time);
};

#endif // NMEAPARSER_H
