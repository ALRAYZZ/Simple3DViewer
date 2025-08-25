// Parsing .obj file into verices and indices

#include "Model.h"
#include <QFile>
#include <QTextStream>

Model::Model() {}

bool Model::loadFromFile(const QString& filePath)
{
	vertices.clear();
	indices.clear();

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return false;
	}

	QTextStream in(&file);
	std::vector<QVector3D> tempVertices;

	while (!in.atEnd())
	{
		QString line = in.readLine().trimmed();
		if (line.startsWith("v ")) // .obj way of telling us that vertice coordinates are next
		{
			QStringList parts = line.split(' ', Qt::SkipEmptyParts);
			if (parts.size() >= 4)
			{
				float x = parts[1].toFloat();
				float y = parts[2].toFloat();
				float z = parts[3].toFloat();
				tempVertices.push_back(QVector3D(x, y, z));
			}
		}
		else if (line.startsWith("f ")) // .obj format to tell us that faces indices are next
		{
			QStringList parts = line.split(' ', Qt::SkipEmptyParts);
			if (parts.size() >= 4)
			{
				for (int i = 1; i < parts.size(); ++i)
				{
					QStringList indexParts = parts[i].split('/');
					indices.push_back(indexParts[0].toUInt() - 1); // .obj indices are 1-based
				}
			}
		}
	}

	vertices = tempVertices;
	return true;
}

const std::vector<QVector3D>& Model::getVertices() const
{
	return vertices;
}
const std::vector<unsigned int>& Model::getIndices() const
{
	return indices;
}