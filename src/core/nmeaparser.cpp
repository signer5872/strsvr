#include "nmeaparser.h"

#include <cmath>
#include <cctype>

NmeaParser::SentenceType NmeaParser::parseSentenceType(const QString &sentence)
{
    if (sentence.isEmpty() || sentence[0] != '$') return Unknown;

    // Extract talker + sentence ID (e.g., "$GPGGA" -> "GPGGA")
    QString type = sentence.mid(1, sentence.indexOf(',') - 1);
    if (type.length() < 4) return Unknown;

    QString id = type.right(3);
    QString upper = id.toUpper();

    if (sentence.startsWith("$P") || sentence.startsWith("$p")) {
        return PType;
    }

    if (upper == "GGA") return GGA;
    if (upper == "GSA") return GSA;
    if (upper == "GSV") return GSV;
    if (upper == "RMC") return RMC;
    if (upper == "ZDA") return ZDA;
    if (upper == "GLL") return GLL;
    if (upper == "VTG") return VTG;
    if (upper == "GST") return GST;
    if (upper == "GRS") return GRS;
    if (upper == "GBS") return GBS;

    return Unknown;
}

QString NmeaParser::computeChecksum(const QString &sentence)
{
    // Find content between '$' and '*' (or end of sentence)
    int start = 1; // skip '$'
    int end = sentence.indexOf('*');
    QString content = (end > 0) ? sentence.mid(start, end - start) : sentence.mid(start);

    unsigned char cksum = 0;
    for (int i = 0; i < content.length(); ++i) {
        cksum ^= static_cast<unsigned char>(content[i].toLatin1());
    }

    return QString("%1").arg(cksum, 2, 16, QLatin1Char('0')).toUpper();
}

bool NmeaParser::isValidChecksum(const QString &sentence)
{
    int starPos = sentence.indexOf('*');
    if (starPos < 0) return false;

    QString expected = computeChecksum(sentence);
    QString actual = sentence.mid(starPos + 1, 2);

    return expected.compare(actual, Qt::CaseInsensitive) == 0;
}

QStringList NmeaParser::splitSentence(const QString &sentence)
{
    // Strip '$' and checksum
    QString stripped = sentence.mid(1);
    int starPos = stripped.indexOf('*');
    if (starPos >= 0) {
        stripped = stripped.left(starPos);
    }
    // Qt 5.14 起 KeepEmptyParts 移入 Qt 命名空间，这里兼容旧版 Qt
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return stripped.split(',', Qt::KeepEmptyParts);
#else
    return stripped.split(',', QString::KeepEmptyParts);
#endif
}

// ===== GGA Parser =====
bool NmeaParser::parseGGA(const QString &sentence, GgaData &out)
{
    QStringList parts = splitSentence(sentence);
    if (parts.size() < 15) return false;

    out.time = parseNmeaTime(parts[1]);
    out.latitude = nmeaToDecimal(parts[2], parts[3]);
    out.longitude = nmeaToDecimal(parts[4], parts[5]);

    int fq = parts[6].toInt();
    out.fixQuality = static_cast<FixQuality>(fq);

    out.satellitesUsed = parts[7].toInt();
    out.hdop = parts[8].toDouble();
    out.altitudeMsl = parts[9].toDouble();
    // parts[10] = altitude unit (M)
    out.geoidSeparation = parts[11].toDouble();
    // parts[12] = geoid unit (M)
    out.dgpsAge = parts[13].isEmpty() ? -1 : parts[13].toDouble();
    out.dgpsStationId = parts[14].isEmpty() ? -1 : parts[14].toInt();

    return true;
}

// ===== GSA Parser =====
bool NmeaParser::parseGSA(const QString &sentence, GsaData &out)
{
    QStringList parts = splitSentence(sentence);
    if (parts.size() < 17) return false;

    out.mode = parts[1];
    int fq = parts[2].toInt();
    out.fixQuality = static_cast<FixQuality>(fq);

    out.prns.clear();
    for (int i = 3; i < 15; ++i) {
        if (!parts[i].isEmpty()) {
            out.prns.append(parts[i].toInt());
        }
    }

    out.pdop = parts[15].toDouble();
    out.hdop = parts[16].toDouble();
    if (parts.size() > 17) {
        out.vdop = parts[17].toDouble();
    }

    return true;
}

// ===== GSV Parser =====
bool NmeaParser::parseGSV(const QString &sentence, GsvData &out)
{
    QStringList parts = splitSentence(sentence);
    if (parts.size() < 7) return false;

    out.totalSentences = parts[1].toInt();
    out.sentenceNumber = parts[2].toInt();
    out.totalSatellites = parts[3].toInt();

    out.satellites.clear();
    int satStartIndex = 4;
    int numSatFields = (parts.size() - 4) / 4; // 4 fields per satellite

    for (int i = 0; i < numSatFields && satStartIndex + i * 4 + 3 < parts.size(); ++i) {
        SatelliteInfo sat;
        sat.prn = parts[satStartIndex + i * 4].toInt();
        sat.elevation = parts[satStartIndex + i * 4 + 1].toInt();
        sat.azimuth = parts[satStartIndex + i * 4 + 2].toInt();
        sat.snr = parts[satStartIndex + i * 4 + 3].toInt();
        out.satellites.append(sat);
    }

    return true;
}

