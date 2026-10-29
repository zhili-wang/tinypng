#pragma once

#include <QAbstractTableModel>
#include <QMutex>


typedef struct TableModelRow {
	QString root;
	QString path;
	int size;
	int thin_size; //压缩后的大小
	int status;
	bool needsCompression(bool failedOnly = false) const {
		return failedOnly ? status == -1 : status == 0 || status == -1;
	}
}_TableModelRow;

Q_DECLARE_METATYPE(TableModelRow)

class TableModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	TableModel(QObject* parent);
	~TableModel();
	virtual int rowCount(const QModelIndex & = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex & = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	void removeRow(int row, const QModelIndex& index = QModelIndex());
	void removeRange(int first, int last, const QModelIndex& index = QModelIndex());
	void removeAll();
	TableModelRow getRow(int row) const;

	void updateStatus(int row, int status);
	QMutex* mutex;

private:
	QStringList labels;
	QList<TableModelRow>* _data;

public slots:
	void addRow(QList<TableModelRow>);
	void addRow(TableModelRow);
	void replaceRow(int, const TableModelRow&);
};
