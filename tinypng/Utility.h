#pragma once
#include <qobject.h>
#include <QByteArray>
#include <QStringList>
class Utility :
	public QObject
{
public:
	//验证文件后缀是否符合压缩格式
	static bool checkFileFormat(const QString&);
	static QStringList getFormatList();
	static bool optimizeSvg(const QByteArray& input, QByteArray& output, QString& error);
};
