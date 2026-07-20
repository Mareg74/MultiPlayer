#pragma once

#include "map/OutputMap.h"

#include <QString>

class ResolumeXmlImport
{
public:
    // Best-effort parser for Resolume Advanced Output / screensetup XML.
    // Extracts rectangular Slice input/output regions when present.
    static bool importFile(const QString &path, OutputMap *map, QString *error = nullptr);
};
