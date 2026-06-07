#ifndef XQ_PREFERENCESSERVICE_H
#define XQ_PREFERENCESSERVICE_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace xq::core
{

class PreferencesService : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesService(QObject* parent = nullptr);

    QStringList Keys() const;

    QString StringValue(const QString& key,
                        const QString& fallback = QString()) const;
    bool BoolValue(const QString& key, bool fallback = false) const;
    int IntValue(const QString& key, int fallback = 0) const;

    void SetStringValue(const QString& key, const QString& value);
    void SetBoolValue(const QString& key, bool value);
    void SetIntValue(const QString& key, int value);

    bool Save(const QString& filePath, QString* errorMessage = nullptr) const;
    bool Load(const QString& filePath, QString* errorMessage = nullptr);

private:
    static void SetError(QString* errorMessage, const QString& message);
    static QString NormalizedKey(const QString& key);

    QMap<QString, QVariant> m_Values;
};

} // namespace xq::core

#endif // XQ_PREFERENCESSERVICE_H
