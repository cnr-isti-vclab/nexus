/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#include "meshloader.h"
#include <QFileInfo>

void MeshLoader::quantize(float &value) {
	if(!quantization) return;
	value = quantization*(int)(value/quantization);
}

void MeshLoader::sanitizeTextureFilepath(QString &textureFilepath) {
  std::replace( textureFilepath.begin(), textureFilepath.end(), QChar('\\'), QChar('/') );
}

void MeshLoader::resolveTextureFilepath(const QString &modelFilepath, QString &textureFilepath) {
  QFileInfo file(modelFilepath);
  QString path = file.path();

  textureFilepath = path + "/" + textureFilepath;
}
