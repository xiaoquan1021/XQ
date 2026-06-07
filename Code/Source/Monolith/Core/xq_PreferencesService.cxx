#include "xq_PreferencesService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace xq::core
{

namespace
{

constexpr const char* kSchemaVersion = "1.0";

QString VariantTypeName(const QVariant& value)
{
    switch (value.typeId())
    {
    case QMetaType::QString:
        return QStringLiteral("string");
    case QMetaType::Bool:
        return QStringLiteral("bool");
    case QMetaType::Int:
        return QStringLiteral("int");
    default:
        return QString();
    }
}

QJsonObject VariantToJson(const QVariant& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), VariantTypeName(value));

    switch (value.typeId())
    {
    case QMetaType::QString:
        object.insert(QStringLiteral("value"), value.toString());
        break;
    case QMetaType::Bool:
        object.insert(QStringLiteral("value"), value.toBool());
        break;
    case QMetaType::Int:
        object.insert(QStringLiteral("value"), value.toInt());
        break;
    default:
        break;
    }

    return object;
}

} // namespace

PreferencesService::PreferencesService(QObject* parent)
    : QObject(parent)
{
}

QStringList PreferencesService::Keys() const
{
    return m_Values.keys();
}

QString PreferencesService::StringValue(const QString& key,
                                        const QString& fallback) const
{
    const auto it = m_Values.find(NormalizedKey(key));
    if (it == m_Values.end())
        return fallback;

    return it.value().toString();
}

bool PreferencesService::BoolValue(const QString& key, bool fallback) const
{
    const auto it = m_Values.find(NormalizedKey(key));
    if (it == m_Values.end())
        return fallback;

    return it.value().toBool();
}

int PreferencesService::IntValue(const QString& key, int fallback) const
{
    const auto it = m_Values.find(NormalizedKey(key));
    if (it == m_Values.end())
        return fallback;

    return it.value().toInt();
}

void PreferencesService::SetStringValue(const QString& key,
                                        const QString& value)
{
    const QString normalizedKey = NormalizedKey(key);
    if (!normalizedKey.isEmpty())
        m_Values.insert(normalizedKey, value);
}

void PreferencesService::SetBoolValue(const QString& key, bool value)
{
    const QString normalizedKey = NormalizedKey(key);
    if (!normalizedKey.isEmpty())
        m_Values.insert(normalizedKey, value);
}

void PreferencesService::SetIntValue(const QString& key, int value)
{
    const QString normalizedKey = NormalizedKey(key);
    if (!normalizedKey.isEmpty())
        m_Values.insert(normalizedKey, value);
}

bool PreferencesService::Save(const QString& filePath,
                              QString* errorMessage) const
{
    const QFileInfo fileInfo(filePath);
    QDir parentDirectory = fileInfo.dir();
    if (!parentDirectory.exists() &&
        !QDir().mkpath(parentDirectory.absolutePath()))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to create preferences directory."));
        return false;
    }

    QJsonObject valuesObject;
    for (auto it = m_Values.constBegin(); it != m_Values.constEnd(); ++it)
    {
        const QString typeName = VariantTypeName(it.value());
        if (typeName.isEmpty())
        {
            SetError(errorMessage,
                     QStringLiteral("Unsupported preference value type."));
            return false;
        }

        valuesObject.insert(it.key(), VariantToJson(it.value()));
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"),
                QString::fromLatin1(kSchemaVersion));
    root.insert(QStringLiteral("values"), valuesObject);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to write preferences file."));
        return false;
    }

    const QJsonDocument document(root);
    file.write(document.toJson(QJsonDocument::Indented));
    SetError(errorMessage, QString());
    return true;
}

bool PreferencesService::Load(const QString& filePath,
                              QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to read preferences file."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        SetError(errorMessage,
                 QStringLiteral("Preferences file is not valid JSON."));
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toString() !=
        QString::fromLatin1(kSchemaVersion))
    {
        SetError(errorMessage,
                 QStringLiteral("Unsupported preferences schema version."));
        return false;
    }

    const QJsonValue valuesValue = root.value(QStringLiteral("values"));
    if (!valuesValue.isObject())
    {
        SetError(errorMessage,
                 QStringLiteral("Preferences values object is missing."));
        return false;
    }

    QMap<QString, QVariant> loadedValues;
    const QJsonObject valuesObject = valuesValue.toObject();
    for (auto it = valuesObject.constBegin(); it != valuesObject.constEnd(); ++it)
    {
        const QString key = NormalizedKey(it.key());
        if (key.isEmpty() || !it.value().isObject())
        {
            SetError(errorMessage,
                     QStringLiteral("Invalid preference entry."));
            return false;
        }

        const QJsonObject valueObject = it.value().toObject();
        const QString type = valueObject.value(QStringLiteral("type")).toString();
        const QJsonValue value = valueObject.value(QStringLiteral("value"));

        if (type == QStringLiteral("string") && value.isString())
            loadedValues.insert(key, value.toString());
        else if (type == QStringLiteral("bool") && value.isBool())
            loadedValues.insert(key, value.toBool());
        else if (type == QStringLiteral("int") && value.isDouble())
            loadedValues.insert(key, value.toInt());
        else
        {
            SetError(errorMessage,
                     QStringLiteral("Unsupported preference entry type."));
            return false;
        }
    }

    m_Values = loadedValues;
    SetError(errorMessage, QString());
    return true;
}

void PreferencesService::SetError(QString* errorMessage,
                                  const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

QString PreferencesService::NormalizedKey(const QString& key)
{
    return key.trimmed();
}

} // namespace xq::core
