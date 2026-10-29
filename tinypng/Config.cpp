#include "Config.h"
#include <QCryptographicHash>

Config::Config(QObject* parent) : QObject(parent) {
    qset = new QSettings("lane", "tinypng", this);
    qset->beginGroup("Property");
    if (!qset->contains("credentialsMigrated")) {
        const auto digest = [](const QString& value) { return QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex(); };
        const QList<QByteArray> oldMails = {"0c4254c55dbf9ccc4314dc21681ad63b7909e467ab16fc765212b2733904d7e4","5654444bb131d87766ef0bb4e15901f1c1193f095989bdf67ce7dc144174dc78"};
        const QList<QByteArray> oldKeys = {"428e23a3f8717b8146c5070f5cbd47e45dc4f0bd94c00ab662d8121fd0d66732","d2f8d614392e71f5e8a0e5ff7d6d3de2a2fadf5d0f5eeccb42c7e9c6a5aee098"};
        if (oldMails.contains(digest(qset->value("mail").toString()))) qset->remove("mail");
        if (oldKeys.contains(digest(qset->value("key").toString()))) qset->remove("key");
        qset->setValue("credentialsMigrated", true);
    }
    qset->endGroup();
}
void Config::set(configItem item) {
    qset->beginGroup("Property");
    qset->setValue("mail", item.mail);
    qset->setValue("key", item.key);
    qset->setValue("proxy", item.proxy);
    qset->setValue("outputMode", int(item.outputMode));
    qset->setValue("minsize", item.minsize);
    qset->setValue("maxsize", item.maxsize);
    qset->setValue("tinyReqMode", int(item.tinyReqMode));
    qset->setValue("autostart", item.autoStart);
    qset->setValue("theme", int(item.themeMode));
    qset->endGroup();
}
configItem Config::get() {
    configItem item;
    qset->beginGroup("Property");
    item.mail = qset->value("mail").toString();
    item.key = qset->value("key").toString();
    item.proxy = qset->value("proxy").toString();
    item.minsize = qset->value("minsize", 0).toInt();
    item.maxsize = qset->value("maxsize", 5 * 1024 * 1024).toInt();
    if (item.maxsize <= 0) item.maxsize = 5 * 1024 * 1024;
    item.outputMode = qset->value("outputMode", int(OutputMode::Replace)).toInt() == 1 ? OutputMode::Replace : OutputMode::NewDir;
    item.tinyReqMode = qset->value("tinyReqMode", 0).toInt() == 1 ? TinyReqMode::Key : TinyReqMode::Web;
    item.autoStart = qset->value("autostart", true).toBool();
    const int theme = qset->value("theme", int(ThemeMode::System)).toInt();
    item.themeMode = theme >= 0 && theme <= 2 ? ThemeMode(theme) : ThemeMode::System;
    qset->endGroup();
    return item;
}
Config::~Config() {}
