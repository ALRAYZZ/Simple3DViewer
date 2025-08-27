// Parsing .obj file into verices and indices

#include "Model.h"
#include <QFile>
#include <QTextStream>

Model::Model() {}

bool Model::loadFromFile(const QString& filePath)
{
	vertices.clear();
	indices.clear();
	normals.clear();

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return false;
	}

	QTextStream in(&file);
	std::vector<QVector3D> tempVertices;
	std::vector<QVector3D> tempNormals;
	std::vector<unsigned int> tempIndices;

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
		else if (line.startsWith("vn "))
		{
			QStringList parts = line.split(' ', Qt::SkipEmptyParts);
			if (parts.size() >= 4)
			{
				float x = parts[1].toFloat();
				float y = parts[2].toFloat();
				float z = parts[3].toFloat();
				tempNormals.push_back(QVector3D(x, y, z));
			}
		}
		else if (line.startsWith("f ")) // .obj format to tell us that faces indices are next
		{
			QStringList parts = line.split(' ', Qt::SkipEmptyParts);
			if (parts.size() >= 4)
			{
				std::vector<int> faceIndices;
				for (int i = 1; i < parts.size(); ++i)
				{
					QStringList indexParts = parts[i].split('/');
					if (!indexParts.isEmpty())
					{
						faceIndices.push_back(indexParts[0].toUInt() - 1); // .obj indices are 1-based
					}
				}

				// Triangulate if more than 3 vertices (fan triangulation)
				for (int i = 1; i < faceIndices.size() - 1; ++i)
				{
					tempIndices.push_back(faceIndices[0]);
					tempIndices.push_back(faceIndices[i]);
					tempIndices.push_back(faceIndices[i + 1]);
				}
			}
		}
	}

	vertices = tempVertices;
	indices = tempIndices;

	// If normals are not provided, we can compute them
	if (tempNormals.empty() && !vertices.empty() && !indices.empty())
	{
		normals.resize(vertices.size(), QVector3D(0, 0, 0));

		// Calculate face normals and accumulate
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			if (i + 2 < indices.size())
			{
				QVector3D v0 = vertices[indices[i]];
				QVector3D v1 = vertices[indices[i + 1]];
				QVector3D v2 = vertices[indices[i + 2]];

				QVector3D normal = QVector3D::crossProduct(v1 - v0, v2 - v0).normalized();

				normals[indices[i]] += normal;
				normals[indices[i + 1]] += normal;
				normals[indices[i + 2]] += normal;
			}
		}

		// Normalize the accumulated normals
		for (auto& normal : normals)
		{
			normal.normalize();
		}
	}
	else
	{
		normals = tempNormals;
	}
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
const std::vector<QVector3D>& Model::getNormals() const
{
	return normals;
}