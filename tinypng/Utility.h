#pragma once
#include <qobject.h>
#include <QByteArray>
#include <QStringList>
#include "Config.h"
class Utility :
	public QObject
{
public:
	//验证文件后缀是否符合压缩格式
	static bool checkFileFormat(const QString&);
	static QStringList getFormatList();
    static QString getFileDialogFilter();
    static bool usesWebsite(const QString& path, TinyReqMode mode);
    static QString imageMimeType(const QString& path);
    static constexpr qint64 webMaxFileSize() { return 5 * 1024 * 1024; }
    static QString webSizeError(qint64 bytes);
	static bool optimizeSvg(const QByteArray& input, QByteArray& output, QString& error);
};
