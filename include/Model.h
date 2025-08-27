// To load and access .obj gemotry data

#pragma once

#include <vector>
#include <QString>
#include <QVector3D>

class Model
{
public:
	Model();
	bool loadFromFile(const QString& filePath);
	const std::vector<QVector3D>& getVertices() const;
	const std::vector<unsigned int>& getIndices() const;
	const std::vector<QVector3D>& getNormals() const;

private:
	std::vector<QVector3D> vertices;
	std::vector<unsigned int> indices;
	std::vector<QVector3D> normals;
};