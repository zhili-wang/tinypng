#pragma once

#include <QObject>
#include <QSettings>

enum class  OutputMode {
	NewDir,
	Replace
};

enum class TinyReqMode {
	Web,  //模拟用户操作web
	Key   //使用api key 
};




enum class ThemeMode { Light, Dark, System };

struct configItem {
    ThemeMode themeMode = ThemeMode::System;
	QString mail;
	QString key;
	QString proxy;
	OutputMode outputMode;
	TinyReqMode tinyReqMode;
	bool autoStart; //添加资源后自动开始压缩
    bool autoRetry = false;
	int minsize;
	int maxsize;
};


class Config : public QObject
{
	Q_OBJECT

public:
	QSettings* qset;
	Config(QObject* parent);
	~Config();
	void set(configItem item);
	configItem get();

};