// ===== RMC Parser =====
bool NmeaParser::parseRMC(const QString &sentence, RmcData &out)
{
    QStringList parts = splitSentence(sentence);
    if (parts.size() < 12) return false;

    out.time = parseNmeaTime(parts[1]);
    out.valid = (parts[2] == "A");
    out.latitude = nmeaToDecimal(parts[3], parts[4]);
    out.longitude = nmeaToDecimal(parts[5], parts[6]);
    out.speedKnots = parts[7].toDouble();
    out.course = parts[8].toDouble();

    if (parts.size() > 9 && parts[9].length() >= 6) {
        QDate date;
        if (parts[9].length() == 6) {
            date = QDate(
                2000 + parts[9].mid(4, 2).toInt(),
                parts[9].mid(2, 2).toInt(),
                parts[9].mid(0, 2).toInt()
            );
        }
        out.time = QDateTime(date, out.time.time());
    }

    if (parts.size() > 10 && !parts[10].isEmpty()) {
        out.variation = parts[10].toDouble();
        out.variationEast = (parts.size() > 11 && parts[11] == "E");
    }

    if (parts.size() > 12) {
        out.navigationStatus = parts[12];
    }

    return true;
}

// ===== ZDA Parser =====
bool NmeaParser::parseZDA(const QString &sentence, ZdaData &out)
{
    QStringList parts = splitSentence(sentence);
    if (parts.size() < 6) return false;

    out.utcTime = parseNmeaTime(parts[1]);
    if (parts.size() > 6) {
        QDate date(
            parts[5].toInt(),
            parts[4].toInt(),
            parts[3].toInt()
        );
        out.utcTime.setDate(date);
    }

    if (parts.size() > 7) {
        out.localHours = parts[6].toInt();
        out.localMinutes = parts[7].toInt();
    }

    return true;
}

// ===== Generation Functions =====

QString NmeaParser::makeGGA(const GgaData &data, const QString &talker)
{
    QString timeStr = data.time.time().toString("hhmmss.zzz");
    QString lat = decimalToNmea(data.latitude, data.latitude >= 0 ? 'N' : 'S');
    QString lon = decimalToNmea(data.longitude, data.longitude >= 0 ? 'E' : 'W');

    QStringList parts;
    parts << QString("$%1GGA").arg(talker);
    parts << timeStr;
    parts << lat;
    parts << lon;
    parts << QString::number(static_cast<int>(data.fixQuality));
    parts << QString::number(data.satellitesUsed);
    parts << QString::number(data.hdop, 'f', 1);
    parts << QString::number(data.altitudeMsl, 'f', 3);
    parts << "M";
    parts << QString::number(data.geoidSeparation, 'f', 1);
    parts << "M";

    QString body = parts.join(',');
    QString cksum = computeChecksum(body);
    return body + "*" + cksum;
}

QString NmeaParser::makeGSA(const GsaData &data, const QString &talker)
{
    QStringList parts;
    parts << QString("$%1GSA").arg(talker);
    parts << (data.mode.isEmpty() ? "A" : data.mode);
    parts << QString::number(static_cast<int>(data.fixQuality));

    for (int i = 0; i < 12; ++i) {
        if (i < data.prns.size()) {
            parts << QString::number(data.prns[i]);
        } else {
            parts << "";
        }
    }

    parts << QString::number(data.pdop, 'f', 1);
    parts << QString::number(data.hdop, 'f', 1);
    parts << QString::number(data.vdop, 'f', 1);

    QString body = parts.join(',');
    return body + "*" + computeChecksum(body);
}

QString NmeaParser::makeGSV(const GsvData &data, const QString &talker)
{
    QStringList parts;
    parts << QString("$%1GSV").arg(talker);
    parts << QString::number(data.totalSentences);
    parts << QString::number(data.sentenceNumber);
    parts << QString::number(data.totalSatellites);

    for (const auto &sat : data.satellites) {
        parts << QString::number(sat.prn);
        parts << QString::number(sat.elevation);
        parts << QString::number(sat.azimuth);
        parts << QString::number(sat.snr);
    }

    QString body = parts.join(',');
    return body + "*" + computeChecksum(body);
}

QString NmeaParser::makeRMC(const RmcData &data, const QString &talker)
{
    QString timeStr = data.time.time().toString("hhmmss.zzz");
    QString lat = decimalToNmea(data.latitude, data.latitude >= 0 ? 'N' : 'S');
    QString lon = decimalToNmea(data.longitude, data.longitude >= 0 ? 'E' : 'W');
    QString dateStr = data.time.date().toString("ddMMyy");

    QStringList parts;
    parts << QString("$%1RMC").arg(talker);
    parts << timeStr;
    parts << (data.valid ? "A" : "V");
    parts << lat;
    parts << lon;
    parts << QString::number(data.speedKnots, 'f', 2);
    parts << QString::number(data.course, 'f', 2);
    parts << dateStr;

    QString body = parts.join(',');
    return body + "*" + computeChecksum(body);
}

