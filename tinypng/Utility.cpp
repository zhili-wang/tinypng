#include "Utility.h"
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>


bool Utility::checkFileFormat(const QString& name) {
	QString suffix = QFileInfo(name).suffix().toLower();
	if (Utility::getFormatList().indexOf("*." + suffix) != -1) {
		return true;
	}
	else {
		return false;
	}
}


QStringList Utility::getFormatList() {
	QStringList suffiexs = QStringList();

	suffiexs << "*.jpg";
	suffiexs << "*.png";
	suffiexs << "*.jpeg";
	suffiexs << "*.webp";
	suffiexs << "*.svg";
	return suffiexs;
}

QString Utility::getFileDialogFilter() {
    QStringList patterns = getFormatList();
    // Native dialogs can match filters case-sensitively on some platforms.
    for (const QString& pattern : getFormatList()) patterns.append(pattern.toUpper());
    return "图片 (" + patterns.join(' ') + ");;所有文件 (*)";
}

bool Utility::optimizeSvg(const QByteArray& input, QByteArray& output, QString& error) {
	// Compact XML without changing paths, precision, text, styles or namespaces.
	QXmlStreamReader reader(input);
	QByteArray compact;
	QXmlStreamWriter writer(&compact);
	writer.setAutoFormatting(false);
	bool hasSvgRoot = false;
	bool preserveDocument = false;
	int depth = 0;
	QList<bool> preserveSpace;
	while (!reader.atEnd()) {
		reader.readNext();
        if (reader.hasError()) break;
		if (reader.isStartElement()) {
			if (depth == 0) {
				hasSvgRoot = reader.name() == QStringLiteral("svg") &&
					(reader.namespaceUri().isEmpty() || reader.namespaceUri() == QStringLiteral("http://www.w3.org/2000/svg"));
			}
			const QString name = reader.name().toString();
			// Scripts, CSS selectors and embedded HTML can depend on DOM whitespace.
			if (name == "script" || name == "style" || name == "foreignObject") preserveDocument = true;
			const QString space = reader.attributes().value(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("space")).toString();
			bool preserve = !preserveSpace.isEmpty() && preserveSpace.last();
			if (space == "preserve") preserve = true;
			// Preserve whitespace throughout SVG text subtrees, including between tspans.
			if (name == "text" || name == "tspan" || name == "textPath" || name == "title" || name == "desc" || name == "metadata") preserve = true;
			if (!reader.namespaceUri().isEmpty() && reader.namespaceUri() != QStringLiteral("http://www.w3.org/2000/svg")) preserve = true;
			preserveSpace.append(preserve);
			++depth;
		}
		if (reader.tokenType() == QXmlStreamReader::DTD || reader.isProcessingInstruction() || reader.isEntityReference()) {
			// Preserve entities and processing instructions byte-for-byte.
			preserveDocument = true;
		}
		if (reader.isStartDocument() || reader.isEndDocument()) continue;
		if (reader.isComment()) continue;
		if (reader.isCharacters() && reader.isWhitespace() && (preserveSpace.isEmpty() || !preserveSpace.last())) continue;
		if (reader.isStartElement()) {
			writer.writeStartElement(reader.qualifiedName().toString());
			for (const auto& ns : reader.namespaceDeclarations()) {
				if (ns.prefix().isEmpty()) writer.writeDefaultNamespace(ns.namespaceUri().toString());
				else writer.writeNamespace(ns.namespaceUri().toString(), ns.prefix().toString());
			}
			for (const auto& attribute : reader.attributes()) {
				writer.writeAttribute(attribute.qualifiedName().toString(), attribute.value().toString());
			}
		} else if (reader.isEndElement()) {
			writer.writeEndElement();
		} else if (reader.isCharacters()) {
			if (reader.isCDATA()) writer.writeCDATA(reader.text().toString());
			else writer.writeCharacters(reader.text().toString());
		}
		if (reader.isEndElement()) {
			preserveSpace.removeLast();
			--depth;
		}
	}
	if (reader.hasError() || !hasSvgRoot || writer.hasError()) {
		error = reader.hasError() ? QString("SVG 格式错误（第 %1 行）：%2").arg(reader.lineNumber()).arg(reader.errorString()) : QStringLiteral("文件不是有效的 SVG 图片");
		return false;
	}
	// Never enlarge a file or alter documents whose DOM may be inspected by scripts.
	output = !preserveDocument && compact.size() < input.size() ? compact : input;
	error.clear();
	return true;
}
