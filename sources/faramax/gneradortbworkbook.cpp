// SPDX-License-Identifier: GPL-2.0-or-later
#include "../container/qetzip.h"
#include "gneradortb.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Faramax::TB
{
namespace
{
const QStringList fields{"posicion",     "tipo_borna", "puente",      "disp_ext", "mang_ext",     "cable_ext",
                         "bornero",      "num_borna",  "cable_int",   "mang_int", "disp_int",     "corte",
                         "modelo",       "reserva",    "nivel",       "shl_ext",  "mang_shl_ext", "shl_int",
                         "mang_shl_int", "peine",      "peine_grupo", "color"};
const QStringList titles{QString::fromUtf8("Posición"),
                         "Tipo Borna",
                         "Puente",
                         "Dispositivo Externo",
                         "Manguera Externa",
                         "Cable Externo",
                         "Bornero",
                         QString::fromUtf8("Nº de Borna"),
                         "Cable Interno",
                         "Manguera Interna",
                         "Dispositivo Interno",
                         "Corte",
                         "Modelo Borna",
                         "Reserva",
                         "Nivel",
                         "SHL Ext",
                         "Rango SHL Ext",
                         "SHL Int",
                         "Rango SHL Int",
                         "Peine",
                         "Grupo Peine",
                         "Color"};
QString column(int n)
{
    QString result;
    do
    {
        result.prepend(QChar('A' + n % 26));
        n = n / 26 - 1;
    } while (n >= 0);
    return result;
}
QString cellText(const QJsonValue &v)
{
    return v.isBool()     ? (v.toBool() ? "1" : "0")
           : v.isDouble() ? QString::number(v.toDouble())
                          : v.toString();
}
QByteArray sheet(const QVector<QStringList> &rows)
{
    QByteArray output;
    QXmlStreamWriter w(&output);
    w.writeStartDocument();
    w.writeStartElement("worksheet");
    w.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    w.writeStartElement("sheetData");
    for (int r = 0; r < rows.size(); ++r)
    {
        w.writeStartElement("row");
        w.writeAttribute("r", QString::number(r + 1));
        for (int c = 0; c < rows[r].size(); ++c)
        {
            w.writeStartElement("c");
            w.writeAttribute("r", column(c) + QString::number(r + 1));
            w.writeAttribute("t", "inlineStr");
            w.writeStartElement("is");
            w.writeTextElement("t", rows[r][c]);
            w.writeEndElement();
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return output;
}
QVector<QStringList> parseSheet(const QByteArray &xml, const QStringList &shared, QString &error)
{
    QVector<QStringList> result;
    QXmlStreamReader reader(xml);
    QStringList row;
    QString type, text;
    int col = 0;
    bool inCell = false;
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement())
        {
            if (reader.name() == QLatin1String("row"))
                row.clear();
            else if (reader.name() == QLatin1String("c"))
            {
                type = reader.attributes().value("t").toString();
                auto ref = reader.attributes().value("r").toString();
                col = 0;
                for (auto ch : ref)
                {
                    if (!ch.isLetter())
                        break;
                    col = col * 26 + ch.unicode() - 'A' + 1;
                }
                col = qMax(0, col - 1);
                text.clear();
                inCell = true;
            }
            else if (inCell && (reader.name() == QLatin1String("t") || reader.name() == QLatin1String("v")))
                text += reader.readElementText();
        }
        else if (reader.isEndElement())
        {
            if (reader.name() == QLatin1String("c"))
            {
                if (col > 1024)
                {
                    error = QStringLiteral("Demasiadas columnas en el historial.");
                    return {};
                }
                while (row.size() <= col)
                    row << QString();
                row[col] = type == "s" ? shared.value(text.toInt()) : text;
                inCell = false;
            }
            else if (reader.name() == QLatin1String("row"))
            {
                result << row;
                if (result.size() > 100000)
                {
                    error = QStringLiteral("Historial demasiado grande.");
                    return {};
                }
            }
        }
    }
    if (reader.hasError())
        error = reader.errorString();
    return result;
}
} // namespace
bool writeWorkbook(const QString &path, const QVector<Row> &data, const QJsonObject &cfg,
                   const QString &project, QString &error)
{
    error.clear();
    QVector<QStringList> table{titles};
    table[0] << "QET UUID" << "QET Manual";
    for (const auto &r : data)
    {
        QStringList line;
        for (const auto &f : fields)
            line << (f == "tipo_borna" ? displayType(r.values.value(f).toString())
                                       : cellText(r.values.value(f)));
        QJsonObject manual;
        for (const auto &k : r.before.m_informations.keys())
            if (k.startsWith("faramax_tb_") && k != key("auto_reserve"))
                manual.insert(k.mid(11), r.values.value(k.mid(11)));
        for (auto it = r.edits.begin(); it != r.edits.end(); ++it)
            manual.insert(it.key(), it.value());
        line << r.id << encode(manual);
        table << line;
    }
    QVector<QStringList> meta{{"format_version", "2"},
                              {"app_version", "QET Faramax / GNerador TB"},
                              {"project_code", project},
                              {"saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                              {"row_count", QString::number(data.size())},
                              {"settings", encode(cfg)}};
    const QByteArray content =
        R"(<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)";
    const QByteArray rel =
        R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)";
    const QByteArray book =
        R"(<?xml version="1.0"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Borneros" sheetId="1" r:id="rId1"/><sheet name="QET_metadata" sheetId="2" state="hidden" r:id="rId2"/></sheets></workbook>)";
    const QByteArray links =
        R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/></Relationships>)";
    return QetZip::write(path,
                         {{"[Content_Types].xml", content},
                          {"_rels/.rels", rel},
                          {"xl/workbook.xml", book},
                          {"xl/_rels/workbook.xml.rels", links},
                          {"xl/worksheets/sheet1.xml", sheet(table)},
                          {"xl/worksheets/sheet2.xml", sheet(meta)}},
                         &error);
}
bool readWorkbook(const QString &path, QJsonArray &result, QJsonObject &cfg, QString &error)
{
    result = {};
    cfg = {};
    error.clear();
    QList<QetZip::Entry> entries;
    if (!QetZip::read(path, &entries, &error))
        return false;
    QMap<QString, QByteArray> files;
    for (const auto &e : entries)
        files[e.name] = e.data;
    QStringList shared;
    QXmlStreamReader strings(files.value("xl/sharedStrings.xml"));
    QString text;
    while (!strings.atEnd())
    {
        strings.readNext();
        if (strings.isStartElement() && strings.name() == QLatin1String("si"))
            text.clear();
        else if (strings.isStartElement() && strings.name() == QLatin1String("t"))
            text += strings.readElementText();
        else if (strings.isEndElement() && strings.name() == QLatin1String("si"))
            shared << text;
    }
    auto table = parseSheet(files.value("xl/worksheets/sheet1.xml"), shared, error);
    if (!error.isEmpty())
        return false;
    if (table.isEmpty() || table[0].mid(0, titles.size()) != titles)
    {
        error = QStringLiteral("El archivo no contiene las columnas de GNerador_TB.");
        return false;
    }
    const QSet<QString> flags{"puente", "corte", "reserva", "shl_ext", "shl_int", "peine"};
    for (int r = 1; r < table.size(); ++r)
    {
        if (table[r].join("").trimmed().isEmpty())
            continue;
        QJsonObject row;
        for (int c = 0; c < fields.size(); ++c)
        {
            auto value = table[r].value(c);
            if (flags.contains(fields[c]))
                row[fields[c]] = value == "1" || value.compare("true", Qt::CaseInsensitive) == 0 ||
                                 value.compare(QString::fromUtf8("sí"), Qt::CaseInsensitive) == 0;
            else if (fields[c] == "nivel" && table[0].value(22)!="QET UUID") {
                // v5.6 uses A/B/C... or 1/2/3..., QET uses zero-based levels.
                row["_level_name"]=value;
                if(value.size()==1&&value[0].toUpper()>=QChar('A')&&value[0].toUpper()<=QChar('D'))row["nivel"]=value[0].toUpper().unicode()-'A';
                else row["nivel"]=qMax(0,value.toInt()-1);
            }
            else if (fields[c] == "nivel" || fields[c] == "peine_grupo")
                row[fields[c]] = value.toInt();
            else
                row[fields[c]] = value;
        }
        row["tipo_borna"] = nativeType(row["tipo_borna"].toString());
        if (!table[r].value(22).isEmpty())
            row["_id"] = table[r][22];
        if (table[r].size() > 23)
            row["_manual"] = QJsonDocument::fromJson(table[r][23].toUtf8()).object();
        result.append(row);
    }
    if (files.contains("xl/worksheets/sheet2.xml"))
    {
        auto meta = parseSheet(files["xl/worksheets/sheet2.xml"], shared, error);
        for (const auto &r : meta)
            if (r.value(0) == "settings")
                cfg = QJsonDocument::fromJson(r.value(1).toUtf8()).object();
    }
    return error.isEmpty();
}
} // namespace Faramax::TB