QString NmeaParser::makeZDA(const ZdaData &data)
{
    QString timeStr = data.utcTime.time().toString("hhmmss.zzz");

    QStringList parts;
    parts << "$GPZDA";
    parts << timeStr;
    parts << QString::number(data.utcTime.date().day());
    parts << QString::number(data.utcTime.date().month());
    parts << QString::number(data.utcTime.date().year());
    parts << QString("%1").arg(data.localHours, 2, 10, QLatin1Char('0'));
    parts << QString("%1").arg(data.localMinutes, 2, 10, QLatin1Char('0'));

    QString body = parts.join(',');
    return body + "*" + computeChecksum(body);
}

// ===== Utility Functions =====

double NmeaParser::nmeaToDecimal(const QString &coord, const QString &hemisphere)
{
    if (coord.isEmpty()) return 0.0;

    // NMEA format: ddmm.mmmmmm (latitude) or dddmm.mmmmmm (longitude)
    bool ok = false;
    const double value = coord.toDouble(&ok);
    if (!ok) return 0.0;

    const QString hemi = hemisphere.toUpper();
    const bool negative = (hemi == "S" || hemi == "W");

    const int dotPos = coord.indexOf('.');
    const int intDigits = (dotPos >= 0) ? dotPos : coord.length();

    // 整数位只有 1~2 位时说明已经是十进制度，直接使用
    if (intDigits <= 2) {
        return negative ? -value : value;
    }

    // 小数点前的最后两位是“分”，其余是“度”：3027.5094 -> 30°27.5094′
    const int degDigits = intDigits - 2;
    const double degrees = coord.left(degDigits).toDouble(&ok);
    if (!ok) return 0.0;
    const double minutes = coord.mid(degDigits).toDouble(&ok);
    if (!ok) return 0.0;

    const double decimal = degrees + minutes / 60.0;
    return negative ? -decimal : decimal;
}

QString NmeaParser::decimalToNmea(double value, char dir)
{
    int sign = (dir == 'S' || dir == 'W') ? -1 : 1;
    double absVal = std::abs(value);

    int degrees = static_cast<int>(std::floor(absVal));
    double minutes = (absVal - degrees) * 60.0;

    int degreeWidth = (dir == 'E' || dir == 'W') ? 3 : 2;

    QString result = QString("%1%2").arg(degrees, degreeWidth, 10, QLatin1Char('0')).arg(minutes, 0, 'f', 5);
    return result;
}

QList<QString> NmeaParser::extractSentences(const QByteArray &data)
{
    QList<QString> sentences;
    QString buffer = QString::fromLatin1(data);

    int dollarIndex = 0;
    while ((dollarIndex = buffer.indexOf('$', dollarIndex)) >= 0) {
        int endIndex = buffer.indexOf('\n', dollarIndex);
        if (endIndex < 0) break;

        QString sentence = buffer.mid(dollarIndex, endIndex - dollarIndex).trimmed();

        // Validate basic structure
        if (sentence.length() >= 6 && sentence.startsWith('$')) {
            sentences.append(sentence);
        }

        dollarIndex = endIndex + 1;
    }

    return sentences;
}

QString NmeaParser::fixQualityToString(FixQuality q)
{
    switch (q) {
    case Invalid:      return "Invalid";
    case GPSFix:       return "GPS Fix";
    case DGPSFix:      return "DGPS Fix";
    case PPSFix:       return "PPS Fix";
    case RTKInteger:   return "RTK Integer";
    case RTKFloat:     return "RTK Float";
    case Estimated:    return "Estimated";
    case ManualInput:  return "Manual Input";
    case Simulator:    return "Simulator";
    }
    return "Unknown";
}

QDateTime NmeaParser::parseNmeaTime(const QString &timeStr)
{
    if (timeStr.length() < 6) return QDateTime();

    int hh = timeStr.mid(0, 2).toInt();
    int mm = timeStr.mid(2, 2).toInt();
    double ss = timeStr.mid(4).toDouble();

    QTime t(hh, mm, static_cast<int>(ss), static_cast<int>((ss - static_cast<int>(ss)) * 1000));
    return QDateTime(QDate(1970, 1, 1), t, Qt::UTC);
}

QDateTime NmeaParser::parseNmeaDate(const QString &dateStr, const QDateTime &time)
{
    if (dateStr.length() != 6) return time;

    int dd = dateStr.mid(0, 2).toInt();
    int mm = dateStr.mid(2, 2).toInt();
    int yy = dateStr.mid(4, 2).toInt();
    int year = (yy < 70) ? 2000 + yy : 1900 + yy;

    return QDateTime(QDate(year, mm, dd), time.time(), Qt::UTC);
}
